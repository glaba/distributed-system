#include "messaging.hpp"
#include "utils.h"

#include <algorithm>

#define PORT 2000

using std::string;
using std::optional;
using std::function;
using std::unordered_map;
using std::unordered_set;
using std::unique_ptr;
using std::make_unique;
using std::tuple;

auto conversation_impl::receive_raw() -> tuple<string, uint32_t> {
    std::string msg;
    {
        unlocked<std::queue<std::string>> msg_queue = msg_queue_lock();
        cv_msg.wait(msg_queue.unsafe_get_mutex(), [&] {
            return msg_queue->size() > 0;
        });

        if (msg_queue->size() > 0) {
            msg = std::move(msg_queue->front());
            msg_queue->pop();
        }
    }

    // Get the serialization ID of the message
    serializable_id_container container;
    if (!container.deserialize_from(msg, true)) {
        throw "Received invalid message! Does not contain serializable ID";
    }

    return {msg.substr(sizeof(uint32_t)), container.serializable_id};
}

void conversation_impl::send_raw(uint32_t serializable_id, std::string const& msg) {
    send_fn(serialization::write_uint32_to_string(serializable_id) + msg);
}

messaging_impl::messaging_impl(environment &env)
    : tcp_fac(env.get<tcp_factory>())
    , config(env.get<configuration>())
    , hb(env.get<heartbeater>())
    , lg(env.get<logger_factory>()->get_logger("messaging")) {}

void messaging_impl::start() {
    if (msg_state_lock()->running) {
        return;
    }
    msg_state_lock()->running = true;
    hb->start();

    server = tcp_fac->get_tcp_server(PORT);
    std::thread client_thread([&] {client_thread_fn();});
    std::thread server_thread([&] {server_thread_fn();});

    function<void()> const& temp = [this, client_thread = std::move(client_thread), server_thread = std::move(server_thread)] () mutable {
            server->stop_server();
            client_thread.join();
            server_thread.join();
        };

    cleanup_callbacks()->push_back(std::move(temp));

    function join_callback = [&] (member const& m) {
        unlocked<messaging_state> msg_state = msg_state_lock();
        msg_state->new_members.insert(m);
        cv_state_change.notify_all();
    };

    hb->on_join(join_callback);

    function leave_callback = [&] (member const& m) {
        unlocked<messaging_state> msg_state = msg_state_lock();
        if (msg_state->members.find(m) != msg_state->members.end()) {
            msg_state->members.erase(m);
        }
        if (msg_state->new_members.find(m) != msg_state->new_members.end()) {
            msg_state->new_members.erase(m);
        }
        cv_state_change.notify_all();
    };

    hb->on_leave(leave_callback);
    hb->on_fail(leave_callback);

    unlocked<messaging_state> msg_state = msg_state_lock();
    for (member const& m : hb->get_members()) {
        msg_state->new_members.insert(m);
    }
    cv_state_change.notify_all();
}

void messaging_impl::stop() {
    if (!msg_state_lock()->running) {
        return;
    }

    msg_state_lock()->running = false;
    cv_state_change.notify_all();

    auto cleanup_callbacks_unlocked = cleanup_callbacks();
    for (auto it = cleanup_callbacks_unlocked->rbegin(); it != cleanup_callbacks_unlocked->rend(); it++) {
        (*it)();
    }

    hb->stop();
}

auto messaging_impl::start_conversation(string host, uint32_t serializable_id, string initial_msg) -> unique_ptr<conversation> {
    // Get the ID corresponding to this host
    uint32_t id = 0;
    for (auto const& m : hb->get_members()) {
        if (m.hostname == host) {
            id = m.id;
        }
    }

    if (id == 0) {
        return unique_ptr<conversation>(nullptr);
    }

    // Generate a conversation ID and a lambda that will send messages for this conversation
    uint32_t conv_id = (*mt())();
    function<void(string)> send_fn = [=] (string msg) {
        // Backoff while the message fails to send
        utils::backoff([&] {
            // Get the tcp_client object corresponding to the connection
            tcp_client *client;
            {
                unlocked<messaging_state> msg_state = msg_state_lock();
                if (msg_state->clients.find(id) == msg_state->clients.end()) {
                    return false;
                }
                client = msg_state->clients[id].get();
            }
            return client->write_to_server(serialization::write_uint32_to_string(conv_id) + msg) > 0;
        }, [&] {
            // Give up if the server has gone down
            unlocked<messaging_state> msg_state = msg_state_lock();
            return msg_state->clients.find(id) == msg_state->clients.end();
        });
    };

    // Create the conversation and send the initial message
    unique_ptr<conversation> conv = unique_ptr<conversation>(static_cast<conversation*>(new conversation_impl(send_fn)));
    conv->send_raw(serializable_id, initial_msg);
    return conv;
}

