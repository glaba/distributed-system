#pragma once

#include "overloaded.h"

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <type_traits>

#include <cxxabi.h>
#include <cstring> // TODO: Delete

#define MAX_DESERIALIZABLE_STRING_LEN 1024

// Helper functions for serialization
namespace serialization {
    void write_uint32_to_char_buf(uint32_t n, char *buf);
    auto write_uint32_to_string(uint32_t n) -> std::string;
    auto read_uint32_from_char_buf(char const* buf) -> uint32_t;
}

class serializer {
public:
    using field = std::variant<uint32_t, std::string>;

    void add_field(field const& f);
    auto serialize() const -> std::string;

private:
    std::vector<field> fields;
};

class deserializer {
public:
    deserializer(char const* buf, unsigned length);
    auto get_int() -> uint32_t;
    auto get_string() -> std::string;
    void done();

private:
    char const* buf;
    unsigned length, pos;
};

template <typename T, typename... SerializableMembers>
class register_serializable;

template <typename T, typename... SerializableMembers>
class serializable {
public:
    using field_ptr = std::variant<
        uint32_t T::*, std::string T::*,
        std::vector<uint32_t> T::*, std::vector<std::string> T::*,
        SerializableMembers T::*...,
        std::vector<SerializableMembers> T::*...>;

    static uint32_t get_serializable_id() {
        static uint32_t id = static_cast<uint32_t>(typeid(T).hash_code());
        return id;
    }

private:

    mutable serializer *ser_;
    mutable deserializer *des_;

    // Helper functions which generate lambdas to insert and extract different types from serializer and deserializer
    template <typename FieldType>
    inline auto add_value() const -> std::function<void(FieldType T::*)> {
        return [&] (FieldType T::* offset) {
            ser_->add_field(static_cast<T const*>(this)->*offset);
        };
    };

    template <typename FieldType>
    inline auto add_vector() const -> std::function<void(std::vector<FieldType> T::*)> {
        return [&] (std::vector<FieldType> T::* offset) {
            auto self = static_cast<T const*>(this);
            ser_->add_field((self->*offset).size());
            for (unsigned i = 0; i < (self->*offset).size(); i++) {
                ser_->add_field((self->*offset)[i]);
            }
        };
    }

    template <typename FieldType>
    inline auto add_struct() const -> std::function<void(FieldType T::*)> {
        return [&] (FieldType T::* offset) {
            // Add the struct serialized to a string (adds 4 byte overhead)
            std::string val = (static_cast<T const*>(this)->*offset).serialize();
            ser_->add_field(val);
        };
    }

    template <typename FieldType>
    inline auto add_struct_vector() const -> std::function<void(std::vector<FieldType> T::*)> {
        return [&] (std::vector<FieldType> T::* offset) {
            auto self = const_cast<T*>(static_cast<T const*>(this));
            ser_->add_field((self->*offset).size());
            for (unsigned i = 0; i < (self->*offset).size(); i++) {
                // Add the struct serialized to a string
                ser_->add_field((self->*offset)[i].serialize());
            }
        };
    }

    template <typename FieldType>
    inline auto get_value() const -> std::function<void(FieldType T::*)> {
        return [&] (FieldType T::* offset) {
            auto self = const_cast<T*>(static_cast<T const*>(this));
            if constexpr (std::is_same<FieldType, uint32_t>::value) {
                self->*offset = des_->get_int();
            } else if constexpr (std::is_same<FieldType, std::string>::value) {
                self->*offset = des_->get_string();
            }
        };
    }

    template <typename FieldType>
    inline auto get_vector() const -> std::function<void(std::vector<FieldType> T::*)> {
        return [&] (std::vector<FieldType> T::* offset) {
            auto self = const_cast<T*>(static_cast<T const*>(this));

            unsigned size = des_->get_int();
            (self->*offset).clear();

            for (unsigned i = 0; i < size; i++) {
                if constexpr (std::is_same<FieldType, uint32_t>::value) {
                    (self->*offset).push_back(des_->get_int());
                } else if constexpr (std::is_same<FieldType, std::string>::value) {
                    (self->*offset).push_back(des_->get_string());
                }
            }
        };
    }

    template <typename FieldType>
    inline auto get_struct() const -> std::function<void(FieldType T::*)> {
        return [&] (FieldType T::* offset) {
            auto self = const_cast<T*>(static_cast<T const*>(this));
            std::string data = des_->get_string();
            if (!(self->*offset).deserialize_from(data)) {
                throw "Could not deserialize member struct";
            }
        };
    }

    template <typename FieldType>
    inline auto get_struct_vector() const -> std::function<void(std::vector<FieldType> T::*)> {
        return [&] (std::vector<FieldType> T::* offset) {
            auto self = const_cast<T*>(static_cast<T const*>(this));

            unsigned size = des_->get_int();
            (self->*offset).clear();

            for (unsigned i = 0; i < size; i++) {
                FieldType f;
                if (!f.deserialize_from(des_->get_string())) {
                    throw "Could not deserialize member vector of structs";
                }
                (self->*offset).push_back(f);
            }
        };
    }

public:
    auto serialize(bool include_id = false) const -> std::string {
        serializer ser;
        ser_ = &ser;
        for (auto const& field : field_registry()) {
            std::visit(overloaded {
                add_value<uint32_t>(), add_value<std::string>(),
                add_vector<uint32_t>(), add_vector<std::string>(),
                add_struct<SerializableMembers>()...,
                add_struct_vector<SerializableMembers>()...
            }, field);
        }
        std::string retval = ser.serialize();
        return retval;
    }

    auto deserialize_from(std::string s, bool ignore_extra = false) -> bool {
        try {
            deserializer des(s.c_str(), s.length());
            des_ = &des;
            for (auto const& field : field_registry()) {
                std::visit(overloaded {
                    get_value<uint32_t>(), get_value<std::string>(),
                    get_vector<uint32_t>(), get_vector<std::string>(),
                    get_struct<SerializableMembers>()...,
                    get_struct_vector<SerializableMembers>()...
                }, field);
            }
            if (!ignore_extra) {
                des.done();
            }
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    // Returns a reference to the field_registry, a list of pointers to members of T to be serialized
    static auto field_registry() -> std::vector<field_ptr>& {
        static std::vector<field_ptr> field_registry_inst;
        return field_registry_inst;
    }

    template <typename T_, typename... SerializableMembers_>
    friend class register_serializable;
};

template <typename T, typename... SerializableMembers>
class register_serializable {
public:
    register_serializable(std::function<void(register_serializable&)> reg) {
        reg(*this);
    }

    template <typename... Rest>
    inline void register_fields(typename serializable<T, SerializableMembers...>::field_ptr ptr, Rest... rest) {
        serializable<T, SerializableMembers...>::field_registry().push_back(ptr);
        register_fields(rest...);
    }

    inline void register_fields() {}
};
