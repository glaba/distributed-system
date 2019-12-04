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
            case msg_type::del:
            case msg_type::ls:
                sdfs_filename = des.get_string();
                break;
            case msg_type::mn_put:
            case msg_type::mn_get:
                sdfs_hostname = des.get_string();
                break;
            case msg_type::ack:
            case msg_type::fail:
            case msg_type::success:
                break;
            default:
                throw "Unknown type";
        }
    } catch (...) {
        type = msg_type::empty;
        sdfs_filename = "";
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
        case msg_type::del:
        case msg_type::ls:
            ser.add_field(sdfs_filename);
            break;
        case msg_type::mn_put:
        case msg_type::mn_get:
            ser.add_field(sdfs_hostname);
            break;
        case msg_type::ack:
        case msg_type::fail:
        case msg_type::success:
            break;
        default:
            assert(false && "Memory corruption caused msg_type to be invalid");
    }

    return ser.serialize();
}
