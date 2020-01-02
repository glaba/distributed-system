#include "mock_sdfs_client.h"
#include "utils.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

using std::string;
using std::unique_ptr;
using namespace sdfs;

mock_sdfs_client::mock_sdfs_client(environment &env)
    : config(env.get<configuration>())
    , lg(env.get<logger_factory>()->get_logger("mock_sdfs_client"))
    , el(env.get<election>())
    , mt(std::chrono::system_clock::now().time_since_epoch().count()) {}

unique_ptr<service_state> mock_sdfs_client::init_state() {
    // All SDFS files for an environment group will be stored in the directory of just one of the nodes
    mock_sdfs_state *state = new mock_sdfs_state();
    state->sdfs_root_dir = config->get_sdfs_dir();
    return unique_ptr<service_state>(state);
}

void mock_sdfs_client::start() {}

void mock_sdfs_client::stop() {}

bool operator==(const internal_path &p1, const internal_path &p2) {
    return p1.path == p2.path;
}

string mock_sdfs_client::get_sdfs_root_dir() {
    string retval;
    access_state([&retval] (service_state *state) {
        retval = dynamic_cast<mock_sdfs_state*>(state)->sdfs_root_dir;
    });
    return retval;
}

void mock_sdfs_client::get_children(internal_path dir, dir_state_map &dir_states,
    std::vector<internal_path> &subdirs, std::vector<internal_path> &subfiles)
{
    for (const string &subfile : dir_states[dir].files) {
        subfiles.push_back(sdfs::convert_path(sdfs::deconvert_path(dir) + "/" + subfile));
    }

    for (const string &subdir : dir_states[dir].dirs) {
        internal_path int_subdir = sdfs::convert_path(sdfs::deconvert_path(dir) + "/" + subdir);
        subdirs.push_back(int_subdir);
        get_children(int_subdir, dir_states, subdirs, subfiles);
    }
}

void mock_sdfs_client::access_pieces(std::function<void(dir_state_map&, file_state_map&, master_callback_type const&)> callback)
{
    access_state([&] (service_state *state) {
        callback(dynamic_cast<mock_sdfs_state*>(state)->dir_states,
                 dynamic_cast<mock_sdfs_state*>(state)->file_states,
                 dynamic_cast<mock_sdfs_state*>(state)->master_callback);
    });
}

void mock_sdfs_client::on_event(const std::function<void(op_type, file_state_map::iterator)> &callback) {
    access_state([&] (service_state *state) {
        dynamic_cast<mock_sdfs_state*>(state)->master_callback = callback;
    });
}

int mock_sdfs_client::access_helper(const string &sdfs_path, std::function<bool(file_state_map::iterator, master_callback_type const&)> callback, op_type type) {
    string filename = sdfs::get_name(sdfs_path);

    internal_path int_dir = sdfs::convert_path(sdfs::get_dir(sdfs_path));
    internal_path int_path = sdfs::convert_path(sdfs_path);

    // Atomically update the counter and lock the mutex for the file while deleting / renaming
    string sdfs_root_dir = get_sdfs_root_dir();
    file_state_map::iterator it;
    master_callback_type m_callback;
    unsigned num_existing_pieces;
    bool failed = false;
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (int_dir.path != "" && (dir_states.find(int_dir) == dir_states.end() || dir_states[int_dir].being_deleted)) {
            failed = true; // Either the directory doesn't exist or it's being deleted
            return;
        }

        if (file_states.find(int_path) != file_states.end() && file_states[int_path].being_deleted) {
            failed = true; // Avoid accessing a file while it's being deleted
            return;
        }

        // For a get or a del, ensure that the file exists
        if (type == op_get || type == op_del) {
            if (file_states.find(int_path) == file_states.end()) {
                failed = true;
                return;
            }
        }

        // Update all the relevant data structures
        dir_states[int_dir].files.insert(filename);

        if (file_states.find(int_path) == file_states.end()) {
            file_states[int_path];
        }
        it = file_states.find(int_path);
        m_callback = master_callback;

        if (!it->second.file_mutex) {
            it->second.file_mutex = std::make_unique<std::recursive_mutex>();
        }
        it->second.file_mutex->lock();

        switch (type) {
            case op_put: {
                num_existing_pieces = it->second.num_pieces;
                it->second.num_pieces = 1;
                break;
            }
            case op_append: {
                it->second.num_pieces++;
                break;
            }
            case op_get: break;
            case op_del: {
                it->second.being_deleted = true;
                break;
            }
            default: assert(false);
        }
    });

    if (failed) {
        return -1;
    }
    // Delete all existing pieces of the file if it is a PUT
    if (type == op_put) {
        for (unsigned i = 0; i < num_existing_pieces; i++) {
            assert(std::filesystem::remove(sdfs_root_dir + int_path.path + "." + std::to_string(i)) &&
                "mock_sdfs_client contained invalid state");
        }
    }

    utils::backoff([&] {
        return callback(it, m_callback);
    });

    it->second.file_mutex->unlock();

    if (type == op_del) {
        // We don't need to worry about another thread holding file_states[int_path].file_mutex, since being_deleted was set
        //  which means any process would've given up instead of trying to acquire it
        access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
            dir_states[int_dir].files.erase(filename);
            file_states.erase(int_path);
        });
    }

    return 0;
}

