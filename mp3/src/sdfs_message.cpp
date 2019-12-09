#include "sdfs_message.h"
#include "serialization.h"

sdfs_message::sdfs_message(char *buf, unsigned length) {
    // do deserialization here
    deserializer des(buf, length);

    try {
        type = static_cast<msg_type>(des.get_int());

        switch (type) {
            case msg_type::put:
            case msg_type::get:
            case msg_type::gmd:
            case msg_type::del:
            case msg_type::ls:
                sdfs_filename = des.get_string();
                break;
            case msg_type::mn_put:
            case msg_type::mn_get:
            case msg_type::mn_gmd:
                sdfs_hostname = des.get_string();
                break;
            case msg_type::mn_ls:
                data = des.get_string();
                break;
            case msg_type::files:
                sdfs_hostname = des.get_string();
                data = des.get_string();
                break;
            case msg_type::ack:
            case msg_type::fail:
            case msg_type::success:
                break;
            case msg_type::rep:
                sdfs_hostname = des.get_string();
                sdfs_filename = des.get_string();
                break;
            default:
                throw "Unknown type";
        }
    } catch (...) {
        type = msg_type::empty;
        sdfs_filename = "";
        sdfs_hostname = "";
        return;
    }
}

std::string sdfs_message::serialize() {
    // do some serialization here
    if (type == msg_type::empty) {
        return "";
    }

    serializer ser;

    ser.add_field(static_cast<uint32_t>(type));
    switch (type) {
        case msg_type::put:
        case msg_type::get:
        case msg_type::gmd:
        case msg_type::del:
        case msg_type::ls:
            ser.add_field(sdfs_filename);
            break;
        case msg_type::mn_put:
        case msg_type::mn_get:
        case msg_type::mn_gmd:
            ser.add_field(sdfs_hostname);
            break;
        case msg_type::mn_ls:
            ser.add_field(data);
            break;
        case msg_type::files:
            ser.add_field(sdfs_hostname);
            ser.add_field(data);
            break;
        case msg_type::ack:
        case msg_type::fail:
        case msg_type::success:
            break;
        case msg_type::rep:
            ser.add_field(sdfs_hostname);
            ser.add_field(sdfs_filename);
            break;
        default:
            assert(false && "Memory corruption caused msg_type to be invalid");
    }

    return ser.serialize();
}

std::string sdfs_message::get_type_as_string() {
    if (type == msg_type::empty) {
        return "";
    }

    std::string ret = "";
    switch (type) {
        case msg_type::put:
            ret = "put"; break;
        case msg_type::get:
            ret = "get"; break;
        case msg_type::del:
            ret = "del"; break;
        case msg_type::ls:
            ret = "ls"; break;
        case msg_type::ack:
            ret = "ack"; break;
        case msg_type::fail:
            ret = "fail"; break;
        case msg_type::success:
            ret = "success"; break;
        case msg_type::mn_put:
            ret = "mn_put"; break;
        case msg_type::mn_get:
            ret = "mn_get"; break;
        case msg_type::mn_ls:
            ret = "mn_ls"; break;
        default:
           break;
    }

    return ret;
}
