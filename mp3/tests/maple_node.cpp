#include "test.h"
#include "environment.h"
#include "configuration.h"
#include "maple_node.h"
#include "tcp.h"
#include "election.h"
#include "sdfs_client.h"
#include "maple_client.h"
#include "mock_tcp.h"
#include "heartbeater.h"
#include "mock_sdfs_client.h"

#include <memory>
#include <stdlib.h>
#include <string>
#include <vector>

using std::string;

namespace maple_node_test {
    void setup_env(environment &env, logger::log_level level, std::string hostname, std::string introducer, bool is_first_node) {
        configuration *config = env.get<configuration>();
        config->set_hostname(hostname);
        config->set_first_node(is_first_node);
        config->set_hb_port(1234);
        config->set_election_port(1235);
        config->set_maple_internal_port(1236);
        config->set_maple_master_port(1237);
        config->set_dir("./dir/");
        config->set_sdfs_subdir("mock_sdfs");
        config->set_maple_subdir("maple");
        // We are going to use mock SDFS so we don't need to set SDFS ports

        env.get<logger_factory>()->configure(level);

        env.get<maple_node>()->start();
        if (!is_first_node) {
            env.get<heartbeater>()->join_group(introducer);
        }
    }

    std::vector<string> ls(string directory) {
        std::vector<string> retval;
        FILE *stream = popen(("ls \"" + directory + "\"").c_str(), "r");
        assert(stream);
        char buffer[17];
        while (true) {
            int bytes_read = fread(static_cast<void*>(buffer), 1, 17, stream);
            if (bytes_read == 0) {
                break;
            }
            retval.push_back(string(buffer, 16));
        }
        return retval;
    }

    void diff_directories(string first, string second) {
        std::string command = "/bin/bash -c 'diff <(find " + first + " -type f -exec md5sum {} + | sort -k 2 | cut -f1 -d\" \") "
            "<(find " + second + " -type f -exec md5sum {} + | sort -k 2 | cut -f1 -d\" \")'";
        FILE *stream = popen((command + " 2>&1").c_str(), "r");
        assert(stream);
        char buffer[1024];
        int bytes_read = fread(static_cast<void*>(buffer), 1, 1024, stream);
        assert(bytes_read == 0 && feof(stream));
        pclose(stream);
    }

    void delete_file(string filename) {
        FILE *stream = popen(("rm \"" + filename + "\"").c_str(), "r"); pclose(stream);
        assert(stream);
    }
};

void check_wc_ram(environment &env) {
    // Check that all the files are there
    // First, delete maple_exe because we don't want to include that in the diff if it's different
    std::string sdfs_dir = dynamic_cast<mock_sdfs_client*>(env.get<sdfs_client>())->get_sdfs_dir();
    maple_node_test::delete_file(sdfs_dir + "wc_maple.0");
    maple_node_test::diff_directories(sdfs_dir, "./mje/test_files/wc_ram_maple_output/");
}

testing::register_test single_node("maple_node.single_node",
    "Tests assigning a job (word count) to a single Maple node",
    13, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::unique_ptr<environment> node_env = env_group.get_env();

    maple_node_test::setup_env(*master_env, level, "master", "", true);
    maple_node_test::setup_env(*node_env, level, "node", "master", false);

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input file into the SDFS
    if (node_env->get<sdfs_client>()->put_operation("./mje/test_files/wc_ram", "wc_ram") != 0)
        assert(false && "Test file wc_ram is missing");

    // Have node_env initiate the job
    maple_client *client = node_env->get<maple_client>();
    assert(client->run_job("master", "./mje/wc_maple", "wc_maple", 1, "wc_ram_intermediate", "wc_ram"));

    check_wc_ram(*node_env);

    std::thread stop_master([&] {master_env->get<maple_node>()->stop();}); stop_master.detach();
    node_env->get<maple_node>()->stop();
});

