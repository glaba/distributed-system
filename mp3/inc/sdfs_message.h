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
        empty, mn_put, mn_get, put, get, del, ls, ack, success, fail
    };

    // Creates a message from a buffer
    sdfs_message(char *buf, unsigned length);

    // Creates a message from a filename
    sdfs_message(std::string sdfs_filename_) : sdfs_filename(sdfs_filename_) {}

    // Sets the message to be a mn_put message
    void set_type_mn_put(std::string filename) {
        type = msg_type::mn_put;
        sdfs_filename = filename;
    }

    // Sets the message to be a mn_get message
    void set_type_mn_get(std::string filename) {
        type = msg_type::mn_get;
        sdfs_filename = filename;
    }

    // Sets the message to be a put message
    void set_type_put(std::string filename) {
        type = msg_type::put;
        sdfs_filename = filename;
    }

    // Sets the message to be a get message
    void set_type_get(std::string filename) {
        type = msg_type::get;
        sdfs_filename = filename;
    }

    // Sets the message to be a del message
    void set_type_del(std::string filename) {
        type = msg_type::del;
        sdfs_filename = filename;
    }

    // Sets the message to be an ack message
    void set_type_ack(std::string filename) {
        type = msg_type::ack;
        sdfs_filename = "";
    }

    // Sets the message to be an success message
    void set_type_success(std::string filename) {
        type = msg_type::success;
        sdfs_filename = "";
    }

    // Sets the message to be an fail message
    void set_type_fail(std::string filename) {
        type = msg_type::fail;
        sdfs_filename = "";
    }

    // Sets the message to be an ls message
    void set_type_ls(std::string filename) {
        type = msg_type::ls;
        sdfs_filename = filename;
    }

    msg_type get_type() {
        return type;
    }

    std::string get_sdfs_filename() {
        return sdfs_filename;
    }

    // Serializes the message and returns a string containing the message
    std::string serialize();

private:
    msg_type type;
    std::string sdfs_hostname; // sdfs hostname for certain ops (MN_PUT, MN_GET)
    std::string sdfs_filename; // sdfs filename for certain ops (LS, DEL)
};
