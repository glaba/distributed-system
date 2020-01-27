#pragma once

#include "partitioner.h"
#include "processor.h"
#include "serialization.h"

#include <string>
#include <variant>
#include <cassert>
#include <vector>

namespace mj_message {
    enum class msg_type {
        start_job, not_master, job_end, assign_job, request_append_perm, append_perm, file_done, job_failed, job_end_worker
    };

    class common_data : public serializable<common_data> {
    public:
        common_data() = default;
        common_data(uint32_t type, uint32_t id) : type(type), id(id) {}

        uint32_t type;
        uint32_t id;
    };

    class start_job : public serializable<start_job, common_data> {
    public:
        start_job() = default;
        start_job(uint32_t id, std::string const& exe, uint32_t num_workers, uint32_t partitioner_type, uint32_t processor_type,
            std::string const& sdfs_src_dir, std::string const& sdfs_output_dir, uint32_t num_files_parallel, uint32_t num_appends_parallel)
            : common(static_cast<uint32_t>(msg_type::start_job), id), exe(exe), num_workers(num_workers),
              partitioner_type(partitioner_type), processor_type(processor_type), sdfs_src_dir(sdfs_src_dir),
              sdfs_output_dir(sdfs_output_dir), num_files_parallel(num_files_parallel), num_appends_parallel(num_appends_parallel) {}
        common_data common;
        std::string exe;
        uint32_t num_workers;
        uint32_t partitioner_type;
        uint32_t processor_type;
        std::string sdfs_src_dir;
        std::string sdfs_output_dir;
        uint32_t num_files_parallel;
        uint32_t num_appends_parallel;
    };

    class not_master : public serializable<not_master, common_data> {
    public:
        not_master() = default;
        not_master(uint32_t id, std::string const& master_node)
            : common(static_cast<uint32_t>(msg_type::not_master), id), master_node(master_node) {}
        common_data common;
        std::string master_node; // When this is set to "", there is no master
    };

    class job_end : public serializable<job_end, common_data> {
    public:
        job_end() = default;
        job_end(uint32_t id, uint32_t succeeded)
            : common(static_cast<uint32_t>(msg_type::job_end), id), succeeded(succeeded) {}
        common_data common;
        uint32_t succeeded;
    };

    class assign_job : public serializable<assign_job, common_data> {
    public:
        assign_job() = default;
        assign_job(uint32_t id, uint32_t job_id, std::string exe, uint32_t processor_type, std::vector<std::string> input_files,
            std::string sdfs_src_dir, std::string sdfs_output_dir, uint32_t num_files_parallel, uint32_t num_appends_parallel)
            : common(static_cast<uint32_t>(msg_type::assign_job), id),
              job_id(job_id), exe(exe), processor_type(processor_type), input_files(input_files), sdfs_src_dir(sdfs_src_dir),
              sdfs_output_dir(sdfs_output_dir), num_files_parallel(num_files_parallel), num_appends_parallel(num_appends_parallel) {}

        common_data common;
        uint32_t job_id;
        std::string exe;
        uint32_t processor_type;
        std::vector<std::string> input_files;
        std::string sdfs_src_dir;
        std::string sdfs_output_dir;
        uint32_t num_files_parallel;
        uint32_t num_appends_parallel;
    };

    class request_append_perm : public serializable<request_append_perm, common_data> {
    public:
        request_append_perm() = default;
        request_append_perm(uint32_t id, uint32_t job_id, std::string const& hostname, std::string const& input_file, std::string const& output_file)
            : common(static_cast<uint32_t>(msg_type::request_append_perm), id), job_id(job_id),
              hostname(hostname), input_file(input_file), output_file(output_file) {}
        common_data common;
        uint32_t job_id;
        std::string hostname; // The hostname of the node sending the message
        std::string input_file;
        std::string output_file;
    };

    class append_perm : public serializable<append_perm, common_data> {
    public:
        append_perm() = default;
        append_perm(uint32_t id, uint32_t allowed)
            : common(static_cast<uint32_t>(msg_type::append_perm), id), allowed(allowed) {}

        common_data common;
        uint32_t allowed;
    };

    class file_done : public serializable<file_done, common_data> {
    public:
        file_done() = default;
        file_done(uint32_t id, uint32_t job_id, std::string const& hostname, std::string const& file)
            : common(static_cast<uint32_t>(msg_type::file_done), id), job_id(job_id), hostname(hostname), file(file) {}
        common_data common;
        uint32_t job_id;
        std::string hostname;
        std::string file;
    };

    class job_failed : public serializable<job_failed, common_data> {
    public:
        job_failed() = default;
        job_failed(uint32_t id, uint32_t job_id)
            : common(static_cast<uint32_t>(msg_type::job_failed), id), job_id(job_id) {}
        common_data common;
        uint32_t job_id;
    };

    class job_end_worker : public serializable<job_end_worker, common_data> {
    public:
        job_end_worker() = default;
        job_end_worker(uint32_t id, uint32_t job_id)
            : common(static_cast<uint32_t>(msg_type::job_end_worker), id), job_id(job_id) {}
        common_data common;
        uint32_t job_id;
    };

    // Metadata type that will be stored along with files in SDFS
    class metadata : public serializable<metadata> {
    public:
        metadata() = default;
        metadata(std::string input_file, uint32_t job_id) : input_file(input_file), job_id(job_id) {}
        std::string input_file;
        uint32_t job_id;
    };
}
