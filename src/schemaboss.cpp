#include "schemaboss.hpp"
#include "lib.hpp"

#define ER_MSG1 "Schema: %s Version: %d already exists !"
#define ER_MSG2 "Schema: %s Version: %d must be greater then the newest version: %d"

bool SchemaBoss::add(const OrmSchema& s) {
    if (s.name.empty()) return false;

    SchemaItem& its = catalog_[s.name];

    // must NOT exist already
    auto it = its.versions.find(s.version);
    if (it != its.versions.end()) {
        THROW(ER_MSG1, s.name, s.version);
    }

    // Must be strictly greater than the current newest (if any)
    // the newest can be applied or not
    // obs: as versions its and ordered calling map.rbegin() to get the last item - newest version
    if (!its.versions.empty()) {
        auto newst =  its.versions.rbegin()->second;
        if (s.version <= newst.s->version) {
            THROW(ER_MSG2, s.name, s.version, newst.s->version);
        }
    }

    Version v;
    v.s        = std::make_shared<OrmSchema>(s);
    v.inactive = false;    // fresh version is candidate to become active when applied
    v.applied  = false;    // added only; will be applied on demand via get()

    its.versions.emplace(s.version, std::move(v)); // add to in-memory
    its.newestVersion = s.version;

    // Optional: persist catalog + version rows now
    if (persist_on_add_) persist_on_add_(*its.versions.at(s.version).s);

    return true; // inserted
}

std::shared_ptr<OrmSchema> SchemaBoss::get(const std::string& name, const MigrateFn& migrate) {
    auto ci = catalog_.find(name);
    if (ci == catalog_.end()) return nullptr;
    SchemaItem& its = ci->second;

    //fast check to avoid processing
    if (its.lastApplied > 0 && its.lastApplied == its.newestVersion) {
        if (!its.current) {
            its.current = get_newest(its.name, true); // retrieve the newest version applied == current
        }
        return its.current->s;
    }

    // If no applied yet: apply the NEWEST one directly.
    if (its.lastApplied <= 0) {
        const auto& newest = get_newest(name);
        if (newest == nullptr) return nullptr;
        its.newestVersion = newest->s->version;
        if (!newest->applied) {
            // migrate from nullptr -> newest
            if (!migrate(nullptr, *newest->s)) return nullptr;
            newest->applied = true;
            // older versions (if any) remain unapplied
            its.lastApplied = its.newestVersion;
            its.current = newest;
            //set all, before newest, as inactive=true; as they would never be applied
            for (auto itv: its.versions ) {
                if (itv.second.s->version < newest->s->version) {
                    itv.second.inactive = true;
                }
            }
            // apply changes to DB;
            if (persist_on_apply_) persist_on_apply_(*newest->s, -1);
            return;
        }
    }

    // Advance from current lastApplied to any higher UNAPPLIED versions (ascending)
    int current = its.lastApplied;
    for (auto itv = its.versions.upper_bound(current); itv != its.versions.end(); ++itv) {
        auto& tgt = itv->second;
        if (tgt.applied) { // already applied somehow (e.g., restored)
            current = itv->first;
            its.lastApplied = current;
            continue;
        }

        // from = last applied version (must exist in map at 'current')
        OrmSchema* from = nullptr;
        auto prev = its.versions.find(current);
        if (prev != its.versions.end()) from = prev->second.s.get();

        if (!migrate(from, *tgt.s)) return nullptr;

        // Update flags per spec
        if (prev != its.versions.end()) prev->second.inactive = true; // old becomes inactive
        tgt.applied = true;                                           // new becomes applied
        its.lastApplied = itv->first;                                 // now the latest applied
        current = its.lastApplied;

        if (persist_on_apply_) {
            int oldV = (prev == its.versions.end()) ? -1 : prev->first;
            persist_on_apply_(*tgt.s, oldV);
        }
    }

    // Deliver the last applied to the user
    auto applied_it = its.versions.find(its.lastApplied);
    return (applied_it == its.versions.end()) ? nullptr : applied_it->second.s;
}

std::vector<int> SchemaBoss::unapplied_versions(const std::string& name) const {
    std::vector<int> out;
    auto ci = catalog_.find(name);
    if (ci == catalog_.end()) return out;
    const SchemaItem& its = ci->second;
    for (const auto& kv : its.versions) {
        if (!kv.second.applied) out.push_back(kv.first);
    }
    return out;
}

std::shared_ptr<SchemaBoss::Version> SchemaBoss::get_newest(const std::string& name, bool onlyApplied=false) {
    auto itc = catalog_.find(name);
    Version resp;
    if (itc != catalog_.end()) {
        SchemaItem its = itc->second;
        if (its.versions.empty()) return itc->second.current;
        int v = 0;
        for (auto itv: its.versions) {
            Version ver = itv.second;
            if (v < ver.s->version) {
                if (!onlyApplied) { // newest applied or not
                    resp = ver;
                } else if (ver.s->applied) { // newest applied version
                    resp = ver;
                }
                v = ver.s->version;
            }
        }
        return static_cast<std::shared_ptr<SchemaBoss::Version>>(&resp);
    }
}
