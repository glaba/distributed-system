#include "messages.h"

#include <iostream>
#include <cassert>

inline uint32_t read_uint32_from_char_buf(char *buf) {
	uint32_t retval = 0;
	retval += (buf[0] & 0xFF);
	retval += (buf[1] & 0xFF) << 8;
	retval += (buf[2] & 0xFF) << 16;
	retval += (buf[3] & 0xFF) << 24;
	return retval;
}

inline void write_uint32_to_char_buf(uint32_t n, char *buf) {
	buf[0] = (n & (0xFF << 0)) >> 0;
	buf[1] = (n & (0xFF << 8)) >> 8;
	buf[2] = (n & (0xFF << 16)) >> 16; 
	buf[3] = (n & (0xFF << 24)) >> 24;
}

// Creates a message from a buffer
message::message(char *buf_, unsigned length_) {
	char *buf = buf_;
	unsigned length = length_;

	// Reconstruct ID in little endian order
	if (length < sizeof(id)) goto malformed_msg;
	id = read_uint32_from_char_buf(buf);
	buf += sizeof(id); length -= sizeof(id);

	// Loop through the three fields: L, L, J (loss/fail, leave, join)
	for (int i = 0; i < 3; i++) {
		// Consume the character specifying the current list of nodes
		char type;
		if (length < sizeof(type)) {malformed_reason = "Message ends before type"; goto malformed_msg;}
		type = *buf;
		buf += sizeof(type); length -= sizeof(type);

		// Verify that the correct character is there
		switch (i) {
			case 0: if (type != 'L') {malformed_reason = "Missing L (loss)"; goto malformed_msg;} else break;
			case 1: if (type != 'L') {malformed_reason = "Missing L (leave)"; goto malformed_msg;} else break;
			case 2: if (type != 'J') {malformed_reason = "Missing J (join)"; goto malformed_msg;} else break;
			default: break;
		}

		// Get the number of entries in the current list
		uint32_t num_entries;

		if (length < sizeof(num_entries)) {malformed_reason = "Message ends before number of entries"; goto malformed_msg;}
		num_entries = read_uint32_from_char_buf(buf);
		buf += sizeof(num_entries); length -= sizeof(num_entries);

		for (unsigned j = 0; j < num_entries; j++) {
			std::string hostname;
			uint32_t id;

			// For joins, we also have to get the hostname 
			if (i == 2) {
				// First, get the length of the string
				uint8_t hostname_len;
				if (length < sizeof(hostname_len)) {malformed_reason = "Message ends before hostname length"; goto malformed_msg;}
				hostname_len = *reinterpret_cast<uint8_t*>(buf);
				buf += sizeof(hostname_len); length -= sizeof(hostname_len);

				// Then read the actual string
				for (unsigned k = 0; k < hostname_len; k++) {
					if (length < sizeof(char)) {malformed_reason = "Message ends before complete hostname"; goto malformed_msg;}
					hostname += *buf;
					buf += sizeof(char); length -= sizeof(char);
				}
			}

			// Then, there is an id for all the message types
			if (length < sizeof(id)) {malformed_reason = "Message ends before ID"; goto malformed_msg;}
			id = read_uint32_from_char_buf(buf);
			buf += sizeof(id); length -= sizeof(id);

			// Add the entry to one of the lists
			switch (i) {
				case 0: failed_nodes.push_back(id); break;
				case 1: left_nodes.push_back(id); break;
				case 2: {
					member m;
					m.id = id;
					m.hostname = hostname;
					joined_nodes.push_back(m);
					break;
				}
				default: break;
			}
		}
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

	id = 0;
	failed_nodes.clear();
	left_nodes.clear();
	joined_nodes.clear();
	return;
}

// Sets the list of failed nodes to the given list of nodes
void message::set_failed_nodes(std::vector<uint32_t> nodes) {
	failed_nodes = nodes;
}

// Sets the list of joined nodes to the given list of nodes
void message::set_joined_nodes(std::vector<member> nodes) {
	joined_nodes = nodes;
}

// Sets the list of left nodes to the given list of nodes
void message::set_left_nodes(std::vector<uint32_t> nodes) {
	left_nodes = nodes;
}

// Returns true if the deserialized message is not malformed
bool message::is_well_formed() {
	return id != 0;
}

// Gets the ID of the node that produced the message
uint32_t message::get_id() {
	return id;
}

// Gets the list of nodes that failed
std::vector<uint32_t> message::get_failed_nodes() {
	return failed_nodes;
}

// Gets the list of nodes that left
std::vector<uint32_t> message::get_left_nodes() {
	return left_nodes;
}

// Gets the list of nodes that joined
std::vector<member> message::get_joined_nodes() {
	return joined_nodes;
}

// Serializes the message and returns a buffer containing the message, along with the length
char *message::serialize(unsigned &length) {
	length = sizeof(uint32_t) + // Node ID
	          sizeof(char) + sizeof(uint32_t) + failed_nodes.size() * sizeof(uint32_t) + // Failed nodes
	          sizeof(char) + sizeof(uint32_t) + left_nodes.size() * sizeof(uint32_t) + // Left nodes
	          sizeof(char) + sizeof(uint32_t); // Beginning of joined nodes
	for (auto &m : joined_nodes) {
		length += sizeof(uint8_t); // Hostname length
		length += m.hostname.length(); // Actual hostname
		length += sizeof(uint32_t); // ID of hostname
	}

	char *buf = new char[length];
	char *original_buf = buf;
	unsigned ind = 0;

	if (ind + sizeof(uint32_t) > length) goto fail;
	write_uint32_to_char_buf(id, buf + ind);
	ind += sizeof(uint32_t);

	// As in deserialization, 0 = failed nodes, 1 = left nodes, 2 = joined nodes
	for (int j = 0; j < 3; j++) {
		if (ind + sizeof(char) > length) goto fail;
		switch (j) {
			case 0: buf[ind] = 'L'; break;
			case 1: buf[ind] = 'L'; break;
			case 2: buf[ind] = 'J'; break;
			default: break;
		}
		ind += sizeof(char);

		// For the rest of the types of messages, there is a length field
		uint32_t num_entries;
		switch (j) {
			case 0: num_entries = failed_nodes.size(); break;
			case 1: num_entries = left_nodes.size(); break;
			case 2: num_entries = joined_nodes.size(); break;
			default: num_entries = 0; break;
		}
		if (ind + sizeof(uint32_t) > length) goto fail;
		write_uint32_to_char_buf(num_entries, buf + ind);
		ind += sizeof(uint32_t);

		// For a fail or leave message, just spit out the list of IDs
		if (j == 0 || j == 1) {
			std::vector<uint32_t> *id_vec = (j == 0) ? &failed_nodes : &left_nodes;

			for (unsigned i = 0; i < id_vec->size(); i++) {
				if (ind + sizeof(uint32_t) > length) goto fail;
				write_uint32_to_char_buf((*id_vec)[i], buf + ind);
				ind += sizeof(uint32_t);
			}
		}

		// For a join message, we need to include the hostname as well
		if (j == 2) {
			for (unsigned i = 0; i < joined_nodes.size(); i++) {
				// Write the length of the hostname
				if (ind + sizeof(uint8_t) > length) goto fail;
				buf[ind] = static_cast<uint8_t>(joined_nodes[i].hostname.length());
				ind += sizeof(uint8_t);

				// Write the actual hostname
				for (unsigned j = 0; j < joined_nodes[i].hostname.length(); j++) {
					if (ind + sizeof(char) > length) goto fail;
					buf[ind] = joined_nodes[i].hostname[j];
					ind += sizeof(char);
				}

				// Write the ID of the host
				if (ind + sizeof(uint32_t) > length) goto fail;
				write_uint32_to_char_buf(joined_nodes[i].id, buf + ind);
				ind += sizeof(uint32_t);
			}
		}
	}

	if (ind != length)
		goto fail;

	return original_buf;

fail:
	std::cout << "Message serialization failed -- exiting" << std::endl;
	exit(1);
}
