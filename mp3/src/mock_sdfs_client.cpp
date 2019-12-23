#include "mock_sdfs_client.h"
#include "utils.h"

#include <filesystem>
#include <fstream>

using std::string;

mock_sdfs_client::mock_sdfs_client(environment &env)
    : config(env.get<configuration>())
    , mt(std::chrono::system_clock::now().time_since_epoch().count()) {}

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

void mock_sdfs_client::access_pieces(std::function<void(std::unordered_map<std::string, unsigned>&,
    std::unordered_map<std::string, std::unique_ptr<std::mutex>>&)> callback)
{
    access_state([&callback] (service_state *state) {
        callback(dynamic_cast<mock_sdfs_state*>(state)->num_pieces,
                 dynamic_cast<mock_sdfs_state*>(state)->file_mutexes);
    });
}

int mock_sdfs_client::put_helper(std::string sdfs_filename, std::function<bool()> callback) {
    // Atomically update the counter and lock the mutex for the file while deleting / renaming
    string sdfs_dir = get_sdfs_dir();
    unsigned cur_num_pieces;
    std::mutex *file_mutex;
    bool fail = false;
    access_pieces([&] (std::unordered_map<string, unsigned> &num_pieces,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>& file_mutexes)
    {
        if (num_pieces.find(sdfs_filename) != num_pieces.end() && num_pieces[sdfs_filename] == 0) {
            fail = true;
            return;
        }
        cur_num_pieces = num_pieces[sdfs_filename];
        num_pieces[sdfs_filename] = 1;
        if (!file_mutexes[sdfs_filename]) {
            file_mutexes[sdfs_filename] = std::make_unique<std::mutex>();
        }
        file_mutex = file_mutexes[sdfs_filename].get();
        file_mutex->lock();
    });

    if (fail) {
        return -1;
    }

    // Delete all existing pieces of the file
    for (unsigned i = 0; i < cur_num_pieces; i++) {
        assert(std::filesystem::remove(sdfs_dir + sdfs_filename + "." + std::to_string(i)) &&
            "mock_sdfs_client contained invalid state");
    }

    utils::backoff([&] {
        return callback();
    });

    file_mutex->unlock();
    return 0;
}

int mock_sdfs_client::put_operation(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    // Copy the file into a temporary location
    string sdfs_dir = get_sdfs_dir();
    string temp_location = sdfs_dir + sdfs_filename + ".~" + std::to_string(mt());
    std::error_code ec;
    if (!std::filesystem::copy_file(local_filename, temp_location, std::filesystem::copy_options::overwrite_existing, ec)) {
        return -1;
    }

    return put_helper(sdfs_filename, [&] {
        // Rename the temp file to the correct filename
        try {
            std::filesystem::rename(temp_location, sdfs_dir + sdfs_filename + ".0");
        } catch (...) {
            return false;
        }
        return true;
    });
}

int mock_sdfs_client::put_operation(inputter<string> in, std::string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();
    return put_helper(sdfs_filename, [&] {
        std::ofstream dest(sdfs_dir + sdfs_filename + ".0", std::ios::binary);
        if (!dest) {
            return false;
        }
        for (string str : in) {
            dest << str;
        }
        dest.close();
        return dest.good();
    });
}

int mock_sdfs_client::get_operation(string local_filename, string sdfs_filename) {
    return -1;
}

int mock_sdfs_client::get_sharded(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();

    bool fail = false;
    unsigned pieces;
    std::mutex *file_mutex;
    access_pieces([&] (std::unordered_map<string, unsigned> &num_pieces,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>& file_mutexes)
    {
        if (num_pieces.find(sdfs_filename) == num_pieces.end() || num_pieces[sdfs_filename] == 0) {
            fail = true;
            return;
        }
        pieces = num_pieces[sdfs_filename];
        file_mutex = file_mutexes[sdfs_filename].get();
        file_mutex->lock();
    });

    if (fail) {
        return -1;
    }

    std::ofstream dest(local_filename, std::ios::binary);
    if (!dest) {
        file_mutex->unlock();
        return -1;
    }
    for (unsigned i = 0; i < pieces; i++) {
        std::ifstream src(sdfs_dir + sdfs_filename + "." + std::to_string(i), std::ios::binary);
        dest << src.rdbuf();
    }
    dest.close();
    file_mutex->unlock();

    return dest.good() ? 0 : -1;
}

