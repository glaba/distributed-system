#pragma once

#include <string>
#include <variant>
#include <cassert>
#include <vector>

struct maple_start_job {
    std::string maple_exe;
    int num_maples;
    std::string sdfs_intermediate_filename_prefix;
    std::string sdfs_src_dir;
};

struct maple_job_end {
    int succeeded;
};

struct maple_assign_job {
    int job_id;
    std::string maple_exe;
    std::vector<std::string> input_files;
    std::string sdfs_intermediate_filename_prefix;
};

struct maple_request_append_perm {
    int job_id;
    std::string key;
};

struct maple_append_perm {
    int allowed;
};

class maple_message {
public:
    enum maple_msg_type {
        START_JOB,
        JOB_END,
        ASSIGN_JOB,
        REQUEST_APPEND_PERM,
        APPEND_PERM,
        INVALID
    };

    // Creates a message from a buffer (safe)
    maple_message(const char *buf, unsigned length);

    // Creates an empty message
    maple_message() : msg_type(INVALID) {}

    // Returns true if the deserialized message is not malformed
    bool is_well_formed() {
        return (msg_type != INVALID);
    }

    // Gets the type of the message
    maple_msg_type get_msg_type() {
        return msg_type;
    }

    // Gets the data contained in the message
    template <typename T> // T is the type of the data
    T get_msg_data() {
        assert(std::holds_alternative<T>(data));
        return std::get<T>(data);
    }

    template <typename T> // T is the type of the data
    void set_msg_data(T data_) {
        data = data_;
        set_msg_type();
    }

    // Serializes the message and returns a string containing the message
    std::string serialize();

private:
    // Sets the message type based on the value stored in data
    void set_msg_type();

    maple_msg_type msg_type;
    std::variant<maple_start_job, maple_job_end, maple_assign_job, maple_request_append_perm, maple_append_perm> data;
};
