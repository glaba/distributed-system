#include "maple_messages.h"
#include "serialization.h"

maple_message::maple_message(const char *buf, unsigned length) {
    deserializer des(buf, length);

    try {
        id = des.get_int();
        msg_type = static_cast<maple_msg_type>(des.get_int());

        switch (msg_type) {
            case START_JOB: {
                maple_start_job d;
                d.maple_exe = des.get_string();
                d.num_maples = des.get_int();
                d.sdfs_intermediate_filename_prefix = des.get_string();
                d.sdfs_src_dir = des.get_string();
                data = d;
                break;
            }
            case NOT_MASTER: {
                maple_not_master d;
                d.master_node = des.get_string();
                data = d;
                break;
            }
            case JOB_END: {
                maple_job_end d;
                d.succeeded = des.get_int();
                data = d;
                break;
            }
            case ASSIGN_JOB: {
                maple_assign_job d;
                d.job_id = des.get_int();
                d.maple_exe = des.get_string();
                int num_input_files = des.get_int();
                for (int i = 0; i < num_input_files; i++) {
                    d.input_files.push_back(des.get_string());
                }
                d.sdfs_intermediate_filename_prefix = des.get_string();
                data = d;
                break;
            }
            case REQUEST_APPEND_PERM: {
                maple_request_append_perm d;
                d.job_id = des.get_int();
                d.hostname = des.get_string();
                d.file = des.get_string();
                d.key = des.get_string();
                data = d;
                break;
            }
            case APPEND_PERM: {
                maple_append_perm d;
                d.allowed = des.get_int();
                data = d;
                break;
            }
            case FILE_DONE: {
                maple_file_done d;
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

std::string maple_message::serialize() {
    assert(msg_type != INVALID && "Should not be serializing before setting message data");

    serializer ser;
    ser.add_field(id);
    ser.add_field(msg_type);

    switch (msg_type) {
        case START_JOB: {
            maple_start_job d = std::get<maple_start_job>(data);
            ser.add_field(d.maple_exe);
            ser.add_field(d.num_maples);
            ser.add_field(d.sdfs_intermediate_filename_prefix);
            ser.add_field(d.sdfs_src_dir);
            break;
        }
        case NOT_MASTER: {
            maple_not_master d = std::get<maple_not_master>(data);
            ser.add_field(d.master_node);
            break;
        }
        case JOB_END: {
            maple_job_end d = std::get<maple_job_end>(data);
            ser.add_field(d.succeeded);
            break;
        }
        case ASSIGN_JOB: {
            maple_assign_job d = std::get<maple_assign_job>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.maple_exe);
            ser.add_field(d.input_files.size());
            for (unsigned i = 0; i < d.input_files.size(); i++) {
                ser.add_field(d.input_files[i]);
            }
            ser.add_field(d.sdfs_intermediate_filename_prefix);
            break;
        }
        case REQUEST_APPEND_PERM: {
            maple_request_append_perm d = std::get<maple_request_append_perm>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.hostname);
            ser.add_field(d.file);
            ser.add_field(d.key);
            break;
        }
        case APPEND_PERM: {
            maple_append_perm d = std::get<maple_append_perm>(data);
            ser.add_field(d.allowed);
            break;
        }
        case FILE_DONE: {
            maple_file_done d = std::get<maple_file_done>(data);
            ser.add_field(d.job_id);
            ser.add_field(d.hostname);
            ser.add_field(d.file);
            break;
        }
        default: assert(false && "Invalid message type provided, meaning memory corruption has occurred");
    }

    return ser.serialize();
}

void maple_message::set_msg_type() {
    if      (std::holds_alternative<maple_start_job>(data))           msg_type = START_JOB;
    else if (std::holds_alternative<maple_not_master>(data))          msg_type = NOT_MASTER;
    else if (std::holds_alternative<maple_job_end>(data))             msg_type = JOB_END;
    else if (std::holds_alternative<maple_assign_job>(data))          msg_type = ASSIGN_JOB;
    else if (std::holds_alternative<maple_request_append_perm>(data)) msg_type = REQUEST_APPEND_PERM;
    else if (std::holds_alternative<maple_append_perm>(data))         msg_type = APPEND_PERM;
    else if (std::holds_alternative<maple_file_done>(data))           msg_type = FILE_DONE;
    else assert(false && "Invalid message data provided");
}