uint64_t mock_sdfs_client::mark_transaction_started() {
    std::lock_guard<std::mutex> guard(tx_times_mutex);
    uint64_t time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    uint64_t counter = tx_counter++;
    uint64_t timestamp = (time << 32) + counter;
    transaction_timestamps.insert(timestamp);
    return timestamp;
}

void mock_sdfs_client::mark_transaction_completed(uint64_t timestamp) {
    std::lock_guard<std::mutex> guard(tx_times_mutex);
    transaction_timestamps.erase(timestamp);
    if (transaction_timestamps.find(0) != transaction_timestamps.end()) {
        assert(false);
    }
    return;
}

uint32_t mock_sdfs_client::get_earliest_transaction() {
    std::lock_guard<std::mutex> guard(tx_times_mutex);
    if (transaction_timestamps.size() == 0) {
        return 0;
    } else {
        return (*transaction_timestamps.begin()) >> 32;
    }
}

void mock_sdfs_client::wait_transactions() {
    uint32_t cur_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    while (true) {
        uint32_t earliest_transaction = get_earliest_transaction();
        if (earliest_transaction > cur_time || earliest_transaction == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int mock_sdfs_client::write(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata, bool is_append) {
    uint64_t timestamp = mark_transaction_started();

    // Copy the file into a temporary location
    string sdfs_root_dir = get_sdfs_root_dir();
    string temp_location = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + ".~" + std::to_string(mt());
    std::error_code ec;
    if (!std::filesystem::copy_file(local_filename, temp_location, std::filesystem::copy_options::overwrite_existing, ec)) {
        return -1;
    }

    return access_helper(sdfs_path, [&] (file_state_map::iterator it, master_callback_type const& master_callback) {
        // Rename the temp file to the correct filename
        try {
            unsigned index = it->second.num_pieces - 1;
            std::filesystem::rename(temp_location, sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(index));
            // We assume that this will never throw an exception
            it->second.metadata = metadata;
            mark_transaction_completed(timestamp);
        } catch (...) {
            mark_transaction_completed(timestamp);
            return false;
        }
        // Inform mock_sdfs_master of the operation and return success
        if (master_callback) {
            master_callback(is_append ? op_append : op_put, it);
        }
        return true;
    }, is_append ? op_append : op_put);
}

int mock_sdfs_client::write(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata, bool is_append) {
    uint64_t timestamp = mark_transaction_started();

    // Write the inputter to a temporary location
    string sdfs_root_dir = get_sdfs_root_dir();
    string temp_location = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "~." + std::to_string(mt());

    std::ofstream dest(temp_location, std::ios::binary);
    if (!dest) {
        return -1;
    }
    for (string str : in) {
        dest << str;
    }
    dest.close();
    if (!dest.good()) {
        return -1;
    }

    return access_helper(sdfs_path, [&] (file_state_map::iterator it, master_callback_type const& master_callback) {
        // Rename the temp file to the correct filename and store the metadata
        try {
            unsigned index = it->second.num_pieces - 1;
            string destination = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(index);

            std::filesystem::rename(temp_location, destination);
            // We assume that this will never throw an exception
            it->second.metadata = metadata;
            mark_transaction_completed(timestamp);
        } catch (...) {
            mark_transaction_completed(timestamp);
            return false;
        }
        // Inform mock_sdfs_master of the operation and return success
        if (master_callback) {
            master_callback(is_append ? op_append : op_put, it);
        }
        return true;
    }, is_append ? op_append : op_put);
}

int mock_sdfs_client::put(const string &local_filename, const string &sdfs_path, const sdfs_metadata &metadata) {
    return write(local_filename, sdfs_path, metadata, false);
}

int mock_sdfs_client::put(const inputter<string> &in, const string &sdfs_path, const sdfs_metadata &metadata) {
    return write(in, sdfs_path, metadata, false);
}

int mock_sdfs_client::append(const string &local_filename, const string &sdfs_path, const sdfs_metadata &metadata) {
    return write(local_filename, sdfs_path, metadata, true);
}

int mock_sdfs_client::append(const inputter<string> &in, const string &sdfs_path, const sdfs_metadata &metadata) {
    return write(in, sdfs_path, metadata, true);
}

int mock_sdfs_client::get(const string &local_filename, const string &sdfs_path) {
    uint64_t timestamp = mark_transaction_started();

    string sdfs_root_dir = get_sdfs_root_dir();
    return access_helper(sdfs_path, [&] (file_state_map::iterator it, master_callback_type const& master_callback) {
        std::ofstream dest(local_filename, std::ios::binary);
        if (!dest) {
            mark_transaction_completed(timestamp);
            return false;
        }
        for (unsigned i = 0; i < it->second.num_pieces; i++) {
            std::ifstream src(sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(i), std::ios::binary);
            dest << src.rdbuf();
        }
        dest.close();

        if (!dest.good()) {
            mark_transaction_completed(timestamp);
            return false;
        }

        // Inform mock_sdfs_master of the get operation and return success
        if (master_callback) {
            master_callback(op_get, it);
        }
        mark_transaction_completed(timestamp);
        return true;
    }, op_get);
}

std::optional<sdfs_metadata> mock_sdfs_client::get_metadata(const std::string &sdfs_path) {
    uint64_t timestamp = mark_transaction_started();
    std::optional<sdfs_metadata> retval;
    internal_path int_path = sdfs::convert_path(sdfs_path);
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
        if (file_states.find(int_path) == file_states.end()) {
            retval = std::nullopt;
        } else {
            std::lock_guard<std::recursive_mutex> guard(*file_states[int_path].file_mutex);
            retval = file_states[int_path].metadata;
        }
    });
    mark_transaction_completed(timestamp);
    return retval;
}

int mock_sdfs_client::del(const string &sdfs_path) {
    uint64_t timestamp = mark_transaction_started();
    string sdfs_root_dir = get_sdfs_root_dir();
    return access_helper(sdfs_path, [&] (file_state_map::iterator it, master_callback_type const& master_callback) {
        for (unsigned i = 0; i < it->second.num_pieces; i++) {
            assert(std::filesystem::remove(sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(i)) &&
                "mock_sdfs_client contained invalid state");
        }
        mark_transaction_completed(timestamp);
        // Inform mock_sdfs_master of the del operation and return success
        if (master_callback) {
            master_callback(op_del, it);
        }
        return true;
    }, op_del);
}

int mock_sdfs_client::mkdir(const std::string &sdfs_dir) {
    uint64_t timestamp = mark_transaction_started();
    int retval;
    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));
    internal_path int_parent_dir = sdfs::convert_path(sdfs::get_dir(sdfs::simplify_dir(sdfs_dir)));

    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) != dir_states.end()) {
            retval = -1; // Quit if the directory already exists
            return;
        }

        if (int_parent_dir.path != "" && dir_states.find(int_parent_dir) == dir_states.end()) {
            retval = -1; // Quit if the parent directory does not exist
            return;
        }
        // By induction, we know that all the parent directories must also exist, so we can safely just check the immediate parent

        if (int_parent_dir.path != "" && dir_states[int_parent_dir].being_deleted) {
            retval = -1; // Quit if the parent directory is being deleted
            return;
        }

        dir_states[int_dir] = dir_state();
        dir_states[int_parent_dir].dirs.insert(sdfs::get_name(sdfs::simplify_dir(sdfs_dir)));
        retval = 0;
    });
    mark_transaction_completed(timestamp);
    return retval;
}

