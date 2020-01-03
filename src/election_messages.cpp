#include "election_messages.h"
#include "serialization.h"

using std::unique_ptr;

auto election_message::operator==(const election_message &m) -> bool {
    if (type != m.type)
        return false;

    if (uuid != m.uuid)
        return false;

    switch (type) {
        case proposal:
        case empty: return true;
        case introduction: return master_id == m.master_id;
        case election: return initiator_id == m.initiator_id && vote_id == m.vote_id;
        case elected: return master_id == m.master_id;
        default: assert(false && "Invalid enum value, likely from memory corruption");
    }
}

// Creates a message from a buffer
election_message::election_message(char *buf_, unsigned length_) {
    deserializer des(buf_, length_);

    try {
        id = des.get_int();
        uuid = des.get_int();
        type = static_cast<msg_type>(des.get_int());

        switch (type) {
            case msg_type::election:
                initiator_id = des.get_int();
                vote_id = des.get_int();
                break;
            case msg_type::elected:
            case msg_type::introduction:
                master_id = des.get_int();
                break;
            case msg_type::proposal:
                break;
            default:
                throw "Unknown type";
        }

        des.done();
    } catch (...) {
        // List the entire message in the reason
        type = empty;
        id = 0;
        uuid = 0;
        initiator_id = 0;
        vote_id = 0;
        master_id = 0;
        return;
    }
}

// Serializes the message and returns a string containing the message
auto election_message::serialize() const -> std::string {
    if (type == msg_type::empty) {
        return "";
    }

    serializer ser;

    ser.add_field(id);
    ser.add_field(uuid);
    ser.add_field(static_cast<uint32_t>(type));

    switch (type) {
        case msg_type::election:
            ser.add_field(initiator_id);
            ser.add_field(vote_id);
            break;
        case msg_type::elected:
        case msg_type::introduction:
            ser.add_field(master_id);
            break;
        case msg_type::proposal:
            break;
        default: assert(false && "Memory corruption caused msg_type to be invalid");
    }

    return ser.serialize();
}
