#include "election_messages.h"
#include "serialization.h"

#include <cassert>

bool election_message::operator==(const election_message &m) {
    if (type != m.type)
        return false;

    switch (type) {
        case empty: return true;
        case introduction: return master_id == m.master_id;
        case election: return initiator_id == m.initiator_id && vote_id == m.vote_id;
        case elected: return master_id == m.master_id;
        default: assert(false && "Invalid enum value, likely from memory corruption");
    }
}

// Creates a message from a buffer
election_message::election_message(char *buf_, unsigned length_) {
    char *buf = buf_;
    unsigned length = length_;

    // Reconstruct ID in little endian order
    if (length < sizeof(id)) {malformed_reason = "Message ends before ID"; goto malformed_msg;}
    id = serialization::read_uint32_from_char_buf(buf);
    buf += sizeof(id); length -= sizeof(id);

    // Get the type of the message
    if (length < sizeof(uint8_t)) {malformed_reason = "Message ends before message type"; goto malformed_msg;}
    switch (*buf) {
        case 0: type = msg_type::election; break;
        case 1: type = msg_type::elected; break;
        case 2: type = msg_type::introduction; break;
        default: {malformed_reason = "Message type is neither election or elected"; goto malformed_msg;}
    }
    buf += sizeof(uint8_t); length -= sizeof(uint8_t);

    switch (type) {
        case msg_type::election:
            // Read the initiator ID first
            if (length < sizeof(initiator_id)) {malformed_reason = "Message ends before initiator ID"; goto malformed_msg;}
            initiator_id = serialization::read_uint32_from_char_buf(buf);
            buf += sizeof(initiator_id); length -= sizeof(initiator_id);

            // Then read the vote ID first
            if (length < sizeof(vote_id)) {malformed_reason = "Message ends before vote ID"; goto malformed_msg;}
            vote_id = serialization::read_uint32_from_char_buf(buf);
            buf += sizeof(vote_id); length -= sizeof(vote_id);
            break;
        case msg_type::elected:
        case msg_type::introduction:
            // Read the master ID
            if (length < sizeof(master_id)) {malformed_reason = "Message ends before master ID"; goto malformed_msg;}
            master_id = serialization::read_uint32_from_char_buf(buf);
            buf += sizeof(master_id); length -= sizeof(master_id);
            break;
        default: goto malformed_msg;
    }

    // Make sure that the message is fully consumed
    if (length != 0) {
        malformed_reason = "Message was not fully consumed, meaning it is invalid";
        goto malformed_msg;
    }

    return;

malformed_msg:
    // List the entire message in the reason
    malformed_reason += ", message was: ";
    for (unsigned i = 0; i < length_; i++) {
        malformed_reason += std::to_string(buf_[i]) + " ";
    }

    type = empty;
    initiator_id = 0;
    vote_id = 0;
    master_id = 0;
    return;
}

// Serializes the message and returns a buffer containing the message, along with the length
char *election_message::serialize(unsigned &length) {
    if (type == msg_type::empty) {
        length = 0;
        return nullptr;
    }

    length = sizeof(uint32_t) + // Node ID
             sizeof(uint8_t) + // Message type
             ((type == msg_type::election) ? (2 * sizeof(uint32_t)) : sizeof(uint32_t)); // Data

    char *buf = new char[length];
    char *original_buf = buf;
    unsigned ind = 0;

    // Write our ID to the message
    if (ind + sizeof(uint32_t) > length) goto fail;
    serialization::write_uint32_to_char_buf(id, buf + ind);
    ind += sizeof(uint32_t);

    // Write the message type
    if (ind + sizeof(uint8_t) > length) goto fail;
    switch (type) {
        case msg_type::election: buf[ind] = 0; break;
        case msg_type::elected: buf[ind] = 1; break;
        case msg_type::introduction: buf[ind] = 2; break;
        default: assert(false);
    }
    ind += sizeof(uint8_t);

    switch (type) {
        case msg_type::election:
            // Write the initiator ID
            if (ind + sizeof(uint32_t) > length) goto fail;
            serialization::write_uint32_to_char_buf(initiator_id, buf + ind);
            ind += sizeof(uint32_t);

            // Write the vote ID
            if (ind + sizeof(uint32_t) > length) goto fail;
            serialization::write_uint32_to_char_buf(vote_id, buf + ind);
            ind += sizeof(uint32_t);
            break;
        case msg_type::elected:
        case msg_type::introduction:
            // Write the master ID
            if (ind + sizeof(uint32_t) > length) goto fail;
            serialization::write_uint32_to_char_buf(master_id, buf + ind);
            ind += sizeof(uint32_t);
            break;
        default: assert(false && "Memory corruption has occurred and type is invalid enum value");
    }

    return original_buf;

fail:
    assert(false && "Message serialization led to memory corruption");
}