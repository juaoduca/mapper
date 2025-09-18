#include "dml_visitor.hpp"
#include <sstream>
#include "lib.hpp"


DML_Result DMLVisitor::insert(const OrmSchema& s, const jval& value, const std::string key) const {
    std::shared_ptr<OrmProp> pk = s.idprop();

    const jval& job = jhlp::first_obj(value);
    if (!value.IsObject()) THROW("upsert: JSON must be an object");

    std::vector<std::string> names, vals, sets;
    bool idValid = false;
    int idIndex = -1;
    int params = 0;
    for (auto jit = job.MemberBegin(); jit != job.MemberEnd(); jit++  ) {
        auto fit = s.fields.find(jit->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        names.push_back(f->name);
        vals.push_back(ph(++params));
        if (f->is_id) {
            idIndex = params;
            if (!jit->value.IsNull()) {
                if (jit->value.IsString()) {
                    idValid = jit->value.GetString() != "" || jit->value.GetString() != "0";
                } else {
                    idValid = jit->value.GetInt64() > 0;
                }
            }
        } else {
            sets.push_back(f->name + std::string(" = excluded.") + f->name);
        }
    }

    if (names.empty()) THROW("upsert: no fields present in JSON");

    if (idIndex == -1) {
        names.push_back(s.idprop()->name);
        vals.push_back(ph(++params));
        idIndex = params;
    }

    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << lib::join(names, ", ") << ") VALUES (" << lib::join(vals, ", ") << ")";
    if (!key.empty()) {
        sql  << " ON CONFLICT(" << key << ") DO NOTHING;";
    } else if (idValid) {
        if (sets.empty()) {
            sql  << " ON CONFLICT(" << pk->name << ") DO NOTHING;";
        } else {
            sql << " ON CONFLICT(" << pk->name << ") DO UPDATE SET " << lib::join(sets, ", ") << ";";
        }
    } else {
        sql << ";";
    }

    DML_Result r = {
        .sql = sql.str(),
        .param_count = params,
        .id_index = idIndex,
        .id_valid = idValid
    };

    std::cout << sql.str() << std::endl;

    return r;
}

dml_pair DMLVisitor::upsert(const OrmSchema &s, const jval& value) const {
    std::shared_ptr<OrmProp> pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!value.IsObject()) THROW("upsert: JSON must be an object");

    std::vector<std::string> names, vals, sets;
    size_t i = 0;
    bool idValid = false;
    int id_index = 0;
    for (auto jit = obj.MemberBegin(); jit != obj.MemberEnd(); jit++  ) {
        auto fit = s.fields.find(jit->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        names.push_back(f->name);
        vals.push_back(ph(++i));
        if (f->is_id) {
            id_index = i;
            if (!jit->value.IsNull()) {
                if (jit->value.IsString()) {
                    idValid = jit->value.GetString() != "" || jit->value.GetString() != "0";
                } else {
                    idValid = jit->value.GetInt64() > 0;
                }
            }
        } else {
            sets.push_back(f->name + std::string(" = excluded.") + f->name);
        }
    }
    if (names.empty()) THROW("upsert: no fields present in JSON");
    if (!idValid) THROW("upsert: JSON must have a VALID ID field");

    std::ostringstream sql;
    sql << "INSERT INTO " << s.name << " (" << lib::join(names, ", ") << ") VALUES (" << lib::join(vals, ", ") << ")";
    if (idValid) {
        if (sets.empty()) {
            sql  << " ON CONFLICT(" << pk->name << ") DO NOTHING;";
        } else {
            sql << " ON CONFLICT(" << pk->name << ") DO UPDATE SET " << lib::join(sets, ", ") << ";";
        }
    } else {
        sql << ";";
    }
    return std::make_pair<std::string, int>(sql.str(), 1);
}

dml_pair DMLVisitor::update(const OrmSchema &s, const jval& value) const {
    std::shared_ptr<OrmProp> pk = s.idprop();

    const jval& obj = jhlp::first_obj(value);
    if (!obj.IsObject()) THROW("update: JSON must be an object");

    std::vector<std::string> sets;
    size_t i = 0, non_pk_count = 0;

    for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); it++ ) {
        auto fit = s.fields.find(it->name.GetString() );
        if (fit == s.fields.end()) continue;
        const auto& f = fit->second;
        if (f->name == pk->name) continue;
        sets.push_back(f->name + " = " + ph(++i));
        ++non_pk_count;
    }
    if (sets.empty()) THROW("update: JSON has no updatable fields");

    const size_t pk_idx = non_pk_count + 1;
    std::ostringstream sql;
    sql << "UPDATE " << s.name << " SET " << lib::join(sets, ", ")
        << " WHERE " << pk->name << " = " << ph(pk_idx) << ";";
    return std::make_pair<std::string, int>(sql.str(), 1);
}

dml_pair DMLVisitor::remove(const OrmSchema &s, const jval&) const {
    std::shared_ptr<OrmProp> pk = s.idprop();

    std::ostringstream sql;
    sql << "DELETE FROM " << s.name << " WHERE " << pk->name << " = " << ph(1) << ";";
    return std::make_pair<std::string, int>(sql.str(), 1);
}

/* ---- SQLite ---- */
std::string SqliteDMLVisitor::ph(size_t i) const { return "?" + std::to_string(i); }

/* ---- Postgres ---- */
std::string PgDMLVisitor::ph(size_t i) const { return "$" + std::to_string(i); }