int mock_sdfs_client::rmdir(const std::string &sdfs_dir) {
    uint64_t timestamp = mark_transaction_started();
    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));

    std::vector<std::tuple<internal_path, std::recursive_mutex*, unsigned>> files_to_delete;
    std::vector<internal_path> dirs_to_delete;
    std::vector<internal_path> files_being_deleted;
    std::vector<internal_path> dirs_being_deleted;
    bool failed = false;
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) == dir_states.end()) {
            failed = true; // Quit if the directory doesn't exist
            return;
        }

        if (dir_states[int_dir].being_deleted) {
            failed = true; // Quit if the directory is already being deleted
            return;
        }

        std::vector<internal_path> subdirs;
        std::vector<internal_path> subfiles;
        get_children(int_dir, dir_states, subdirs, subfiles);

        // Lock all the files in the directory + subdirectories
        // This is potentially very expensive, since access_pieces blocks
        // the global environment mutex, which blocks EVERY access to shared state
        // ...but it's a mock so who cares

        // Normally, we would have to lock the files in some fixed order, but since
        // this is the only place multiple files are locked, and rmdir can only be called
        // once at a time on one directory, we can lock them in any order
        for (const internal_path &filename : subfiles) {
            // Check that the file is not being deleted already -- if it is, just skip it
            // We will wait for it to be actually deleted in the end
            if (!file_states[filename].being_deleted) {
                std::recursive_mutex *cur_lock = file_states[filename].file_mutex.get();
                cur_lock->lock();

                file_states[filename].being_deleted = true;

                files_to_delete.push_back(std::make_tuple(filename, cur_lock, file_states[filename].num_pieces));
            } else {
                files_being_deleted.push_back(filename);
            }
        }

        // Mark this directory as well as all subdirectories as being deleted
        dir_states[int_dir].being_deleted = true;
        for (const internal_path &dir : subdirs) {
            // If the directory is already being deleted, skip it, and we will wait
            // for it to be deleted at the end along with the files being deleted
            if (!dir_states[dir].being_deleted) {
                dir_states[dir].being_deleted = true;
                dirs_to_delete.push_back(dir);
            } else {
                dirs_being_deleted.push_back(dir);
            }
        }
    });

    if (failed) {
        mark_transaction_completed(timestamp);
        return -1;
    }

    string sdfs_root_dir = get_sdfs_root_dir();
    for (auto &[int_path, lock, pieces] : files_to_delete) {
        for (unsigned i = 0; i < pieces; i++) {
            assert(std::filesystem::remove(sdfs_root_dir + int_path.path + "." + std::to_string(i)) &&
                "mock_sdfs_client contained invalid state");
        }
        lock->unlock();
    }

    // Wait for the files that were being deleted to be deleted
    // No new writes will have occurred because the entire directory was marked as being_deleted
    utils::backoff([&] {
        bool all_deleted = true;
        access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
            for (const internal_path &filename : files_being_deleted) {
                if (file_states.find(filename) != file_states.end()) {
                    assert(file_states[filename].being_deleted &&
                        "File marked for deletion was either unmarked or recreated despite parent dir being marked for deletion");
                    all_deleted = false;
                    return;
                }
            }

            for (const internal_path &dir : dirs_being_deleted) {
                if (dir_states.find(dir) != dir_states.end()) {
                    assert(dir_states[dir].being_deleted &&
                        "Dir marked for deletion was either unmarked or recreated despite parent dir being marked for deletion");
                    all_deleted = false;
                    return;
                }
            }
        });

        return all_deleted;
    });

    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
        dir_states.erase(int_dir);
        for (auto &dir : dirs_to_delete) {
            dir_states.erase(dir);
        }
        for (auto &[int_path, _1, _2] : files_to_delete) {
            file_states.erase(int_path);
        }
    });

    mark_transaction_completed(timestamp);
    return 0;
}

