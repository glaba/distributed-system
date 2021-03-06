#include "mock_sdfs_client.h"
#include "utils.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>

using std::string;
using std::unique_ptr;
using std::optional;
using namespace sdfs;
using namespace std::chrono;

mock_sdfs_client::mock_sdfs_client(environment &env)
    : config(env.get<configuration>())
    , lg(env.get<logger_factory>()->get_logger("mock_sdfs_client"))
    , mt(std::chrono::system_clock::now().time_since_epoch().count()) {}

auto mock_sdfs_client::init_state() -> unique_ptr<service_state> {
    // All SDFS files for an environment group will be stored in the directory of just one of the nodes
    mock_sdfs_state *state = new mock_sdfs_state();
    state->sdfs_root_dir = config->get_sdfs_dir();
    return std::unique_ptr<service_state>(static_cast<service_state*>(state));
}

void mock_sdfs_client::start() {}

void mock_sdfs_client::stop() {}

auto operator==(internal_path const& p1, internal_path const& p2) -> bool {
    return p1.path == p2.path;
}

auto mock_sdfs_client::get_sdfs_root_dir() -> string {
    return unlocked<mock_sdfs_state>::dyn_cast(access_state())->sdfs_root_dir;
}

void mock_sdfs_client::get_children(internal_path const& dir, dir_state_map &dir_states,
    std::vector<internal_path> &subdirs, std::vector<internal_path> &subfiles)
{
    for (string const& subfile : dir_states[dir].files) {
        subfiles.push_back(sdfs::convert_path(sdfs::deconvert_path(dir) + "/" + subfile));
    }

    for (string const& subdir : dir_states[dir].dirs) {
        internal_path int_subdir = sdfs::convert_path(sdfs::deconvert_path(dir) + "/" + subdir);
        subdirs.push_back(int_subdir);
        get_children(int_subdir, dir_states, subdirs, subfiles);
    }
}

void mock_sdfs_client::access_pieces(std::function<void(dir_state_map&, file_state_map&, master_callback_type const&)> callback)
{
    unlocked<mock_sdfs_state> state = unlocked<mock_sdfs_state>::dyn_cast(access_state());
    callback(state->dir_states, state->file_states, state->master_callback);
}

void mock_sdfs_client::on_event(master_callback_type const& callback) {
    unlocked<mock_sdfs_state> state = unlocked<mock_sdfs_state>::dyn_cast(access_state());
    state->master_callback = callback;
}

auto mock_sdfs_client::access_helper(string const& sdfs_path, access_helper_callback const& callback, op_type type) -> int
{
    string filename = sdfs::get_name(sdfs_path);
    internal_path int_dir = sdfs::convert_path(sdfs::get_dir(sdfs_path));
    internal_path int_path = sdfs::convert_path(sdfs_path);
    string sdfs_root_dir = get_sdfs_root_dir();

    { // Lock the file while deleting / renaming
        unlocked<file_state> f_state{unlocked<file_state>::empty()};

        master_callback_type m_callback;
        unsigned num_existing_pieces;

        access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
        {
            if (int_dir.path != "" && (dir_states.find(int_dir) == dir_states.end() || dir_states[int_dir].being_deleted)) {
                // Either the directory doesn't exist or it's being deleted
                return;
            }

            if (file_states.find(int_path) != file_states.end() && file_states[int_path]()->being_deleted) {
                // Avoid accessing a file while it's being deleted
                return;
            }

            // For a get or a del, ensure that the file exists
            if (type == op_get || type == op_del) {
                if (file_states.find(int_path) == file_states.end()) {
                    return;
                }
            }

            m_callback = master_callback;

            // Update all the relevant data structures
            dir_states[int_dir].files.insert(filename);
            f_state = file_states[int_path]();
            switch (type) {
                case op_put: {
                    num_existing_pieces = f_state->num_pieces;
                    f_state->num_pieces = 1;
                    break;
                }
                case op_append: {
                    f_state->num_pieces++;
                    break;
                }
                case op_get: break;
                case op_del: {
                    f_state->being_deleted = true;
                    break;
                }
                default: assert(false);
            }
        });

        if (!f_state) {
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
            return callback(f_state, m_callback);
        });
    }

    if (type == op_del) {
        // We don't need to worry about another thread holding file_states[int_path], since being_deleted was set
        //  which means any process would've given up instead of trying to acquire it
        access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
            dir_states[int_dir].files.erase(filename);
            file_states.erase(int_path);
        });
    }

    return 0;
}

auto mock_sdfs_client::mark_transaction_started() -> uint64_t {
    unlocked<transaction_timestamps> tx_timestamps = tx_timestamps_lock();
    uint64_t time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    uint64_t counter = tx_timestamps->counter++;
    uint64_t timestamp = (time << 32) + counter;
    tx_timestamps->set.insert(timestamp);
    return timestamp;
}

