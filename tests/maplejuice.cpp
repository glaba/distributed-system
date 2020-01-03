#include "test.h"
#include "environment.h"
#include "configuration.h"
#include "mj_worker.h"
#include "tcp.h"
#include "election.h"
#include "sdfs_client.h"
#include "maple_client.h"
#include "mock_tcp.h"
#include "heartbeater.h"
#include "mock_sdfs_client.h"
#include "partitioner.h"
#include "juice_client.h"

#include <memory>
#include <stdlib.h>
#include <string>
#include <vector>

using std::string;

namespace maplejuice_test {
    void setup_env(environment &env, logger::log_level level, std::string hostname, std::string introducer, bool is_first_node) {
        configuration *config = env.get<configuration>();
        config->set_hostname(hostname);
        config->set_first_node(is_first_node);
        config->set_hb_port(1234);
        config->set_election_port(1235);
        config->set_mj_internal_port(1236);
        config->set_mj_master_port(1237);
        config->set_dir("./dir/");
        config->set_sdfs_subdir("mock_sdfs");
        config->set_mj_subdir("mj");
        // We are going to use mock SDFS so we don't need to set SDFS ports

        env.get<logger_factory>()->configure(level);

        env.get<mj_worker>()->start();
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

    void diff_files(string f1, string f2) {
        std::string command = "diff " + f1 + " " + f2;
        FILE *stream = popen(command.c_str(), "r");
        assert(stream);
        char buffer[1];
        int bytes_read = fread(static_cast<void*>(buffer), 1, 1, stream);
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
    // First, delete exe because we don't want to include that in the diff if it's different
    std::string sdfs_dir = dynamic_cast<mock_sdfs_client*>(env.get<sdfs_client>())->get_sdfs_root_dir();
    maplejuice_test::delete_file(sdfs_dir + "wc_maple.0");
    maplejuice_test::diff_directories(sdfs_dir, "./mje/test_files/wc_ram_maple_output/");
}

testing::register_test single_node_maple("maple.single_node",
    "Tests assigning a job (word count) to a single Maple node",
    13, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::unique_ptr<environment> node_env = env_group.get_env();

    maplejuice_test::setup_env(*master_env, level, "master", "", true);
    maplejuice_test::setup_env(*node_env, level, "node", "master", false);

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input file into the SDFS
    sdfs_client *sdfsc = node_env->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_data") == 0);
    assert(sdfsc->put("./mje/test_files/wc_ram", "wc_data/ram") == 0 &&
        "Test file wc_ram is missing or put operation failed unexpectedly");

    // Create output directory
    assert(sdfsc->mkdir("intermediate") == 0);

    // Have node_env initiate the job
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *client = node_env->get<maple_client>();
    assert(client->run_job("master", "./mje/wc_maple", "wc_maple", 1, "wc_data", "intermediate"));

    check_wc_ram(*node_env);

    std::thread stop_master([&] {master_env->get<mj_worker>()->stop();});
    node_env->get<mj_worker>()->stop();
    stop_master.join();
});

testing::register_test single_node_mj("maplejuice.single_node",
    "Tests assigning a full job to a single MapleJuice node",
    26, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::unique_ptr<environment> node_env = env_group.get_env();

    maplejuice_test::setup_env(*master_env, level, "mster", "", true);
    maplejuice_test::setup_env(*node_env, level, "node", "mster", false);

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input file into the SDFS
    sdfs_client *sdfsc = node_env->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_data") == 0);
    assert(sdfsc->put("./mje/test_files/wc_ram", "wc_data/ram") == 0 &&
        "Test file wc_ram is missing or put failed");

    // Create output directories
    assert(sdfsc->mkdir("intermediate") == 0);
    assert(sdfsc->mkdir("ram_wordcount") == 0);

    // Have node_env initiate the Maple job
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *mclient = node_env->get<maple_client>();
    assert(mclient->run_job("mster", "./mje/wc_maple", "wc_maple", 1, "wc_data", "intermediate"));

    check_wc_ram(*node_env);

    // Have node_env initiate the Juice job
    juice_client *jclient = node_env->get<juice_client>();
    assert(jclient->run_job("mster", "./mje/wc_juice", "wc_juice", 1,
        partitioner::type::round_robin, "intermediate", "ram_wordcount"));

    std::thread stop_master([&] {master_env->get<mj_worker>()->stop();});
    node_env->get<mj_worker>()->stop();
    stop_master.join();
});

