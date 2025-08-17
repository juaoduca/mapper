#include "dml_visitor.hpp"
#include <sstream>

using nlohmann::json;

namespace {
    std::string join(const std::vector<std::string>& xs, const char* sep) {
        std::ostringstream os;
        for (size_t i=0;i<xs.size();++i) { if (i) os << sep; os << xs[i]; }
        return os.str();
    }
};

/* ---- helpers ---- */
const OrmField* DMLVisitor::find_pk(const OrmSchema& s) {
    for (const auto& f : s.fields) if (f.is_id) return &f;
    for (const auto& f : s.fields) if (f.name == "id") return &f;
    return nullptr;
}

const json& DMLVisitor::first_object(const json& data) {
    if (data.is_array()) {
        if (data.empty()) throw std::runtime_error("JSON array is empty");
        if (!data.front().is_object()) throw std::runtime_error("First array element is not an object");
        return data.front();
    }
    if (!data.is_object()) throw std::runtime_error("JSON must be an object or array of objects");
    return data;
}

std::vector<const OrmField*> DMLVisitor::select_fields_in_order(const OrmSchema& s,
                                                                    const json& obj,
                                                                    bool exclude_pk) {
    std::vector<const OrmField*> cols;
    const OrmField* pk = find_pk(s);
    for (const auto& f : s.fields) {
        if (exclude_pk && pk && f.name == pk->name) continue;
        if (obj.contains(f.name)) cols.push_back(&f);
    }
    return cols;
}

/* ---- SQLite ---- */
std::string SqliteDMLVisitor::ph(size_t i) const { return "?" + std::to_string(i); }

std::string SqliteDMLVisitor::insert(const OrmSchema& s, const json& data) const {
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, false);
    if (cols.empty()) throw std::runtime_error("insert: no fields present in JSON");
    std::vector<std::string> names, vals; names.reserve(cols.size()); vals.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) { names.push_back(cols[i]->name); vals.push_back(ph(i+1)); }
    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ") << ");";
    return sql.str();
}

std::string SqliteDMLVisitor::upsert(const OrmSchema& s, const json& data) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("upsert: no PK");
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, false);
    if (cols.empty()) throw std::runtime_error("upsert: no fields present in JSON");

    std::vector<std::string> names, vals, sets;
    names.reserve(cols.size()); vals.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) {
        names.push_back(cols[i]->name);
        vals.push_back(ph(i+1));
        if (cols[i]->name != pk->name) sets.push_back(cols[i]->name + " = excluded." + cols[i]->name);
    }
    std::ostringstream sql;
    if (sets.empty()) {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk->name << ") DO NOTHING;";
    } else {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk->name << ") DO UPDATE SET " << join(sets, ", ") << ";";
    }
    return sql.str();
}

std::string SqliteDMLVisitor::update(const OrmSchema& s, const json& data) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("update: no PK");
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, true);
    if (cols.empty()) throw std::runtime_error("update: JSON has no updatable fields");

    std::vector<std::string> sets; sets.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) sets.push_back(cols[i]->name + " = " + ph(i+1));
    const size_t pk_idx = cols.size() + 1;

    std::ostringstream sql;
    sql << "UPDATE " << s.name << " SET " << join(sets, ", ")
        << " WHERE " << pk->name << " = " << ph(pk_idx) << ";";
    return sql.str();
}

std::string SqliteDMLVisitor::remove(const OrmSchema& s, const json&) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("delete: no PK");
    std::ostringstream sql;
    sql << "DELETE FROM " << s.name << " WHERE " << pk->name << " = " << ph(1) << ";";
    return sql.str();
}

/* ---- Postgres ---- */
std::string PgDMLVisitor::ph(size_t i) const { return "$" + std::to_string(i); }

std::string PgDMLVisitor::insert(const OrmSchema& s, const json& data) const {
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, false);
    if (cols.empty()) throw std::runtime_error("insert: no fields present in JSON");
    std::vector<std::string> names, vals; names.reserve(cols.size()); vals.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) { names.push_back(cols[i]->name); vals.push_back(ph(i+1)); }
    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ") << ");";
    return sql.str();
}

std::string PgDMLVisitor::upsert(const OrmSchema& s, const json& data) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("upsert: no PK");
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, false);
    if (cols.empty()) throw std::runtime_error("upsert: no fields present in JSON");

    std::vector<std::string> names, vals, sets;
    names.reserve(cols.size()); vals.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) {
        names.push_back(cols[i]->name);
        vals.push_back(ph(i+1));
        if (cols[i]->name != pk->name) sets.push_back(cols[i]->name + " = excluded." + cols[i]->name);
    }
    std::ostringstream sql;
    if (sets.empty()) {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk->name << ") DO NOTHING;";
    } else {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk->name << ") DO UPDATE SET " << join(sets, ", ") << ";";
    }
    return sql.str();
}

std::string PgDMLVisitor::update(const OrmSchema& s, const json& data) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("update: no PK");
    const json& obj = first_object(data);
    auto cols = select_fields_in_order(s, obj, true);
    if (cols.empty()) throw std::runtime_error("update: JSON has no updatable fields");

    std::vector<std::string> sets; sets.reserve(cols.size());
    for (size_t i=0;i<cols.size();++i) sets.push_back(cols[i]->name + " = " + ph(i+1));
    const size_t pk_idx = cols.size() + 1;

    std::ostringstream sql;
    sql << "UPDATE " << s.name << " SET " << join(sets, ", ")
        << " WHERE " << pk->name << " = " << ph(pk_idx) << ";";
    return sql.str();
}

std::string PgDMLVisitor::remove(const OrmSchema& s, const json&) const {
    const OrmField* pk = find_pk(s); if (!pk) throw std::runtime_error("delete: no PK");
    std::ostringstream sql;
    sql << "DELETE FROM " << s.name << " WHERE " << pk->name << " = " << ph(1) << ";";
    return sql.str();
}