std::optional<std::vector<string>> mock_sdfs_client::ls_files(const string &sdfs_dir) {
    uint64_t timestamp = mark_transaction_started();
    std::optional<std::vector<string>> retval;

    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) == dir_states.end()) {
            retval = std::nullopt;
        } else {
            retval = std::vector<string>();
            for (const string &filename : dir_states[int_dir].files) {
                retval.value().push_back(filename);
            }
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

std::optional<std::vector<string>> mock_sdfs_client::ls_dirs(const string &sdfs_dir) {
    uint64_t timestamp = mark_transaction_started();
    std::optional<std::vector<string>> retval;

    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) == dir_states.end()) {
            retval = std::nullopt;
        } else {
            retval = std::vector<string>();
            for (const string &dirname : dir_states[int_dir].dirs) {
                retval.value().push_back(dirname);
            }
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

int mock_sdfs_client::get_num_shards(const string &sdfs_path) {
    uint64_t timestamp = mark_transaction_started();
    int retval;
    internal_path int_path = sdfs::convert_path(sdfs_path);

    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (file_states.find(int_path) == file_states.end()) {
            retval = -1;
        } else {
            std::lock_guard<std::recursive_mutex> guard(*file_states[int_path].file_mutex);
            retval = file_states[int_path].num_pieces;
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

register_test_service<sdfs_client, mock_sdfs_client> register_mock_sdfs_client;