testing::register_test contact_not_master("maple.contact_not_master",
    "Tests assigning a job (word count) to a single Maple node with the client connecting to a node that is not the master",
    15, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::unique_ptr<environment> node_env = env_group.get_env();

    maplejuice_test::setup_env(*master_env, level, "master", "", true);
    maplejuice_test::setup_env(*node_env, level, "node", "master", false);

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input file into the SDFS
    sdfs_client *sdfsc = node_env->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_data") == 0);
    assert(sdfsc->put("./mje/test_files/wc_ram", "wc_data/ram") == 0
        && "Test file wc_ram is missing or put failed");

    // Create output directory
    assert(sdfsc->mkdir("intermediate") == 0);

    // Have node_env initiate the job and send it to node instead of master
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *client = node_env->get<maple_client>();
    assert(client->run_job("node", "./mje/wc_maple", "wc_maple", 1, "wc_data", "intermediate"));

    check_wc_ram(*node_env);

    std::thread stop_master([&] {master_env->get<mj_worker>()->stop();});
    node_env->get<mj_worker>()->stop();
    stop_master.join();
});

testing::register_test multiple_nodes("maplejuice.multiple_nodes",
    "Tests assigning a job (word count) to multiple nodes",
    130, [] (logger::log_level level)
{
    environment_group env_group(true);

    unsigned NUM_NODES = 4;

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::vector<std::unique_ptr<environment>> node_envs = env_group.get_envs(NUM_NODES);

    maplejuice_test::setup_env(*master_env, level, "mster", "", true);
    for (unsigned i = 0; i < NUM_NODES; i++) {
        maplejuice_test::setup_env(*node_envs[i], level, "node" + std::to_string(i), "mster", false);
    }

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input files into the SDFS
    sdfs_client *sdfsc = node_envs[0]->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_hitchhiker") == 0);

    std::vector<string> input_files = maplejuice_test::ls("./mje/test_files/wc_hitchhiker_full");
    for (string file : input_files) {
        assert(sdfsc->put("./mje/test_files/wc_hitchhiker_full/" + file, "wc_hitchhiker/" + file) == 0);
    }

    // Create output directories
    assert(sdfsc->mkdir("intermediate") == 0);
    assert(sdfsc->mkdir("hitchhiker_wordcount") == 0);

    // Have node_envs[0] initiate the job and send it to node instead of master
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *mclient = node_envs[0]->get<maple_client>();
    assert(mclient->run_job("mster", "./mje/wc_maple", "wc_maple", 5, "wc_hitchhiker", "intermediate"));

    juice_client *jclient = node_envs[0]->get<juice_client>();
    assert(jclient->run_job("mster", "./mje/wc_juice", "wc_juice", 5,
        partitioner::type::range, "intermediate", "hitchhiker_wordcount"));

    // Get the file and check its validity
    string temp_dir = node_envs[0]->get<configuration>()->get_mj_dir();
    assert(node_envs[0]->get<sdfs_client>()->get(temp_dir + "result", "hitchhiker_wordcount/output") == 0);
    pclose(popen(const_cast<char*>(("cat " + temp_dir + "result | sort > " + temp_dir + "results_sorted").c_str()), "r"));
    maplejuice_test::delete_file(temp_dir + "result");
    maplejuice_test::diff_files(temp_dir + "results_sorted", "./mje/test_files/wc_hitchhiker_results");
    maplejuice_test::delete_file(temp_dir + "results_sorted");

    std::vector<std::thread> stop_nodes;
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_nodes.push_back(std::thread([&, i] {node_envs[i]->get<mj_worker>()->stop();}));
    }
    master_env->get<mj_worker>()->stop();
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_nodes[i].join();
    }
});

