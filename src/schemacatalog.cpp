#include <concepts>
#include "lib.hpp"
#include "jsonhlp.hpp"
#include "schemacatalog.hpp"


#define ER_MSG1 "Schema: %s Version: %d already exists !"
#define ER_MSG2 "Schema: %s Version: %d must be greater then latest version: %d"


void SchemaCatalog::load() {
        // storage_->with_conn(DbIntent::Write,
        //     [&](SQLConnection& conn) {

        //     }
        // );

}

bool SchemaCatalog::add(const OrmSchema& new_schema, bool insert/*=true*/) {
    if (!insertCatalog) { THROW("No 'InsertCatalogFn' function provided! Provide a SchemaCatalog::insertCatalog() implementation"); }
    if (new_schema.name.empty()) return false;
    CatalogItem *catalog_item = nullptr;
    //schema catalog item may not exists
    auto catalog_iterator = catalog_.find(new_schema.name);
    if (catalog_iterator == catalog_.end()) {
        //if not exists add catalog item
        CatalogItem temp_cat_item;
        temp_cat_item.lastApplied = NONE_APPLIED;
        temp_cat_item.latestVersion = NONE_LATEST;
        temp_cat_item.name = new_schema.name;
        temp_cat_item.current = nullptr; //no current version was applied yet
        catalog_item = &temp_cat_item;
    } else {
        catalog_item = &catalog_.find(new_schema.name)->second;
    }

    // Version must NOT exist already
    auto it = catalog_item->versions.find(new_schema.version);
    if (it != catalog_item->versions.end()) {
        THROW(ER_MSG1, new_schema.name, new_schema.version);
    }

    //new version must be greater then the existing latest
    const auto& latest = get_latest(new_schema.name, false); // last one applied or not
    if (latest != nullptr && new_schema.version <= latest->schema->version) {
        THROW(ER_MSG2, new_schema.name, new_schema.version, latest->schema->version);
    }

    CatalogVersion scv; // create the version
    scv.schema   = std::make_shared<OrmSchema>(new_schema);
    scv.applied  = false;    // added only; will be applied on demand via Storage::init() -> catalog_->init();
    scv.item     = std::make_shared<CatalogItem>(*catalog_item);

    catalog_item->latestVersion = new_schema.version;
    catalog_item->versions.emplace(new_schema.version, std::move(scv)); // add to in-memory
    catalog_.emplace(catalog_item->name, std::move(*catalog_item) );

    // sci.lastApplied = NONE_APPLIED;

    // persist catalog and version rows
    if (insert) {
        return insertCatalog(new_schema);
    }
    return true;

}

/** check if has a current version, if true: return it! if not: retrieve return */
std::shared_ptr<OrmSchema> SchemaCatalog::get_schema(const std::string& name) {

    auto ci = catalog_.find(name);
    if (ci == catalog_.end()) return nullptr;
    CatalogItem& sci = ci->second;
    if (!sci.current) {
        CatalogVersion &latest = *get_latest(sci, false);
        return latest.schema;
    }
    return sci.current->schema;

    // //fast check to avoid seaching the catalog
    // if (sci.lastApplied == sci.latestVersion) {
    //     if (!sci.current) {
    //         sci.current = get_latest(sci.name, true); // retrieve the latest version applied == current
    //     }
    //     return sci.current->schema;
    // }

    // // If no one was applied yet: apply the NEWEST one directly.
    // if (sci.lastApplied <= 0) {
    //     const auto& latest = get_latest(name, false);
    //     if (latest == nullptr) return nullptr;
    //     sci.latestVersion = latest->schema->version;
    //     if (!latest->applied) {
    //         //its not a migrate - its a initialization
    //         if (!storage_->init_schema(*sci.current->schema)) return nullptr;

    //         latest->applied = true;
    //         sci.lastApplied = sci.latestVersion;
    //         sci.current = latest;

    //         // apply changes to DB;
    //         storage_->updateApplied(*latest->schema, sci.lastApplied);
    //         latest->schema->applied_at = std::chrono::system_clock::now();
    //     }
    //     return latest->schema;
    // }

    // // if exists an applied version(current) find the latest unapplied one
    // const auto& latest = get_latest(name, /*applied=*/false);//must exist - may throw if not
    // if (sci.latestVersion != latest->schema->version) THROW("[SchemaBoss] - Falha de sincronia entre ADD e GET");
    // OrmSchema* from = sci.current->schema.get();

    // //apply to DB
    // if (!storage_->applyUpdates(from, *latest->schema)) return nullptr;
    // // if (!migrate_(from, *latest->schema)) return nullptr;

    // //update schema catalog item ctrl flags
    // sci.current = latest;
    // sci.current->applied = true;                    // new becomes applied
    // sci.lastApplied = sci.current->schema->version; // now the latest applied

    // //return the latest
    // return latest->schema;

}

std::shared_ptr<CatalogItem> SchemaCatalog::get_item(const std::string& name){
    return nullptr;
}

std::shared_ptr<CatalogVersion> SchemaCatalog::get_latest(CatalogItem &catalog_item, bool onlyApplied/*=false*/) {

    std::shared_ptr<CatalogVersion> resp = nullptr;

    int v = 0;
    for (auto itv: catalog_item.versions) {
        CatalogVersion ver = itv.second;
        if (v < ver.schema->version) {
            if (!onlyApplied) { // latest applied or not
                resp = std::make_shared<CatalogVersion>(ver);
            } else if (ver.schema->applied) { // only latest applied
                resp = std::make_shared<CatalogVersion>(ver);
            }
        }
        v = ver.schema->version;
    }
    return resp;
}

std::shared_ptr<CatalogVersion> SchemaCatalog::get_latest(const std::string& name, bool onlyApplied/*=false*/) {
    auto catalog_iterator = catalog_.find(name);
    if (catalog_iterator != catalog_.end()) {
        CatalogItem catalog_item = catalog_iterator->second;
        return get_latest(catalog_item, onlyApplied);
    }
    return nullptr;
}

bool SchemaCatalog::init(CatalogItem &item) {
    if (!initSchema) {THROW("Function SchemaCatalog::InitSchemaFn was not Defined!");}
    if (!applyUpdates) {THROW("Function SchemaCatalog::ApplyUpdatesFn was not Defined!");}

    if (item.current == nullptr) { // none applied yet - apply latest
        CatalogVersion &latest = *get_latest(item, false);
        return initSchema(*latest.schema);
    } else if ( item.lastApplied != item.latestVersion ) {
        // there is a new version present - current is the old one
        CatalogVersion &latest = *get_latest(item);
        OrmSchema *old = item.current->schema.get();
        OrmSchema &new_ = *latest.schema;
        return applyUpdates(old, new_);
    }
    return false;

    // if (!version.applied) { // se não foi aplicada ao banco
    //     //its not a migrate - its a initialization
    //     if (!storage_->init_schema(*version.schema)) return false;
    //     CatalogItem &catalog_item = *version.item;
    //     version.applied = true;
    //     catalog_item.lastApplied = catalog_item.latestVersion;
    //     catalog_item.current = std::make_shared<CatalogVersion>(version);
    //     version.schema->applied_at = std::chrono::system_clock::now();

    // }
    // return true;
}

bool SchemaCatalog::init(std::string &name) {
    auto catalog_iterator = catalog_.find(name);
    if (catalog_iterator == catalog_.end()) return false;
    CatalogItem &catalog_item = catalog_iterator->second;
    return init(catalog_item);
}