int mock_sdfs_client::del_operation(string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();

    unsigned pieces;
    std::mutex *file_mutex;
    bool fail = false;
    access_pieces([&] (std::unordered_map<string, unsigned> &num_pieces,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>& file_mutexes)
    {
        if (num_pieces.find(sdfs_filename) == num_pieces.end() || num_pieces[sdfs_filename] == 0) {
            fail = true;
            return;
        }
        num_pieces[sdfs_filename] = 0;

        pieces = num_pieces[sdfs_filename];
        file_mutex = file_mutexes[sdfs_filename].get();
        file_mutex->lock();
    });

    if (fail) {
        return -1;
    }

    for (unsigned i = 0; i < pieces; i++) {
        assert(std::filesystem::remove(sdfs_dir + sdfs_filename + "." + std::to_string(i)) &&
            "mock_sdfs_client contained invalid state");
    }
    file_mutex->unlock();

    access_pieces([&] (std::unordered_map<string, unsigned> &num_pieces,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>& file_mutexes)
    {
        num_pieces.erase(sdfs_filename);
        file_mutexes.erase(sdfs_filename);
    });
    return 0;
}

int mock_sdfs_client::append_helper(std::string sdfs_filename, std::function<bool(unsigned)> callback) {
    string sdfs_dir = get_sdfs_dir();
    unsigned index;
    std::mutex *file_mutex;
    bool fail = false;
    access_pieces([&] (std::unordered_map<string, unsigned> &num_pieces,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>& file_mutexes)
    {
        if (num_pieces.find(sdfs_filename) != num_pieces.end() && num_pieces[sdfs_filename] == 0) {
            fail = true;
            return;
        }
        index = num_pieces[sdfs_filename]++;
        if (!file_mutexes[sdfs_filename]) {
            file_mutexes[sdfs_filename] = std::make_unique<std::mutex>();
        }
        file_mutex = file_mutexes[sdfs_filename].get();
        file_mutex->lock();
    });

    if (fail) {
        return -1;
    }

    utils::backoff([&] {
        return callback(index);
    });

    file_mutex->unlock();
    return 0;
}

int mock_sdfs_client::append_operation(string local_filename, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();
    string temp_location = sdfs_dir + sdfs_filename + ".~" + std::to_string(mt());
    std::error_code ec;
    if (!std::filesystem::copy_file(local_filename, temp_location, std::filesystem::copy_options::overwrite_existing, ec)) {
        return -1;
    }

    return append_helper(sdfs_filename, [&] (unsigned index) {
        try {
            std::filesystem::rename(temp_location, sdfs_dir + sdfs_filename + "." + std::to_string(index));
        } catch (...) {
            return false;
        }
        return true;
    });
}

int mock_sdfs_client::append_operation(inputter<string> in, string sdfs_filename) {
    if (!running.load() || isolated) {
        return -1;
    }

    string sdfs_dir = get_sdfs_dir();
    return append_helper(sdfs_filename, [&] (unsigned index) {
        std::ofstream dest(sdfs_dir + sdfs_filename + "." + std::to_string(index), std::ios::binary);
        if (!dest) {
            return false;
        }
        for (string str : in) {
            dest << str;
        }
        dest.close();
        return dest.good();
    });
}

int mock_sdfs_client::ls_operation(string sdfs_filename) {
    return -1;
}

int mock_sdfs_client::store_operation() {
    return -1;
}

int mock_sdfs_client::get_index_operation(std::string sdfs_filename) {
    return -1;
}

register_test_service<sdfs_client, mock_sdfs_client> register_mock_sdfs_client;
