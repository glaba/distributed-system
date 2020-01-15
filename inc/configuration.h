#pragma once

#include <string>

class configuration {
public:
    virtual void set_hostname(std::string const& hostname) = 0;
    virtual void set_first_node(bool is_first_node_) = 0;
    virtual void set_hb_port(int port) = 0;
    virtual void set_election_port(int port) = 0;
    virtual void set_sdfs_internal_port(int port) = 0;
    virtual void set_sdfs_master_port(int port) = 0;
    virtual void set_mj_internal_port(int port) = 0;
    virtual void set_mj_master_port(int port) = 0;
    virtual void set_dir(std::string const& dir) = 0;
    virtual void set_sdfs_subdir(std::string const& subdir) = 0;
    virtual void set_mj_subdir(std::string const& subdir) = 0;
    virtual void set_shared_config_subdir(std::string const& subdir) = 0;

    virtual auto get_hostname() const -> std::string = 0;
    virtual auto is_first_node() const -> bool = 0;
    virtual auto get_hb_port() const -> int = 0;
    virtual auto get_election_port() const -> int = 0;
    virtual auto get_sdfs_internal_port() const -> int = 0;
    virtual auto get_sdfs_master_port() const -> int = 0;
    virtual auto get_mj_internal_port() const -> int = 0;
    virtual auto get_mj_master_port() const -> int = 0;
    virtual auto get_dir() const -> std::string = 0;
    virtual auto get_sdfs_dir() const -> std::string = 0;
    virtual auto get_mj_dir() const -> std::string = 0;
    virtual auto get_shared_config_dir() const -> std::string = 0;
};
