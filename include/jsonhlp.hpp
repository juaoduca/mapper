// jsonhelper.hpp

#pragma once

// Centralize all necessary RapidJSON headers
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include "lib.hpp"

namespace json = rapidjson;
using jdoc = json::Document;
using jval = json::Value;
using jit = rapidjson::Value::ConstMemberIterator;
using jalloc = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>;
using jdaloc = rapidjson::Document::AllocatorType;


// A namespace to keep our helper functions organized
namespace jhlp {


    // Helper function to safely parse a JSON string into a RapidJSON Document.
    // It returns true on success and prints an error message on failure.
    inline bool parse_str(const std::string& json_string, rapidjson::Document& document) {
        document.Parse(json_string.c_str());
        // jval::IsNumber
        if (document.HasParseError()) {
            std::cerr << "JSON Parse Error: " << rapidjson::GetParseErrorFunc(document.GetParseError())
                      << " at offset " << document.GetErrorOffset() << std::endl;
            return false;
        }
        return true;
    }

    // Helper function to safely parse a JSON file into a RapidJSON Document.
    inline bool parse_file(const std::string& file_path, rapidjson::Document& document) {
        std::ifstream ifs(file_path);
        if (!ifs.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return false;
        }
        rapidjson::IStreamWrapper isw(ifs);
        document.ParseStream(isw);
        if (document.HasParseError()) {
            std::cerr << "JSON Parse Error in file " << file_path << ": "
                      << rapidjson::GetParseErrorFunc(document.GetParseError())
                      << " at offset " << document.GetErrorOffset() << std::endl;
            return false;
        }
        return true;
    }

    // Helper function to stringify a RapidJSON Document into a std::string.
    inline std::string stringify(const rapidjson::Document& document) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return buffer.GetString();
    }

    inline const jval& first_obj(const jval& value) {
        if (value.IsArray()) {
            if (value.Empty()) THROW("JSON array is empty");
            const jval& val = value.MemberBegin()->value;
            if (!val.IsObject()) THROW("First array element is not an object");
            return val;
        }
        if (!value.IsObject()) THROW("JSON must be an object or array of objects");
        return value;
    }

    // Utility to convert any Value to string
    inline std::string val2str(const rapidjson::Value& value) {
        if (value.IsNumber()) {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return buffer.GetString();
        } else if (value.IsString()) {
            return value.GetString();
        } else if (value.IsBool()) {
            return value.GetBool() ? "true" : "false";
        } else if (value.IsNull()) {
            return "null";
        }
        // Handle other types (object, array) as needed
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return buffer.GetString();
    }

    // --- Helper functions for element access (mimicking nlohmann/json) ---

    // Template helper to get a value from a RapidJSON Value/Document.
    // This function will check for existence and type before returning the value.
    // It returns a default value if the key is not found or the type is incorrect.
    template<typename T>
    inline T get(const rapidjson::Value& parent, const std::string& key, const T& default_value = T()) {

        if (!parent.IsObject() || !parent.HasMember(key.c_str())) { return default_value; }
        const jval& val = parent.FindMember(key.c_str())->value;
        if constexpr (std::is_same_v<T, std::string>) {
            if (val.IsString()) return val.GetString();
            if (val.IsNumber()) return val2str(val);
        } else if constexpr (std::is_same_v<T, int>) {
            if (val.IsInt()) return val.GetInt();
        } else if constexpr (std::is_same_v<T, int64_t>) {
            if (val.IsInt64()) return val.GetInt64();
        } else if constexpr (std::is_same_v<T, double>){
            if (val.IsFloat() || val.IsDouble()) return val.GetDouble();
        } else if constexpr (std::is_same_v<T, bool>) {
            if (val.IsBool()) return val.GetBool();
        } else if constexpr (std::is_same_v<T, uint>) {
            if (val.IsUint()) return val.GetUint();
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            if (val.IsUint64()) return val.GetUint64();
        }
        return default_value;
    }

    // Template helper to set a value in a RapidJSON Document or Value.
    template<typename T>
    inline void set(rapidjson::Document& document, const std::string& key, const T& value) {
        rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
        if constexpr (std::is_same_v<T, std::string>) {
            document.AddMember(rapidjson::Value(key.c_str(), allocator).Move(),
                               rapidjson::Value(value.c_str(), allocator).Move(),
                               allocator);
        } else if constexpr (std::is_same_v<T, int64_t>){
            document.AddMember<int64_t>(rapidjson::Value(key.c_str(), allocator).Move(),
                               value,
                               allocator);
        } else {
            document.AddMember(rapidjson::Value(key.c_str(), allocator).Move(),
                               value,
                               allocator);
        }
    }

    // Overload for setting values in a nested object.
    template<typename T>
    inline void set(rapidjson::Value& parent, const std::string& key, const T& value, rapidjson::Document::AllocatorType& allocator) {
        if constexpr (std::is_same_v<T, std::string>) {
            parent.AddMember(rapidjson::Value(key.c_str(), allocator).Move(),
                             rapidjson::Value(value.c_str(), allocator).Move(),
                             allocator);
        } else {
            parent.AddMember(rapidjson::Value(key.c_str(), allocator).Move(),
                             value,
                             allocator);
        }
    }

    inline std::string dump(const rapidjson::Value& value, bool pretty = false) {
        // Create a StringBuffer to hold the output
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (value.IsNumber()) {
            value.Accept(writer);
            return buffer.GetString();
        } else if (value.IsString()) {
            return value.GetString();
        } else if (value.IsBool()) {
            return value.GetBool() ? "true" : "false";
        } else if (value.IsNull()) {
            return "null";
        }
        value.Accept(writer);
        return buffer.GetString();
    }

} // namespace json_helper
