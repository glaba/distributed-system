#include "mock_sdfs_client.h"

#include <stdlib.h>

using std::string;

mock_sdfs_client::mock_sdfs_client(environment &env)
    : config(env.get<configuration>()) {}

std::unique_ptr<service_state> mock_sdfs_client::init_state() {
    // All SDFS files for an environment group will be stored in the directory of just one of the nodes
    mock_sdfs_state *state = new mock_sdfs_state();
    state->sdfs_dir = config->get_sdfs_dir();
    return std::unique_ptr<service_state>(state);
}

void mock_sdfs_client::start() {
    running = true;
}

void mock_sdfs_client::stop() {
    running = false;
}

string mock_sdfs_client::get_sdfs_dir() {
    string retval;
    access_state([&retval] (service_state *state) {
        retval = dynamic_cast<mock_sdfs_state*>(state)->sdfs_dir;
    });
    return retval;
}

void mock_sdfs_client::access_pieces(std::function<void(std::unordered_map<string, unsigned>&)> callback) {
    access_state([&callback] (service_state *state) {
        callback(dynamic_cast<mock_sdfs_state*>(state)->num_pieces);
    });
}

int mock_sdfs_client::put_operation(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    // Delete all existing pieces of the file
    string sdfs_dir = get_sdfs_dir();
    unsigned cur_num_pieces;
    access_pieces([&cur_num_pieces, &sdfs_filename] (std::unordered_map<string, unsigned> &num_pieces) {
        cur_num_pieces = num_pieces[sdfs_filename];
        num_pieces[sdfs_filename] = 1;
    });

    std::lock_guard<std::mutex> guard(file_mutex);
    for (unsigned i = 0; i < cur_num_pieces; i++) {
        FILE *stream = popen(("rm \"" + sdfs_dir + sdfs_filename + "." + std::to_string(i) + "\" 2>&1").c_str(), "r");
        assert(pclose(stream) == 0 && "mock_sdfs_client contained invalid state"); // We assume this will run without failure
    }

    FILE *stream = popen(("cp \"" + local_filename + "\" \"" + sdfs_dir + sdfs_filename + ".0\"" + " 2>&1").c_str(), "r");
    return pclose(stream) ? -1 : 0;
}

int mock_sdfs_client::get_operation(string local_filename, string sdfs_filename) {
    return -1;
}

int mock_sdfs_client::get_sharded(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();

    std::lock_guard<std::mutex> guard(file_mutex);

    string command = "cat ";
    bool fail = false;
    access_pieces([&command, &sdfs_filename, &sdfs_dir, &fail] (std::unordered_map<string, unsigned> &num_pieces) {
        if (num_pieces.find(sdfs_filename) == num_pieces.end()) {
            fail = true;
            return;
        }
        for (unsigned i = 0; i < num_pieces[sdfs_filename]; i++) {
            command += "\"" + sdfs_dir + sdfs_filename + "." + std::to_string(i) + "\" ";
        }
    });
    command += "> \"" + local_filename + "\"";

    if (fail) {
        return -1;
    }

    FILE *stream = popen((command + " 2>&1").c_str(), "r");
    // TODO: chmod only when required
    if (pclose(stream)) {
        return -1;
    }
    // TODO: handle this failure correctly
    stream = popen(("chmod +x \"" + local_filename + "\"").c_str(), "r");
    pclose(stream);
    return 0;
}

int mock_sdfs_client::del_operation(string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();

    std::lock_guard<std::mutex> guard(file_mutex);

    string command = "rm ";
    bool fail = false;
    access_pieces([&command, &sdfs_filename, &sdfs_dir, &fail] (std::unordered_map<string, unsigned> &num_pieces) {
        if (num_pieces.find(sdfs_filename) == num_pieces.end()) {
            fail = true;
            return;
        }
        for (unsigned i = 0; i < num_pieces[sdfs_filename]; i++) {
            command += "\"" + sdfs_dir + sdfs_filename + "." + std::to_string(i) + "\" ";
        }
    });

    if (fail) {
        return -1;
    }

    FILE *stream = popen((command + " 2>&1").c_str(), "r");
    return pclose(stream) ? -1 : 0;
}

int mock_sdfs_client::append_operation(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    std::lock_guard<std::mutex> guard(file_mutex);

    unsigned index;
    access_pieces([&index, &sdfs_filename] (std::unordered_map<string, unsigned> &num_pieces) {
        index = num_pieces[sdfs_filename]++;
    });

    string sdfs_dir = get_sdfs_dir();
    FILE *stream = popen(("cp \"" + local_filename + "\" \"" + sdfs_dir + sdfs_filename + "." + std::to_string(index) + "\" 2>&1").c_str(), "r");
    return pclose(stream) ? -1 : 0;
}

int mock_sdfs_client::ls_operation(string sdfs_filename) {
    return -1;
}

int mock_sdfs_client::store_operation() {
    return -1;
}

register_test_service<sdfs_client, mock_sdfs_client> register_mock_sdfs_client;
