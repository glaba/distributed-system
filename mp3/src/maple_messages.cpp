#include "maple_messages.h"
#include "serialization.h"

maple_message::maple_message(const char *buf, unsigned length) {
    deserializer des(buf, length);

    try {
        msg_type = static_cast<maple_msg_type>(des.get_int());

        switch (msg_type) {
            case START_JOB: {
                maple_start_job_data d;
                d.maple_exe = des.get_string();
                d.num_maples = des.get_int();
                d.sdfs_intermediate_filename_prefix = des.get_string();
                d.sdfs_src_dir = des.get_string();
                data = d;
                break;
            }
            case JOB_END: {
                maple_job_end_data d;
                d.succeeded = des.get_int();
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
    ser.add_field(msg_type);

    switch (msg_type) {
        case START_JOB: {
            maple_start_job_data d = std::get<maple_start_job_data>(data);
            ser.add_field(d.maple_exe);
            ser.add_field(d.num_maples);
            ser.add_field(d.sdfs_intermediate_filename_prefix);
            ser.add_field(d.sdfs_src_dir);
            break;
        }
        case JOB_END: {
            maple_job_end_data d = std::get<maple_job_end_data>(data);
            ser.add_field(d.succeeded);
            break;
        }
        default: assert(false && "Invalid memory type provided, meaning memory corruption has occurred");
    }

    return ser.serialize();
}

void maple_message::set_msg_type() {
    if (std::holds_alternative<maple_start_job_data>(data)) msg_type = START_JOB;
    else assert(false && "Invalid message data provided");
}