testing::register_test contact_not_master("maple_node.contact_not_master",
    "Tests assigning a job (word count) to a single Maple node with the client connecting to a node that is not the master",
    15, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::unique_ptr<environment> node_env = env_group.get_env();

    maple_node_test::setup_env(*master_env, level, "master", "", true);
    maple_node_test::setup_env(*node_env, level, "node", "master", false);

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input file into the SDFS
    if (node_env->get<sdfs_client>()->put_operation("./mje/test_files/wc_hitchhiker", "wc_hitchhiker") != 0)
        assert(false && "Test file wc_ram is missing");

    // Have node_env initiate the job and send it to node instead of master
    maple_client *client = node_env->get<maple_client>();
    assert(client->run_job("node", "./mje/wc_maple", "wc_maple", 1, "wc_hitchhiker_intermediate", "wc_hitchhiker"));

    // check_wc_ram(*node_env);

    std::thread stop_master([&] {master_env->get<maple_node>()->stop();}); stop_master.detach();
    node_env->get<maple_node>()->stop();
});

testing::register_test multiple_nodes("maple_node.multiple_nodes",
    "Tests assigning a job (word count) to multiple Maple nodes",
    35, [] (logger::log_level level)
{
    environment_group env_group(true);

    unsigned NUM_NODES = 4;

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::vector<std::unique_ptr<environment>> node_envs = env_group.get_envs(NUM_NODES);

    maple_node_test::setup_env(*master_env, level, "master", "", true);
    for (unsigned i = 0; i < NUM_NODES; i++) {
        maple_node_test::setup_env(*node_envs[i], level, "node" + std::to_string(i), "master", false);
    }

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input files into the SDFS
    std::vector<string> input_files = maple_node_test::ls("./mje/test_files/wc_hitchhiker");
    for (string file : input_files) {
        node_envs[0]->get<sdfs_client>()->put_operation("./mje/test_files/wc_hitchhiker/" + file, file);
    }

    // Have node_envs[0] initiate the job and send it to node instead of master
    maple_client *client = node_envs[0]->get<maple_client>();
    assert(client->run_job("master", "./mje/wc_maple", "wc_maple", 5, "wc_hitchhiker_intermediate", "wc_hitchhiker"));

    // check_wc_ram(*node_env);

    for (unsigned i = 0; i < NUM_NODES; i++) {
        std::thread stop_node([&, i] {node_envs[i]->get<maple_node>()->stop();}); stop_node.detach();
    }
    master_env->get<maple_node>()->stop();
});

testing::register_test drop_nodes("maple_node.drop_nodes",
    "Tests assigning a job (word count) to multiple Maple nodes",
    55, [] (logger::log_level level)
{
    environment_group env_group(true);

    unsigned NUM_NODES = 4;

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::vector<std::unique_ptr<environment>> node_envs = env_group.get_envs(NUM_NODES);

    maple_node_test::setup_env(*master_env, level, "master", "", true);
    for (unsigned i = 0; i < NUM_NODES; i++) {
        maple_node_test::setup_env(*node_envs[i], level, "node" + std::to_string(i), "master", false);
    }

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input files into the SDFS
    std::vector<string> input_files = maple_node_test::ls("./mje/test_files/wc_hitchhiker");
    for (string file : input_files) {
        node_envs[0]->get<sdfs_client>()->put_operation("./mje/test_files/wc_hitchhiker/" + file, file);
    }

    std::thread drop_thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        node_envs[0]->get<maple_node>()->stop();
    });
    drop_thread.detach();

    // Have node_envs[0] initiate the job and send it to node instead of master
    maple_client *client = node_envs[0]->get<maple_client>();
    assert(client->run_job("master", "./mje/wc_maple", "wc_maple", 5, "wc_hitchhiker_intermediate", "wc_hitchhiker"));

    // check_wc_ram(*node_env);

    for (unsigned i = 0; i < NUM_NODES; i++) {
        std::thread stop_node([&, i] {node_envs[i]->get<maple_node>()->stop();}); stop_node.detach();
    }
    master_env->get<maple_node>()->stop();
});
