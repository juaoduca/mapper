#include <iostream>
#include "storage.hpp"
#include "bootstrap.hpp"

// load tenants from DB
// we need a LandLord to control the tenants
// it act like a master tenant with its DB sync with an upper server

class Tenant {
    public:
        Tenant(const std::string &dbConnStr, Dialect dialect) {
            storage_ = std::make_shared<Storage>(dbConnStr, dialect);
        }
        std::string TenantName;
    private:
        std::shared_ptr<Storage> storage_ = nullptr;
};

class Lessor: public Tenant {
    public:
        Lessor(const std::string &dbConnStr, Dialect dialect)
        : Tenant(dbConnStr, dialect) { // this initi is like a super() call
        };

        void loadTenants() {

            // if (storage_->select("select * from tenant", {})) {

            // }
        }
};

int main() {
    Storage store("./database.db", Dialect::SQLite);
    store.init_catalog();
    // must exist a item in schemacatalog for schema_catalog and schema_versions
    // must exist a schema_catalog table and a schema_version table
    // must exist a record in both tables for schema_catalog
    // must exist a record in both tables for schema_versions

    std::string jsch = SCHEMA_USER;
    if (store.addSchema(jsch)) {
        //must exist a record in schema_catalog for schema_user
        //and a record in schema_versions for schema_user
        //
    };

    return 0;
}


// int main()
// {
//     Lessor lessor("./lessor.db", Dialect::SQLite); // the owner of the service
//     std::unordered_map<std::string, std::shared_ptr<Tenant>> tenants; // they pay to use the service

//     lessor.loadTenants();

//     while (true) {
//         //
//     };


//     return 0;
// }
