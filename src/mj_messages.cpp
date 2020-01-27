#include "mj_messages.h"

namespace mj_message {
    register_serializable<common_data> register_common_data([] (auto &registry) {
        registry.register_fields(&common_data::type, &common_data::id);
    });
    register_serializable<start_job, common_data> register_start_job([] (auto &registry) {
        registry.register_fields(&start_job::common, &start_job::exe, &start_job::num_workers, &start_job::partitioner_type, &start_job::sdfs_src_dir,
            &start_job::processor_type, &start_job::sdfs_output_dir, &start_job::num_files_parallel, &start_job::num_appends_parallel);
    });
    register_serializable<not_master, common_data> register_not_master([] (auto &registry) {
        registry.register_fields(&not_master::common, &not_master::master_node);
    });
    register_serializable<job_end, common_data> register_job_end([] (auto &registry) {
        registry.register_fields(&job_end::common, &job_end::succeeded);
    });
    register_serializable<assign_job, common_data> register_assign_job([] (auto &registry) {
        registry.register_fields(&assign_job::common, &assign_job::job_id, &assign_job::exe, &assign_job::sdfs_src_dir, &assign_job::input_files,
            &assign_job::processor_type, &assign_job::sdfs_output_dir, &assign_job::num_files_parallel, &assign_job::num_appends_parallel);
    });
    register_serializable<request_append_perm, common_data> register_request_append_perm([] (auto &registry) {
        registry.register_fields(&request_append_perm::common, &request_append_perm::job_id, &request_append_perm::hostname,
            &request_append_perm::input_file, &request_append_perm::output_file);
    });
    register_serializable<append_perm, common_data> register_append_perm([] (auto &registry) {
        registry.register_fields(&append_perm::common, &append_perm::allowed);
    });
    register_serializable<file_done, common_data> register_file_done([] (auto &registry) {
        registry.register_fields(&file_done::common, &file_done::job_id, &file_done::hostname, &file_done::file);
    });
    register_serializable<job_failed, common_data> register_job_failed([] (auto &registry) {
        registry.register_fields(&job_failed::common, &job_failed::job_id);
    });
    register_serializable<job_end_worker, common_data> register_job_end_worker([] (auto &registry) {
        registry.register_fields(&job_end_worker::common, &job_end_worker::job_id);
    });

    // Register metadata type which will be stored along with files in SDFS
    register_serializable<metadata> register_metadata([] (auto &registry) {
        registry.register_fields(&metadata::input_file, &metadata::job_id);
    });
}