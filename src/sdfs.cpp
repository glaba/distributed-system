#include "sdfs.h"

#include <cassert>

using std::string;

namespace sdfs {
    string simplify_dir(const string &sdfs_dir) {
        if (sdfs_dir.at(sdfs_dir.length() - 1) == '/') {
            return sdfs_dir.substr(0, sdfs_dir.length() - 1);
        } else {
            return sdfs_dir;
        }
    }

    string get_dir(const string &sdfs_path) {
        size_t backslash_index = sdfs_path.find_last_of("/");
        if (backslash_index != string::npos) {
            return sdfs_path.substr(0, backslash_index);
        } else {
            return "";
        }
    }

    string get_name(const string &sdfs_path) {
        size_t backslash_index = sdfs_path.find_last_of("/");
        if (backslash_index != string::npos) {
            return sdfs_path.substr(backslash_index + 1);
        } else {
            return sdfs_path;
        }
    }

    internal_path convert_path(const string &sdfs_path) {
        internal_path retval;

        for (unsigned i = 0; i < sdfs_path.length(); i++) {
            if (sdfs_path.at(i) == '%') {
                retval.path += "%%";
            } else if (sdfs_path.at(i) == '/') {
                retval.path += "%|";
            } else {
                retval.path += sdfs_path.at(i);
            }
        }

        return retval;
    }

    string deconvert_path(const internal_path &p) {
        string retval;

        for (unsigned i = 0; i < p.path.length(); i++) {
            if (p.path.at(i) == '%') {
                switch (p.path.at(i + 1)) {
                    case '%': retval += "%"; break;
                    case '|': retval += "/"; break;
                    default: assert(false);
                }
                i++;
            } else {
                retval += p.path.at(i);
            }
        }

        return retval;
    }
}
