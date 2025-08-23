#pragma once
#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include "orm.hpp"

/**
 * SchemaBoss
 *  - Manages schemas by name
 *  - Keeps all versions (ascending)
 *  - Tracks newestVersion and lastApplied
 *  - Applies forward migrations on demand (CRUD entry) via callback
 *
 * Notes:
 *  - applied == "DDL for this version was already applied to DB"
 *  - inactive == "not the most-recent applied version currently served to NEW users"
 *  - Old applied versions remain available for existing users until released externally.
 */
class SchemaBoss {
public:
    struct Version {
        std::shared_ptr<OrmSchema> schema; // stored schema (immutable content)
        bool applied  = false;         // DDL applied to DB at some point
        bool inactive = false;         // not the latest applied (new users should not receive)
        int  in_use   = 0;             // reserved for future leasing (old users finish jobs)
    };

    struct SchemaItem {
        std::map<int, Version> versions; // key: schema.version (ascending)
        int newestVersion = -1;          // highest key present
        int lastApplied   = -1;          // highest version with applied==true, -1 if none
        std::string name = "";           // filled on load
        std::shared_ptr<Version> current; // current version in use
    };

    using MigrateFn       = std::function<bool(const OrmSchema* from, const OrmSchema& to)>;
    using PersistOnAddFn  = std::function<void(const OrmSchema& added)>;            // insert into schema_catalog / schema_versions
    using PersistOnApplyFn= std::function<void(const OrmSchema& applied, int oldV)>; // update schema_versions flags

    // Optional side-effects (DB integration)
    void set_persist_on_add(PersistOnAddFn fn)   { persist_on_add_   = std::move(fn); }
    void set_persist_on_apply(PersistOnApplyFn f){ persist_on_apply_ = std::move(f); }

    // Insert a NEW version for a schema name. Never replaces; throws if duplicate.
    // Enforces strictly increasing version numbers.
    bool add(const OrmSchema& s);

    // Ensure latest-applied for 'name' (migrate as needed) and return it.
    // If nothing applied yet, applies the NEWEST directly (per spec).
    // Returns nullptr if name unknown.
    std::shared_ptr<OrmSchema> get(const std::string& name, const MigrateFn& migrate);

    // Read helpers
    bool has(const std::string& name) const { return catalog_.find(name) != catalog_.end(); }
    std::vector<int> unapplied_versions(const std::string& name) const;
    std::shared_ptr<Version> get_newest(const std::string& name, bool onlyApplied=false);

private:
    std::unordered_map<std::string, SchemaItem> catalog_;
    PersistOnAddFn    persist_on_add_;
    PersistOnApplyFn  persist_on_apply_;
};