void mock_sdfs_client::mark_transaction_completed(uint64_t timestamp) {
    unlocked<transaction_timestamps> tx_timestamps = tx_timestamps_lock();
    tx_timestamps->set.erase(timestamp);
    if (tx_timestamps->set.find(0) != tx_timestamps->set.end()) {
        assert(false);
    }
    return;
}

auto mock_sdfs_client::get_earliest_transaction() const -> uint32_t {
    unlocked<transaction_timestamps> tx_timestamps = tx_timestamps_lock();
    if (tx_timestamps->set.size() == 0) {
        return 0;
    } else {
        return (*tx_timestamps->set.begin()) >> 32;
    }
}

void mock_sdfs_client::wait_transactions() const {
    uint32_t cur_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    while (true) {
        uint32_t earliest_transaction = get_earliest_transaction();
        if (earliest_transaction > cur_time || earliest_transaction == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

auto mock_sdfs_client::write(string const& local_filename, string const& sdfs_path, sdfs_metadata const& metadata, bool is_append) -> int {
    uint64_t timestamp = mark_transaction_started();

    // Copy the file into a temporary location
    string sdfs_root_dir = get_sdfs_root_dir();
    string temp_location = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + ".~" + std::to_string((*mt())());
    std::error_code ec;
    if (!std::filesystem::copy_file(local_filename, temp_location, std::filesystem::copy_options::overwrite_existing, ec)) {
        return -1;
    }

    return access_helper(sdfs_path, [&] (unlocked<file_state> &f_state, master_callback_type const& master_callback) {
        // Rename the temp file to the correct filename
        try {
            unsigned index = f_state->num_pieces - 1;
            std::filesystem::rename(temp_location, sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(index));
            // We assume that this will never throw an exception
            f_state->metadata = metadata;
            mark_transaction_completed(timestamp);
        } catch (...) {
            mark_transaction_completed(timestamp);
            return false;
        }
        // Inform mock_sdfs_master of the operation and return success
        if (master_callback) {
            master_callback(is_append ? op_append : op_put, sdfs_path, f_state);
        }
        return true;
    }, is_append ? op_append : op_put);
}

auto mock_sdfs_client::write(inputter<string> const& in, string const& sdfs_path, sdfs_metadata const& metadata, bool is_append) -> int {
    uint64_t timestamp = mark_transaction_started();

    // Write the inputter to a temporary location
    string sdfs_root_dir = get_sdfs_root_dir();
    string temp_location = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "~." + std::to_string((*mt())());

    std::ofstream dest(temp_location, std::ios::binary);
    if (!dest) {
        return -1;
    }
    for (string const& str : in) {
        dest << str;
    }
    dest.close();
    if (!dest.good()) {
        return -1;
    }

    return access_helper(sdfs_path, [&] (unlocked<file_state> &f_state, master_callback_type const& master_callback) {
        // Rename the temp file to the correct filename and store the metadata
        try {
            unsigned index = f_state->num_pieces - 1;
            string destination = sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(index);

            std::filesystem::rename(temp_location, destination);
            // We assume that this will never throw an exception
            f_state->metadata = metadata;
            mark_transaction_completed(timestamp);
        } catch (...) {
            mark_transaction_completed(timestamp);
            return false;
        }
        // Inform mock_sdfs_master of the operation and return success
        if (master_callback) {
            master_callback(is_append ? op_append : op_put, sdfs_path, f_state);
        }
        return true;
    }, is_append ? op_append : op_put);
}

auto mock_sdfs_client::put(string const& local_filename, string const& sdfs_path, sdfs_metadata const& metadata) -> int {
    return write(local_filename, sdfs_path, metadata, false);
}

auto mock_sdfs_client::put(inputter<string> const& in, string const& sdfs_path, sdfs_metadata const& metadata) -> int {
    return write(in, sdfs_path, metadata, false);
}

auto mock_sdfs_client::append(string const& local_filename, string const& sdfs_path, sdfs_metadata const& metadata) -> int {
    return write(local_filename, sdfs_path, metadata, true);
}

auto mock_sdfs_client::append(inputter<string> const& in, string const& sdfs_path, sdfs_metadata const& metadata) -> int {
    return write(in, sdfs_path, metadata, true);
}

auto mock_sdfs_client::get(string const& local_filename, string const& sdfs_path) -> int {
    uint64_t timestamp = mark_transaction_started();

    string sdfs_root_dir = get_sdfs_root_dir();
    return access_helper(sdfs_path, [&] (unlocked<file_state> &f_state, master_callback_type const& master_callback) {
        std::ofstream dest(local_filename, std::ios::binary);
        if (!dest) {
            mark_transaction_completed(timestamp);
            return false;
        }
        for (unsigned i = 0; i < f_state->num_pieces; i++) {
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
            master_callback(op_get, sdfs_path, f_state);
        }
        mark_transaction_completed(timestamp);
        return true;
    }, op_get);
}

auto mock_sdfs_client::get_metadata(string const& sdfs_path) -> std::optional<sdfs_metadata> {
    uint64_t timestamp = mark_transaction_started();
    std::optional<sdfs_metadata> retval;
    internal_path int_path = sdfs::convert_path(sdfs_path);
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
        if (file_states.find(int_path) == file_states.end()) {
            retval = std::nullopt;
        } else {
            retval = file_states[int_path]()->metadata;
        }
    });
    mark_transaction_completed(timestamp);
    return retval;
}

auto mock_sdfs_client::del(string const& sdfs_path) -> int {
    uint64_t timestamp = mark_transaction_started();
    string sdfs_root_dir = get_sdfs_root_dir();
    return access_helper(sdfs_path, [&] (unlocked<file_state> &f_state, master_callback_type const& master_callback) {
        for (unsigned i = 0; i < f_state->num_pieces; i++) {
            assert(std::filesystem::remove(sdfs_root_dir + sdfs::convert_path(sdfs_path).path + "." + std::to_string(i)) &&
                "mock_sdfs_client contained invalid state");
        }
        mark_transaction_completed(timestamp);
        // Inform mock_sdfs_master of the del operation and return success
        if (master_callback) {
            master_callback(op_del, sdfs_path, f_state);
        }
        return true;
    }, op_del);
}

auto mock_sdfs_client::mkdir(string const& sdfs_dir) -> int {
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

auto mock_sdfs_client::rmdir(string const& sdfs_dir) -> int {
    uint64_t timestamp = mark_transaction_started();
    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));

    std::vector<std::tuple<internal_path, unlocked<file_state>, unsigned>> files_to_delete;
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
        for (internal_path const& filename : subfiles) {
            // Check that the file is not being deleted already -- if it is, just skip it
            // We will wait for it to be actually deleted in the end
            unlocked<file_state> f_state = file_states[filename]();
            if (!f_state->being_deleted) {
                f_state->being_deleted = true;

                files_to_delete.push_back({filename, std::move(f_state), f_state->num_pieces});
            } else {
                files_being_deleted.push_back(filename);
            }
        }

        // Mark this directory as well as all subdirectories as being deleted
        dir_states[int_dir].being_deleted = true;
        for (internal_path const& dir : subdirs) {
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
    for (auto const& [int_path, lock, pieces] : files_to_delete) {
        for (unsigned i = 0; i < pieces; i++) {
            assert(std::filesystem::remove(sdfs_root_dir + int_path.path + "." + std::to_string(i)) &&
                "mock_sdfs_client contained invalid state");
        }
    }
    // Unlock all the locked files
    files_to_delete.clear();

    // Wait for the files that were being deleted to be deleted
    // No new writes will have occurred because the entire directory was marked as being_deleted
    utils::backoff([&] {
        bool all_deleted = true;
        access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback) {
            for (internal_path const& filename : files_being_deleted) {
                if (file_states.find(filename) != file_states.end()) {
                    assert(file_states[filename]()->being_deleted &&
                        "File marked for deletion was either unmarked or recreated despite parent dir being marked for deletion");
                    all_deleted = false;
                    return;
                }
            }

            for (internal_path const& dir : dirs_being_deleted) {
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
        for (auto const& dir : dirs_to_delete) {
            dir_states.erase(dir);
        }
        for (auto const& [int_path, _1, _2] : files_to_delete) {
            file_states.erase(int_path);
        }
    });

    mark_transaction_completed(timestamp);
    return 0;
}

auto mock_sdfs_client::ls_files(string const& sdfs_dir) -> std::optional<std::vector<string>> {
    uint64_t timestamp = mark_transaction_started();
    std::optional<std::vector<string>> retval;

    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) == dir_states.end()) {
            retval = std::nullopt;
        } else {
            retval = std::vector<string>();
            for (string const& filename : dir_states[int_dir].files) {
                retval.value().push_back(filename);
            }
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

auto mock_sdfs_client::ls_dirs(string const& sdfs_dir) -> std::optional<std::vector<string>> {
    uint64_t timestamp = mark_transaction_started();
    std::optional<std::vector<string>> retval;

    internal_path int_dir = sdfs::convert_path(sdfs::simplify_dir(sdfs_dir));
    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (dir_states.find(int_dir) == dir_states.end()) {
            retval = std::nullopt;
        } else {
            retval = std::vector<string>();
            for (string const& dirname : dir_states[int_dir].dirs) {
                retval.value().push_back(dirname);
            }
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

auto mock_sdfs_client::get_num_shards(string const& sdfs_path) -> int {
    uint64_t timestamp = mark_transaction_started();
    int retval;
    internal_path int_path = sdfs::convert_path(sdfs_path);

    access_pieces([&] (dir_state_map &dir_states, file_state_map &file_states, master_callback_type const& master_callback)
    {
        if (file_states.find(int_path) == file_states.end()) {
            retval = -1;
        } else {
            retval = file_states[int_path]()->num_pieces;
        }
    });

    mark_transaction_completed(timestamp);
    return retval;
}

register_test_service<sdfs_client, mock_sdfs_client> register_mock_sdfs_client;
