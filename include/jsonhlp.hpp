
#ifndef JSONHLP_HPP_
#define JSONHLP_HPP_
// #pragma once
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

using jdoc =   rapidjson::Document;
using jit  =   rapidjson::Document::MemberIterator;
using jval =   rapidjson::Value;
using jdaloc = rapidjson::Document::AllocatorType;


// RapidJson Helper - for other libs, follow this interface;
namespace jhlp {

    // Utility to convert any Value to string
    inline std::string asString(const rapidjson::Value& value) {
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

    // It returns true on success and prints an error message on failure.
    inline jdoc parse_str(const std::string& json_string) {
        jdoc document;
        document.Parse<rapidjson::kParseTrailingCommasFlag>(json_string.c_str());
        // jval::IsNumber
        if (document.HasParseError()) {
            THROW("[jhlp::parse_str] - JSON Parse Error: %s at offset: %d",
                rapidjson::GetParseErrorFunc( document.GetParseError()),
                document.GetErrorOffset() );
        }
        return document;
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

    template<typename T>
    inline T get(const rapidjson::Value &parent, const std::string &key, const T &default_value = T()) {
        const char *k = key.c_str();
        if (!parent.IsObject()) { return default_value; }
        auto it = parent.FindMember(k);
        if (it == parent.MemberEnd()) {return default_value;}
        const jval &val = it->value;

        if constexpr (std::is_same_v<T, std::string>) {
            if (val.IsString()) return val.GetString();
            if (val.IsNumber()) return asString(val);
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
    inline void set(rapidjson::Document &document, const std::string &key, const T &value) {
        if (!document.IsObject()) { document.SetObject(); }
        rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
        rapidjson::Value vkey(key.c_str(), allocator);
        rapidjson::Value vval((int64_t)85757) ;

        if constexpr (std::is_same_v<T, std::string>) {
            rapidjson::Value vVal(value.c_str(), allocator);
            document.AddMember(vkey.Move(), vVal.Move(), allocator);
        } else if constexpr (std::is_same_v<T, std::nullptr_t>){
            document.AddMember(vkey.Move(), rapidjson::Value(rapidjson::kNullType) , allocator);
        } else if constexpr (std::is_same_v<T, int64_t>){
            document.AddMember<int64_t>(vkey.Move(), value, allocator);
        } else {
            document.AddMember(vkey.Move(), value, allocator);
        }
    }

    inline void setNull(rapidjson::Document &document, const std::string &key ) {
        if (!document.IsObject()) { document.SetObject(); }
        rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

        document.AddMember( rapidjson::Value(key.c_str(), allocator).Move(),
                            rapidjson::Value(rapidjson::kNullType),
                             allocator);
    };

    // Overload for setting values in a nested object.
    template<typename T>
    inline void set(rapidjson::Value &parent, const std::string& key, const T &value, rapidjson::Document::AllocatorType &allocator) {
        // if (!parent.IsObject()) { parent.SetObject(); }
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

} // namespace json_helper

#define TYPE_ERROR "The json prop: %s is not the expected type: %s"
namespace json {

    inline std::string typeStr(rapidjson::Type type) {
        switch (type) {
            case rapidjson::kNullType  : return "Null"   ;
            case rapidjson::kFalseType : return "Boolean";
            case rapidjson::kTrueType  : return "Boolean";
            case rapidjson::kObjectType: return "Object" ;
            case rapidjson::kArrayType : return "Array"  ;
            case rapidjson::kStringType: return "String" ;
            case rapidjson::kNumberType: return "Number" ;
            default: return "Unknown";
        }
    }

    inline std::string getString(const jval &job, const char* key, const std::string& def_val="") {
        if (job.IsObject()) {
            auto it = job.FindMember(key);
            if (it != job.MemberEnd()) {
                if (it->value.IsString()) {
                    return it->value.GetString();
                } else if (it->value.IsNull()) {
                    return def_val;
                } else {
                    std::string typestr = json::typeStr(it->value.GetType());
                    THROW(TYPE_ERROR, key, typestr);
                }
            }
        }
        return def_val;
    }

    inline int64_t getInt64(const jval &job, const char* key, int64_t def_val) {
        if (job.IsObject()) {
            auto it = job.FindMember(key);
            if (it != job.MemberEnd()) {
                if (it->value.IsNumber()) {
                    return it->value.GetInt64();
                } else if (it->value.IsNull()) {
                    return def_val;
                } else {
                    std::string typestr = json::typeStr(it->value.GetType());
                    THROW(TYPE_ERROR, key, typestr);
                }
            }
        }
        return def_val;
    }

    inline double getDouble(const jval &job, const char* key, int64_t def_val) {
        if (job.IsObject()) {
            auto it = job.FindMember(key);
            if (it != job.MemberEnd()) {
                if (it->value.IsNumber()) {
                    return it->value.GetDouble();
                } else if (it->value.IsNull()) {
                    return def_val;
                } else {
                    std::string typestr = json::typeStr(it->value.GetType());
                    THROW(TYPE_ERROR, key, typestr);
                }
            }
        }
        return def_val;
    }

    inline bool getBool(const jval &job, const char* key, int64_t def_val) {
        if (job.IsObject()) {
            auto it = job.FindMember(key);
            if (it != job.MemberEnd()) {
                if (it->value.IsBool()) {
                    return it->value.GetBool();
                } else if (it->value.IsNull()) {
                    return def_val;
                } else {
                    std::string typestr = json::typeStr(it->value.GetType());
                    THROW(TYPE_ERROR, key, typestr);
                }
            }
        }
        return def_val;
    }

} // namespace json

#endif