testing::register_test drop_maple("maplejuice.drop_maple",
    "Tests assigning a job (word count) to multiple nodes, with a node failing during the Maple portion",
    150, [] (logger::log_level level)
{
    environment_group env_group(true);

    unsigned NUM_NODES = 4;

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::vector<std::unique_ptr<environment>> node_envs = env_group.get_envs(NUM_NODES);

    maplejuice_test::setup_env(*master_env, level, "mster", "", true);
    for (unsigned i = 0; i < NUM_NODES; i++) {
        maplejuice_test::setup_env(*node_envs[i], level, "node" + std::to_string(i), "mster", false);
    }

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input files into the SDFS
    sdfs_client *sdfsc = node_envs[0]->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_hitchhiker") == 0);

    std::vector<string> input_files = maplejuice_test::ls("./mje/test_files/wc_hitchhiker_full");
    for (string file : input_files) {
        assert(sdfsc->put("./mje/test_files/wc_hitchhiker_full/" + file, "wc_hitchhiker/" + file) == 0);
    }

    // Create output directories
    assert(sdfsc->mkdir("intermediate") == 0);
    assert(sdfsc->mkdir("hitchhiker_wordcount") == 0);

    std::thread drop_thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        node_envs[0]->get<mj_worker>()->stop();
    });
    drop_thread.detach();

    // Have node_envs[0] initiate the job and send it to node instead of master
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *client = node_envs[0]->get<maple_client>();
    assert(client->run_job("mster", "./mje/wc_maple", "wc_maple", 5, "wc_hitchhiker", "intermediate"));

    juice_client *jclient = node_envs[0]->get<juice_client>();
    assert(jclient->run_job("mster", "./mje/wc_juice", "wc_juice", 5,
        partitioner::type::range, "intermediate", "hitchhiker_wordcount"));

    // Get the file and check its validity
    string temp_dir = node_envs[0]->get<configuration>()->get_mj_dir();
    assert(node_envs[0]->get<sdfs_client>()->get(temp_dir + "result", "hitchhiker_wordcount/output") == 0);
    pclose(popen(const_cast<char*>(("cat " + temp_dir + "result | sort > " + temp_dir + "results_sorted").c_str()), "r"));
    maplejuice_test::delete_file(temp_dir + "result");
    maplejuice_test::diff_files(temp_dir + "results_sorted", "./mje/test_files/wc_hitchhiker_results");
    maplejuice_test::delete_file(temp_dir + "results_sorted");

    std::vector<std::thread> stop_nodes;
    for (unsigned i = 0; i < NUM_NODES; i++) {
        if (i == 0) {
            stop_nodes.push_back(std::thread([&] {master_env->get<mj_worker>()->stop();}));
        } else {
            stop_nodes.push_back(std::thread([&, i] {node_envs[i]->get<mj_worker>()->stop();}));
        }
    }
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_nodes[i].join();
    }
});

testing::register_test drop_juice("maplejuice.drop_juice",
    "Tests assigning a job (word count) to multiple nodes, with a node failing during the Juice portion",
    150, [] (logger::log_level level)
{
    environment_group env_group(true);

    unsigned NUM_NODES = 4;

    std::unique_ptr<environment> master_env = env_group.get_env();
    std::vector<std::unique_ptr<environment>> node_envs = env_group.get_envs(NUM_NODES);

    maplejuice_test::setup_env(*master_env, level, "mster", "", true);
    for (unsigned i = 0; i < NUM_NODES; i++) {
        maplejuice_test::setup_env(*node_envs[i], level, "node" + std::to_string(i), "mster", false);
    }

    // Wait for all services to get set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Put the input files into the SDFS
    sdfs_client *sdfsc = node_envs[0]->get<sdfs_client>();
    assert(sdfsc->mkdir("wc_hitchhiker") == 0);

    std::vector<string> input_files = maplejuice_test::ls("./mje/test_files/wc_hitchhiker_full");
    for (string file : input_files) {
        assert(sdfsc->put("./mje/test_files/wc_hitchhiker_full/" + file, "wc_hitchhiker/" + file) == 0);
    }

    // Create output directories
    assert(sdfsc->mkdir("intermediate") == 0);
    assert(sdfsc->mkdir("hitchhiker_wordcount") == 0);

    // Have node_envs[0] initiate the job and send it to node instead of master
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait for master to be ready (TODO: remove this after standardizing start/stop)
    maple_client *client = node_envs[0]->get<maple_client>();
    assert(client->run_job("mster", "./mje/wc_maple", "wc_maple", 5, "wc_hitchhiker", "intermediate"));

    std::thread drop_thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        node_envs[0]->get<mj_worker>()->stop();
    });
    drop_thread.detach();

    juice_client *jclient = node_envs[0]->get<juice_client>();
    assert(jclient->run_job("mster", "./mje/wc_juice", "wc_juice", 5,
        partitioner::type::range, "intermediate", "hitchhiker_wordcount"));

    // Get the file and check its validity
    string temp_dir = node_envs[1]->get<configuration>()->get_mj_dir();
    assert(node_envs[1]->get<sdfs_client>()->get(temp_dir + "result", "hitchhiker_wordcount/output") == 0);
    pclose(popen(const_cast<char*>(("cat " + temp_dir + "result | sort > " + temp_dir + "results_sorted").c_str()), "r"));
    maplejuice_test::delete_file(temp_dir + "result");
    maplejuice_test::diff_files(temp_dir + "results_sorted", "./mje/test_files/wc_hitchhiker_results");
    maplejuice_test::delete_file(temp_dir + "results_sorted");

    std::vector<std::thread> stop_nodes;
    for (unsigned i = 0; i < NUM_NODES; i++) {
        if (i == 0) {
            stop_nodes.push_back(std::thread([&] {master_env->get<mj_worker>()->stop();}));
        } else {
            stop_nodes.push_back(std::thread([&, i] {node_envs[i]->get<mj_worker>()->stop();}));
        }
    }
    for (unsigned i = 0; i < NUM_NODES; i++) {
        stop_nodes[i].join();
    }
});
