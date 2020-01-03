#pragma once

#include <string>
#include <unordered_map>
#include <functional>

// TODO: replace the key and value with generic serializable types
using sdfs_metadata = std::unordered_map<std::string, std::string>;

namespace sdfs {
    // Callback types used in sdfs_master
    // Callback parameters are: sdfs_path, (index of piece that was appended), sdfs_metadata for the file
    using put_callback = std::function<void(std::string const&, sdfs_metadata const&)>;
    using append_callback = std::function<void(std::string const&, unsigned, sdfs_metadata const&)>;
    using get_callback = std::function<void(std::string const&, sdfs_metadata const&)>;
    using del_callback = std::function<void(std::string const&, sdfs_metadata const&)>;

    struct internal_path {
        std::string path;
        inline bool operator==(internal_path const& p) const {
            return path == p.path;
        }
    };

    struct internal_path_hash {
        inline std::size_t operator()(internal_path const& p) const {
            return static_cast<size_t>(static_cast<uint32_t>(std::hash<std::string>()(p.path)));
        }
    };

    std::string simplify_dir(std::string const& sdfs_dir);
    std::string get_dir(std::string const& sdfs_path);
    std::string get_name(std::string const& sdfs_path);

    internal_path convert_path(std::string const& sdfs_path);
    std::string deconvert_path(internal_path const& p);
}
