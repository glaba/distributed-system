#include "test.h"
#include "environment.h"
#include "configuration.h"
#include "heartbeater.h"
#include "election.h"
#include "messaging.h"
#include "serialization.h"
#include "mock_tcp.h"

#include <memory>

using std::unique_ptr;
using std::make_unique;

class conv1_A : public serializable<conv1_A> {
public:
    conv1_A() {}
    conv1_A(int val) : val(val) {}
    uint32_t val;
};

class conv1_B : public serializable<conv1_B> {
public:
    conv1_B() {}
    conv1_B(uint32_t val) : val(val) {}
    uint32_t val;
};

class conv1_C : public serializable<conv1_C> {
public:
    conv1_C() {}
    conv1_C(uint32_t val) : val(val) {}
    uint32_t val;
};

register_serializable<conv1_A> register_conv1_A([] (auto &registry) {
    registry.register_fields(&conv1_A::val);
});
register_serializable<conv1_B> register_conv1_B([] (auto &registry) {
    registry.register_fields(&conv1_B::val);
});
register_serializable<conv1_C> register_conv1_C([] (auto &registry) {
    registry.register_fields(&conv1_C::val);
});

testing::register_test messaging_test("messaging",
    "Tests messaging",
    35, [] (logger::log_level level)
{
    const int NUM_NODES = 10;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> envs = env_group.get_envs(NUM_NODES);

    // Set the hostnames and heartbeater / election parameters for each of the environments
    for (int i = 0; i < 10; i++) {
        configuration *config = envs[i]->get<configuration>();
        config->set_hostname("h" + std::to_string(i));
        config->set_hb_port(1234);
        config->set_first_node(i == 0);

        envs[i]->get<logger_factory>()->configure(level);
    }

    std::vector<heartbeater*> hbs;
    std::vector<messaging*> msgs;
    for (int i = 0; i < NUM_NODES; i++) {
        // dynamic_cast<mock_tcp_factory*>(envs[i]->get<tcp_factory>())->show_packets();
        hbs.push_back(envs[i]->get<heartbeater>());
        msgs.push_back(envs[i]->get<messaging>());
        msgs[i]->start();
        hbs[i]->join_group("h0");

    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    msgs[0]->register_conversation_handler(conv1_A::get_serializable_id(), [&] (conversation &conv) {
        conv.receive([&] (conv1_A const& msg) {
            std::cout << "Received value " << msg.val << std::endl;
        });
    });

    msgs[1]->start_conversation("h0", conv1_A(5));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Stop all nodes
    std::vector<std::thread> stop_threads;
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_threads.push_back(std::thread([&msgs, i] {
            msgs[i]->stop();
        }));
    }
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_threads[i].join();
    }
});
