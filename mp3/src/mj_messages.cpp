#include "mj_messages.h"
#include "serialization.h"

mj_message::mj_message(const char *buf, unsigned length) {
    deserializer des(buf, length);

    try {
        id = des.get_int();
        msg_type = static_cast<mj_msg_type>(des.get_int());

        switch (msg_type) {
            case START_JOB: {
                mj_start_job d;
                d.exe = des.get_string();
                d.num_workers = des.get_int();
                d.partitioner_type = static_cast<partitioner::type>(des.get_int());
                d.sdfs_src_dir = des.get_string();
                d.outputter_type = static_cast<outputter::type>(des.get_int());
                d.sdfs_output_dir = des.get_string();
                data = d;
                break;
            }
            case NOT_MASTER: {
                mj_not_master d;
                d.master_node = des.get_string();
                data = d;
                break;
            }
            case JOB_END: {
                mj_job_end d;
                d.succeeded = des.get_int();
                data = d;
                break;
            }
            case ASSIGN_JOB: {
                mj_assign_job d;
                d.job_id = des.get_int();
                d.exe = des.get_string();
                d.sdfs_src_dir = des.get_string();
                int num_input_files = des.get_int();
                for (int i = 0; i < num_input_files; i++) {
                    d.input_files.push_back(des.get_string());
                }
                d.outputter_type = static_cast<outputter::type>(des.get_int());
                d.sdfs_output_dir = des.get_string();
                data = d;
                break;
            }
            case REQUEST_APPEND_PERM: {
                mj_request_append_perm d;
                d.job_id = des.get_int();
                d.hostname = des.get_string();
                d.input_file = des.get_string();
                d.output_file = des.get_string();
                data = d;
                break;
            }
            case APPEND_PERM: {
                mj_append_perm d;
                d.allowed = des.get_int();
                data = d;
                break;
            }
            case FILE_DONE: {
                mj_file_done d;
                d.job_id = des.get_int();
                d.hostname = des.get_string();
                d.file = des.get_string();
                data = d;
                break;
            }
            default: throw "Invalid message type";
        }

        des.done();
    } catch (...) {
        msg_type = INVALID;
    }
}

std::string mj_message::serialize() {
    assert(msg_type != INVALID && "Should not be serializing before setting message data");

    serializer ser;
    ser.add_field(id);
    ser.add_field(msg_type);

    switch (msg_type) {
        case START_JOB: {
            mj_start_job d = std::get<mj_start_job>(data);
            ser.add_field(d.exe);
            ser.add_field(d.num_workers);
            ser.add_field(d.partitioner_type);
            ser.add_field(d.sdfs_src_dir);
            ser.add_field(d.outputter_type);
            ser.add_field(d.sdfs_output_dir);
            break;
        }
        case NOT_MASTER: {
            mj_not_master d = std::get<mj_not_master>(data);
            ser.add_field(d.master_node);
            break;
        }
        case JOB_END: {
            mj_job_end d = std::get<mj_job_end>(data);
            ser.add_field(d.succeeded);
            break;
        }
        case ASSIGN_JOB: {
            mj_assign_job d = std::get<mj_assign_job>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.exe);
            ser.add_field(d.sdfs_src_dir);
            ser.add_field(d.input_files.size());
            for (unsigned i = 0; i < d.input_files.size(); i++) {
                ser.add_field(d.input_files[i]);
            }
            ser.add_field(d.outputter_type);
            ser.add_field(d.sdfs_output_dir);
            break;
        }
        case REQUEST_APPEND_PERM: {
            mj_request_append_perm d = std::get<mj_request_append_perm>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.hostname);
            ser.add_field(d.input_file);
            ser.add_field(d.output_file);
            break;
        }
        case APPEND_PERM: {
            mj_append_perm d = std::get<mj_append_perm>(data);
            ser.add_field(d.allowed);
            break;
        }
        case FILE_DONE: {
            mj_file_done d = std::get<mj_file_done>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.hostname);
            ser.add_field(d.file);
            break;
        }
        default: assert(false && "Invalid message type provided, meaning memory corruption has occurred");
    }

    return ser.serialize();
}

void mj_message::set_msg_type() {
    if      (std::holds_alternative<mj_start_job>(data))           msg_type = START_JOB;
    else if (std::holds_alternative<mj_not_master>(data))          msg_type = NOT_MASTER;
    else if (std::holds_alternative<mj_job_end>(data))             msg_type = JOB_END;
    else if (std::holds_alternative<mj_assign_job>(data))          msg_type = ASSIGN_JOB;
    else if (std::holds_alternative<mj_request_append_perm>(data)) msg_type = REQUEST_APPEND_PERM;
    else if (std::holds_alternative<mj_append_perm>(data))         msg_type = APPEND_PERM;
    else if (std::holds_alternative<mj_file_done>(data))           msg_type = FILE_DONE;
    else assert(false && "Invalid message data provided");
}