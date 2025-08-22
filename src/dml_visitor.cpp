#include "dml_visitor.hpp"
#include <sstream>



namespace {
    std::string join(const std::vector<std::string>& xs, const char* sep) {
        std::ostringstream os;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (i) os << sep;
            os << xs[i];
        }
        return os.str();
    }
}

/* ---- helpers ---- */
// const OrmProp* DMLVisitor::find_pk(const OrmSchema& s) {
//     // Prefer explicit is_id; fall back to field named "id"
//     for (const auto& kv : s.fields) {
//         if (kv.second.is_id) return &kv.second;
//     }
//     if (auto it = s.fields.find("id"); it != s.fields.end())
//         return &it->second;
//     return nullptr;
// }

// const jval& DMLVisitor::first_object(const jval& value) {
//     if (value.IsArray()) {
//         if (value.Empty()) throw std::runtime_error("JSON array is empty");
//         const jval& val = value.MemberBegin()->value;
//         if (!val.IsObject()) throw std::runtime_error("First array element is not an object");
//         return val;
//     }
//     if (!value.IsObject()) throw std::runtime_error("JSON must be an object or array of objects");
//     return value;
// }

/* ---- SQLite ---- */
std::string SqliteDMLVisitor::ph(size_t i) const { return "?" + std::to_string(i); }

dml_pair SqliteDMLVisitor::insert(const OrmSchema& s, const jval& value) const {

    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!obj.IsObject() ) throw std::runtime_error("insert: JSON must be an object");

    std::vector<std::string> names, vals;
    size_t i = 0;
    bool pk_in_json = false;

    // Emit fields in JSON order
    // for (auto it : members(obj) ) {
    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
        auto fit = s.fields.find(it->name.GetString());
        if (fit == s.fields.end()) continue;             // ignore unknown keys
        const auto& f = fit->second;
        if (f.name == pk.name) pk_in_json = true;
        names.push_back(f.name);
        vals.push_back(ph(++i));
    }

    // If PK wasn't present in JSON, append it as the LAST column/param
    if (!pk_in_json) {
        names.push_back(pk.name);
        vals.push_back(ph(++i));
    }

    if (names.empty()) throw std::runtime_error("insert: no fields present in JSON or schema");

    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << join(names, ", ")
        << ") VALUES (" << join(vals, ", ") << ");";
    return std::make_pair<std::string, int>(sql.str(), i);
}


dml_pair SqliteDMLVisitor::upsert(OrmSchema& s, const jval& value) const {
    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!value.IsObject()) throw std::runtime_error("upsert: JSON must be an object");

    std::vector<std::string> names, vals, sets;
    size_t i = 0;
    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); it++  ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        names.push_back(f.name);
        vals.push_back(ph(++i));
        if (f.name != pk.name) sets.push_back(f.name + std::string(" = excluded.") + f.name);
    }
    if (names.empty()) throw std::runtime_error("upsert: no fields present in JSON");

    std::ostringstream sql;
    if (sets.empty()) {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk.name << ") DO NOTHING;";
    } else {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk.name << ") DO UPDATE SET " << join(sets, ", ") << ";";
    }
    return std::make_pair<str, int>(sql.str(), 1);
}

dml_pair SqliteDMLVisitor::update(OrmSchema& s, const jval& value) const {
    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!obj.IsObject()) throw std::runtime_error("update: JSON must be an object");

    std::vector<std::string> sets;
    size_t i = 0, non_pk_count = 0;

    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); it++ ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        if (f.name == pk.name) continue;
        sets.push_back(f.name + " = " + ph(++i));
        ++non_pk_count;
    }
    if (sets.empty()) throw std::runtime_error("update: JSON has no updatable fields");

    const size_t pk_idx = non_pk_count + 1;
    std::ostringstream sql;
    sql << "UPDATE " << s.name << " SET " << join(sets, ", ")
        << " WHERE " << pk.name << " = " << ph(pk_idx) << ";";
    return std::make_pair<str, int>(sql.str(), 1);
}

dml_pair SqliteDMLVisitor::remove(OrmSchema& s, const jval&) const {
    const OrmProp& pk = s.idprop();

    std::ostringstream sql;
    sql << "DELETE FROM " << s.name << " WHERE " << pk.name << " = " << ph(1) << ";";
    return std::make_pair<str, int>(sql.str(), 1);
}

/* ---- Postgres ---- */
std::string PgDMLVisitor::ph(size_t i) const { return "$" + std::to_string(i); }

dml_pair PgDMLVisitor::insert(const OrmSchema& s, const jval& value) const {
    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!value.IsObject()) throw std::runtime_error("insert: JSON must be an object");

    std::vector<std::string> names, vals;
    size_t i = 0;
    bool pk_in_json = false;

    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); it++ ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        if (f.name == pk.name) pk_in_json = true;
        names.push_back(f.name);
        vals.push_back(ph(++i));      // uses $i
    }

    if (!pk_in_json) {
        names.push_back(pk.name);
        vals.push_back(ph(++i));
    }

    if (names.empty()) throw std::runtime_error("insert: no fields present in JSON or schema");

    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << join(names, ", ")
        << ") VALUES (" << join(vals, ", ") << ");";
    return std::make_pair<str, int>(sql.str(), 1);
}


dml_pair PgDMLVisitor::upsert(OrmSchema& s, const jval& value) const {
    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!value.IsObject()) throw std::runtime_error("insert: JSON must be an object");

    std::vector<std::string> names, vals, sets;
    size_t i = 0;
    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); it++ ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        names.push_back(f.name);
        vals.push_back(ph(++i));
        if (f.name != pk.name) sets.push_back(f.name + std::string(" = excluded.") + f.name);
    }
    if (names.empty()) throw std::runtime_error("upsert: no fields present in JSON");

    std::ostringstream sql;
    if (sets.empty()) {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk.name << ") DO NOTHING;";
    } else {
        sql << "INSERT INTO " << s.name << " (" << join(names, ", ") << ") VALUES (" << join(vals, ", ")
            << ") ON CONFLICT(" << pk.name << ") DO UPDATE SET " << join(sets, ", ") << ";";
    }
    return std::make_pair<str, int>(sql.str(), 1);
}

dml_pair PgDMLVisitor::update(OrmSchema& s, const jval& value) const {
    const OrmProp& pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!value.IsObject()) throw std::runtime_error("insert: JSON must be an object");

    std::vector<std::string> sets;
    size_t i = 0, non_pk_count = 0;
    for (jit it = obj.MemberBegin(); it != obj.MemberEnd(); it++ ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        if (f.name == pk.name) continue;
        sets.push_back(f.name + " = " + ph(++i));
        ++non_pk_count;
    }
    if (sets.empty()) throw std::runtime_error("update: JSON has no updatable fields");

    const size_t pk_idx = non_pk_count + 1;
    std::ostringstream sql;
    sql << "UPDATE " << s.name << " SET " << join(sets, ", ")
        << " WHERE " << pk.name << " = " << ph(pk_idx) << ";";
    return std::make_pair<str, int>(sql.str(), 1);
}

dml_pair PgDMLVisitor::remove(OrmSchema& s, const jval&) const {
    const OrmProp& pk = s.idprop();
    std::ostringstream sql;
    sql << "DELETE FROM " << s.name << " WHERE " << pk.name << " = " << ph(1) << ";";
    return std::make_pair<str, int>(sql.str(), 1);
}
