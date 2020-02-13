#pragma once

#include "serialization.h"
#include "locking.h"
#include "utils.h"

#include <string>
#include <queue>
#include <functional>
#include <memory>
#include <condition_variable>

class conversation {
public:
    virtual ~conversation() {}

    template <typename T>
    void send(T const& msg) {
        send_raw(T::get_serializable_id(), msg.serialize());
    }

    template <typename... Callbacks>
    void receive(Callbacks... c) {
        auto [msg, serializable_id] = receive_raw();

        // Call the appropriate callback using dispatch
        dispatch(serializable_id, msg, c...);
    }

    virtual void send_raw(uint32_t serializable_id, std::string const& msg) = 0;
    virtual auto receive_raw() -> std::tuple<std::string, uint32_t> = 0;

private:
    // Calls the callback that accepts the serializable of type corresponding to serializable_id
    template <typename Callback, typename... Rest>
    inline void dispatch(uint32_t serializable_id, std::string const& msg, Callback c, Rest... rest) {
        using callback_msg_type = typename utils::templates::get_arg<Callback>::type;
        using msg_type = typename std::decay<callback_msg_type>::type;

        if (msg_type::get_serializable_id() == serializable_id) {
            msg_type msg_serializable;
            if (!msg_serializable.deserialize_from(msg)) {
                throw "Received invalid message! Message contents do not match serializable ID";
            }
            c(msg_serializable);
        } else {
            dispatch(serializable_id, msg, rest...);
        }
    }

    inline void dispatch(uint32_t serializable_id, std::string const& msg) {
        throw "None of the provided callbacks match the received message type!";
    }
};

class messaging {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void register_conversation_handler(uint32_t serializable_id, std::function<void(conversation&)> handler) = 0;
    template <typename T>
    auto start_conversation(std::string host, T msg) -> std::unique_ptr<conversation> {
        return start_conversation(host, T::get_serializable_id(), msg.serialize());
    }

protected:
    virtual auto start_conversation(std::string host, uint32_t serializable_id, std::string msg) -> std::unique_ptr<conversation> = 0;
};
