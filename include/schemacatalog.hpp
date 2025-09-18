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
 *          -----------------
 *          |  CatalogItem  |
 *          -----------------
 *                     1|
 *                      |
 *                      |N
 *                   --------------------- 1     1 --------------
 *                   |  CatalogVersions  | ------> | OrmSchema  |
 *                   ---------------------         --------------
 */

 #define NONE_LATEST 0;
 #define NONE_APPLIED -1

    struct CatalogItem;

    struct CatalogVersion {
        std::shared_ptr<OrmSchema> schema; // stored schema (immutable content)
        std::shared_ptr<struct CatalogItem> item; //
        bool applied  = false;         // DDL applied to DB at some point
    };

    struct CatalogItem {
        std::map<int, CatalogVersion> versions; // key= schema.version (ascending)
        int latestVersion = NONE_LATEST;          // highest version present applied or not = NONE_LATEST == 0
        int lastApplied   = NONE_APPLIED;         // current version with applied==true, NONE_APPLIED == -1
        std::string name = "";           // filled on load
        std::shared_ptr<CatalogVersion> current; // current CatalogVersion object in use
    };

using InsertCatalogFn = std::function<bool (const OrmSchema &new_Schema)>;
using InitSchemaFn = std::function<bool (OrmSchema &schema)>;
using ApplyUpdatesFn = std::function<bool (OrmSchema *old_Schema, OrmSchema &new_schema)>;

/**
 * SchemaCatalog
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
class SchemaCatalog {
private:
    std::unordered_map<std::string, CatalogItem> catalog_;
    // std::shared_ptr<Storage> storage_;
    // PersistOnAddFn    persist_on_add_;
    // PersistOnApplyFn  persist_on_apply_;
    // MigrateFn         migrate_;
public:
    SchemaCatalog() { };
    ~SchemaCatalog() = default;

    InsertCatalogFn insertCatalog;
    InitSchemaFn    initSchema   ;
    ApplyUpdatesFn  applyUpdates ;

    bool add(const OrmSchema& s, bool insert = true);

    // Gets an OrmSchema by its name, nullptr if not found
    std::shared_ptr<OrmSchema> get_schema(const std::string &name);

    // Returns a CatalogItem by its name, nullptr if not found
    std::shared_ptr<CatalogItem> get_item(const std::string &name);

    //returns a CatalogVersion by its name,
    // if onlyApplied is true - return the current applied version, if none applied, returns the lastest
    // if onlyApplied is false return the latest version, aplied or not
    std::shared_ptr<CatalogVersion> get_latest(const std::string &name, bool onlyApplied=false);
    std::shared_ptr<CatalogVersion> get_latest(CatalogItem &catalog_item, bool onlyApplied=false);

    // Ensure latest-applied for 'name' (migrate as needed) and return it.
    // If nothing applied yet, applies the latest directly (per spec).
    bool init(std::string &name);
    //
    bool init(CatalogItem &item);

    //loads all registered schemas from DB, and all versions above lastest applied
    void load();

};
