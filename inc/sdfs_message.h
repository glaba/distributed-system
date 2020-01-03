#pragma once

#include <string>
#include <memory>
#include <cassert>

class sdfs_message {
public:
    // Major operations in the filesystem
    // Might squeeze some more specific operations here later
    // If the implementation demands
    enum msg_type {
        empty, mn_put, mn_get, mn_ls, mn_gmd, mn_append, mn_gidx, put, get, del, ls, gmd, append, gidx, rep, ack, files, success, fail
    };

    // Creates a message from a buffer
    sdfs_message(char *buf, unsigned length);

    // Creates a message with no attributes or type
    sdfs_message() : type(), sdfs_hostname(), sdfs_filename() {}

    // Sets the message to be a mn_put message
    void set_type_mn_put(std::string const& hostname) {
        type = msg_type::mn_put;
        sdfs_hostname = hostname;
    }

    // Sets the message to be a mn_get message
    void set_type_mn_get(std::string const& hostname) {
        type = msg_type::mn_get;
        sdfs_hostname = hostname;
    }

    // Sets the message to be a mn_gmd message
    void set_type_mn_gmd(std::string const& hostname) {
        type = msg_type::mn_gmd;
        sdfs_hostname = hostname;
    }

    // Sets the message to be a mn_ls message
    void set_type_mn_ls(std::string const& payload) {
        type = msg_type::mn_ls;
        data = payload;
    }

    // Sets the message to be a rep message
    void set_type_rep(std::string const& hostname, std::string const& filename) {
        type = msg_type::rep;
        sdfs_hostname = hostname;
        sdfs_filename = filename;
    }

    // Sets the message to be a put message
    void set_type_put(std::string const& filename) {
        type = msg_type::put;
        sdfs_filename = filename;
    }

    // Sets the message to be a get message
    void set_type_get(std::string const& filename) {
        type = msg_type::get;
        sdfs_filename = filename;
    }

    // Sets the message to be a del message
    void set_type_del(std::string const& filename) {
        type = msg_type::del;
        sdfs_filename = filename;
    }

    // Sets the message to be a get message
    void set_type_gmd(std::string const& filename) {
        type = msg_type::gmd;
        sdfs_filename = filename;
    }

    // Sets the message to be a get message
    void set_type_mn_gidx(std::string const& filename, std::string const& idx) {
        type = msg_type::mn_gidx;
        sdfs_filename = filename;
        data = idx;
    }

    // Sets the message to be a get_idx message
    void set_type_gidx(std::string const& filename) {
        type = msg_type::gidx;
        sdfs_filename = filename;
    }

    // Sets the message to be an ack message
    void set_type_ack() {
        type = msg_type::ack;
    }

    // Sets the message to be an success message
    void set_type_success() {
        type = msg_type::success;
    }

    // Sets the message to be an fail message
    void set_type_fail() {
        type = msg_type::fail;
    }

    // Sets the message to be a files message
    void set_type_files(std::string const& hostname, std::string const& files) {
        type = msg_type::files;
        sdfs_hostname = hostname;
        data = files;
    }

    // Sets the message to be an ls message
    void set_type_ls(std::string const& filename) {
        type = msg_type::ls;
        sdfs_filename = filename;
    }

    // Sets the message to be an append message
    void set_type_append(std::string const& filename, std::string const& metadata) {
        type = msg_type::append;
        sdfs_filename = filename;
        data = metadata;
    }

    // Sets the message to be an mn_append message
    void set_type_mn_append(std::string const& hostname, std::string const& filename) {
        type = msg_type::mn_append;
        sdfs_hostname = hostname;
        sdfs_filename = filename;
    }

    auto get_type() const -> msg_type {
        return type;
    }

    auto get_sdfs_filename() const -> std::string {
        return sdfs_filename;
    }

    auto get_sdfs_hostname() const -> std::string {
        return sdfs_hostname;
    }

    auto get_data() const -> std::string {
        return data;
    }

    auto get_type_as_string() const -> std::string;

    // Serializes the message and returns a string containing the message
    auto serialize() const -> std::string;

private:
    msg_type type;
    std::string data;          // data for certain ops (MN_LS)
    std::string sdfs_hostname; // sdfs hostname for certain ops (MN_PUT, MN_GET)
    std::string sdfs_filename; // sdfs filename for certain ops (PUT, GET, LS, DEL)
};
