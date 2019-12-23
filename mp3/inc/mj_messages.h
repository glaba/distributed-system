#pragma once

#include "partitioner.h"
#include "processor.h"

#include <string>
#include <variant>
#include <cassert>
#include <vector>

struct mj_start_job {
    std::string exe;
    int num_workers;
    partitioner::type partitioner_type;
    std::string sdfs_src_dir;
    processor::type processor_type;
    std::string sdfs_output_dir;
    int num_files_parallel;
    int num_appends_parallel;
};

struct mj_not_master {
    std::string master_node; // When this is set to "", there is no master
};

struct mj_job_end {
    int succeeded;
};

struct mj_assign_job {
    int job_id;
    std::string exe;
    std::string sdfs_src_dir;
    std::vector<std::string> input_files;
    processor::type processor_type;
    std::string sdfs_output_dir;
    int num_files_parallel;
    int num_appends_parallel;
};

struct mj_request_append_perm {
    int job_id;
    std::string hostname; // The hostname of the node sending the message
    std::string input_file;
    std::string output_file;
};

struct mj_append_perm {
    int allowed;
};

struct mj_file_done {
    int job_id;
    std::string hostname;
    std::string file;
};

struct mj_job_failed {
    int job_id;
};

struct mj_job_end_worker {
    int job_id;
};

class mj_message {
public:
    using msg_data = std::variant<
        mj_start_job, mj_not_master, mj_job_end,
        mj_assign_job, mj_request_append_perm, mj_append_perm, mj_file_done, mj_job_failed, mj_job_end_worker>;

    enum mj_msg_type {
        START_JOB,
        NOT_MASTER,
        JOB_END,
        ASSIGN_JOB,
        REQUEST_APPEND_PERM,
        APPEND_PERM,
        FILE_DONE,
        JOB_FAILED,
        JOB_END_WORKER,
        INVALID
    };

    // Creates a message from a buffer (safe)
    mj_message(const char *buf, unsigned length);

    // Creates an empty message
    mj_message(uint32_t id_, msg_data d) {
        id = id_;
        data = d;
        set_msg_type();
    }

    // Returns true if the deserialized message is not malformed
    bool is_well_formed() {
        return (msg_type != INVALID);
    }

    // Gets the type of the message
    mj_msg_type get_msg_type() {
        return msg_type;
    }

    uint32_t get_id() {
        return id;
    }

    // Gets the data contained in the message
    template <typename T> // T is the type of the data
    T get_msg_data() {
        assert(std::holds_alternative<T>(data));
        return std::get<T>(data);
    }

    // Serializes the message and returns a string containing the message
    std::string serialize();

private:
    // Sets the message type based on the value stored in data
    void set_msg_type();

    uint32_t id;
    mj_msg_type msg_type;
    msg_data data;
};
