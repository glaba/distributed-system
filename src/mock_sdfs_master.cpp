#include "mock_sdfs_master.h"

#include <stdlib.h>
#include <unordered_set>

using std::string;

mock_sdfs_master::mock_sdfs_master(environment &env)
    : client(dynamic_cast<mock_sdfs_client*>(env.get<sdfs_client>())) {}

std::vector<string> ls(string directory) {
    std::vector<string> retval;
    char buffer[1024];
    FILE *stream = popen(("ls \"" + directory + "\"").c_str(), "r");
    if (stream) {
        string cur_line = "";
        while (!feof(stream)) {
            size_t bytes_read = fread(static_cast<void*>(buffer), 1, 1024, stream);
            if (bytes_read == 1024 || feof(stream)) {
                cur_line += string(buffer, bytes_read);

                while (true) {
                    int newline_pos = -1;
                    for (size_t i = 0; i < cur_line.length(); i++) {
                        if (cur_line.at(i) == '\n') {
                            newline_pos = i;
                            break;
                        }
                    }

                    // Check if we encountered a \n, which means we should call the callback
                    if (newline_pos >= 0) {
                        retval.push_back(cur_line.substr(0, newline_pos));
                        cur_line = cur_line.substr(newline_pos + 1);
                    } else {
                        break;
                    }
                }
            } else {
                assert(false);
            }
        }
        pclose(stream);
        return retval;
    } else {
        assert(false);
    }
}

std::vector<string> mock_sdfs_master::get_files_by_prefix(string prefix) {
    string sdfs_dir = dynamic_cast<mock_sdfs_client*>(client)->get_sdfs_dir();
    std::vector<string> raw_files = ls(sdfs_dir);
    std::unordered_set<string> output;
    for (string raw_file : raw_files) {
        // Get the original filename
        string original_file = raw_file.substr(0, raw_file.find_last_of("."));
        if (original_file.find(prefix) == 0) {
            output.insert(original_file);
        }
    }
    return std::vector<string>(output.begin(), output.end());
}

register_test_service<sdfs_master, mock_sdfs_master> register_mock_sdfs_master;