void messaging_impl::client_thread_fn() {
    unordered_set<member, member_hash> new_members;

    while (true) {
        {
            unlocked<messaging_state> msg_state = msg_state_lock();
            cv_state_change.wait(msg_state.unsafe_get_mutex(), [&] {
                return msg_state->new_members.size() > 0 || !msg_state->running;
            });
            // TODO: handle failure here

            if (!msg_state->running) {
                break;
            }

            new_members = msg_state->new_members;
            msg_state->new_members.clear();
        }

        for (member const& m : new_members) {
            lg->debug("Initiating connection with new member at " + m.hostname);
            unique_ptr<tcp_client> client = tcp_fac->get_tcp_client(m.hostname, PORT);
            client->write_to_server(config->get_hostname());
            client->write_to_server(serialization::write_uint32_to_string(hb->get_id()));
            msg_state_lock()->clients[m.id] = std::move(client);
        }
    }

    // Close all client TCP connections
    msg_state_lock()->clients.clear();
}

void messaging_impl::handle_message(unordered_map<uint32_t, conversation_impl> &conversation_map,
    uint32_t node_id, msg_data data, string const& msg)
{
    // Create a new conversation if we have not seen this conversation ID before
    if (conversation_map.find(data.conversation_id) == conversation_map.end()) {
        // Check that there is a handler to handle this message, ignore it otherwise
        function<void(conversation&)> handler;
        {
            auto handlers = handlers_lock();
            if (handlers->find(data.serializable_id) == handlers->end()) {
                return;
            }
            handler = (*handlers)[data.serializable_id];
        }

        // Create the conversation, providing a send function that prepends the conversation ID
        conversation_map.try_emplace(data.conversation_id, [=] (string const& msg) {
            // Backoff while the message fails to send
            utils::backoff([&] {
                // Get the fd corresponding to this connection
                int fd;
                {
                    unlocked<messaging_state> msg_state = msg_state_lock();
                    if (msg_state->server_fds.find(node_id) == msg_state->server_fds.end()) {
                        return false;
                    }
                    fd = msg_state->server_fds[node_id];
                }
                string id_str = serialization::write_uint32_to_string(data.conversation_id);
                return server->write_to_client(fd, id_str + msg) > 0;
            }, [&] {
                // Give up if the client has gone down
                unlocked<messaging_state> msg_state = msg_state_lock();
                return msg_state->clients.find(node_id) == msg_state->clients.end();
            });
        });

        // Start the handler for this conversation type
        // TODO: manage the lifetime of this thread correctly so that conversation_map stays alive
        std::thread conversation_thread([=, &conversation_map] {
            handler(conversation_map.find(data.conversation_id)->second);
        });

        cleanup_callbacks()->emplace_back([conversation_thread = std::move(conversation_thread)] () mutable {
            conversation_thread.join();
        });
    }

    // If the conversation was successfully created or already existed
    if (auto it = conversation_map.find(data.conversation_id); it != conversation_map.end()) {
        // Push this message to the conversation's message queue
        it->second.msg_queue_lock()->push(msg);
        it->second.cv_msg.notify_all();
    }
}

void messaging_impl::server_thread_fn() {
    while (msg_state_lock()->running) {
        int fd = server->accept_connection();
        if (fd < 0) {
            break;
        }

        std::thread server_thread([=] {
            string hostname = server->read_from_client(fd);
            if (hostname.length() == 0) {
                return;
            }
            string id_str = server->read_from_client(fd);
            if (id_str.length() == 0) {
                return;
            }
            uint32_t node_id = serialization::read_uint32_from_char_buf(id_str.c_str());

            // TODO: make sure all messages are read before doing this
            { // Close connection with old fd if there was one open, and note down the new fd
                unlocked<messaging_state> msg_state = msg_state_lock();
                if (msg_state->server_fds.find(node_id) != msg_state->server_fds.end()) {
                    server->close_connection(msg_state->server_fds[node_id]);
                }
                msg_state->server_fds[node_id] = fd;
            }

            // Process all incoming messages for any conversations with this node
            // Continue only if the service is running and if the node is still considered up by heartbeater
            unordered_map<uint32_t, conversation_impl> conversation_map;
            while (msg_state_lock()->running && (hb->get_members().size() == 0 || hb->get_member_by_id(node_id).id != 0)) {
                // Get the conversation ID from the message
                int cur_fd = msg_state_lock()->server_fds[node_id];
                string msg = server->read_from_client(cur_fd);
                msg_data data;
                if (!data.deserialize_from(msg, true)) {
                    return;
                }

                handle_message(conversation_map, node_id, data, msg.substr(sizeof(uint32_t)));
            }
        });

        cleanup_callbacks()->emplace_back([this, fd, server_thread = std::move(server_thread)] () mutable {
            server->close_connection(fd);
            server_thread.join();
        });
    }
}

register_auto<messaging, messaging_impl> register_messaging;
register_serializable<conversation_impl::serializable_id_container> register_serializable_id_container([] (auto &registry) {
    registry.register_fields(&conversation_impl::serializable_id_container::serializable_id);
});
register_serializable<messaging_impl::msg_data> register_msg_data([] (auto &registry) {
    registry.register_fields(&messaging_impl::msg_data::conversation_id, &messaging_impl::msg_data::serializable_id);
});
