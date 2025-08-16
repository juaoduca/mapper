#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "orm.hpp"
#include "visitor.hpp"
#include "sqlconnection.hpp"

using OrmSchemaMap = std::map<std::string, std::unique_ptr<OrmSchema>>;

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage
{
public:
    Storage(const std::string &db_path, Dialect dialect) // constructor
        : catalog(OrmSchemaMap()) {};
    ~Storage(); // destructor

    /**
    * @brief initialize catalog table to hold JSONSchemas for all tables in ORM
    *
    * This method performs the following steps:
    * 1. Parses the JSONSchema string for the hardcoded schema_catalog string
    * 2. Create and Hydrate an OrmSchema object with schema_catalog JSON Object
    * 2. call DDLVistor to produce the create table script for schema_catalog
    * 3. call exec() method to create the table on DB
    * 4. call addSchema() to add the schema to the in-memory catalog.
    * 5. repeat the above steps for the hardcoded schema_versions string.
    *
    * @return The index of the schema in the catalog map.
    */
    bool init_catalog();

    /**
    * @brief Adds a schema to the system.
    *
    * This method performs the following steps:
    * 1. Parses the JSONSchema string to a SJON Object
    * 2. Create an OrmSchema object an hydrate it with JSONSchema Object
    * 3. Adds the OrmSchema to the in-memory catalog.
    * 4. Writes the schema to the database if @p db is true.
    *
    * @param JSONSchema The JSONSchema string to be added.
    * @param db If true, also adds the schema to the DB catalog table.
    * @return The index of the schema in the catalog map.
    */
    int addSchema(std::string &JSONSchema, bool db = true);

    /**
    * @brief Removes a schema from system
    *
    * This method performs the following steps:
    * 1. find the OrmSchema
    * 2. kill the orm schema
    * 3. remove from catalog
    * 4. remove from DB
    *
    * @param name the schema name to be removed
    * @return True if schema was removed from system and DB, false otherwise
    */
    bool remSchema(std::string &name);

    /**
    * @brief Retrive an OrmSchema pointer from catalog
    *
    * @param name the schema name to be retrieved
    * @param schema pointer to the OrmSchema instance
    * @return true  if Schema was found , false otherwise
    */
    bool getSchema(std::string &name, OrmSchema& schema);

    bool exec(std::string_view sql); // executes SQL direct to DB

    /**
    * @brief Insert or Upsert data into Schema table
    *
    * This method performs the following steps:
    * 1 - find the OrmSchema
    * 2 - check if data is a JSON Array or a JSON Object
    * 3 - call DMLVisitor to create Insert/Upsert SQL statement
    * 4 - start a Transaction
    * 5 - prepare the statement
    * 6 - for each JSON Object in data
    *       6.1 - if Object does not have an ID generate one by the IDKind prop of OrmField
    *       6.2 - bind the params and insert
    *       6.3 - if params user or context is not null  insert Track/Audit data
    * 7 - commit the Transaction
    * 8 - call notify() to notify subscribers for this Schema and this CRUD operation
    *
    * @param name the schema to be inserted
    * @param data a JSON instance with the data to be inserted
    * @param user a pointer to user info for trank and audit
    * @param context a pointer to context infor for track and audit
    * @return pointer to OrmSchema instance
    */
    std::string insert(const std::string &name, nlohmann::json &data, const std::string &user = "", const std::string &context = "");

    /**
    * @brief Update data into Schema table
    *
    * This method performs the following steps:
    * 1 - find the OrmSchema
    * 2 - check if data is a JSON Array or a JSON Object
    * 3 - call DMLVisitor to create UPDATE SQL statement
    * 4 - start a Transaction
    * 5 - prepare the statement
    * 6 - for each JSON Object in data
    *       6.1 - if Object does not have an ID throw an error.
    *       6.2 - bind the params and insert
    *       6.3 - if params user or context is not null  insert Track/Audit data
    * 7 - commit the Transaction
    * 8 - call notify() to notify subscribers for this Schema and this CRUD operation
    *
    * @param name the schema to be inserted
    * @param data a JSON instance with the data to be inserted
    * @param user a pointer to user info for trank and audit
    * @param context a pointer to context infor for track and audit
    * @return pointer to OrmSchema instance
    */
    void update(const std::string &name, const nlohmann::json &data, const std::string &user = "", const std::string &context = "");

    /**
    * @brief Delete data from Schema table
    *
    * This method performs the following steps:
    * 1 - find the OrmSchema
    * 2 - check if data is a JSON Array or a JSON Object
    * 3 - call DMLVisitor to create DELETE SQL statement - only Delete by ID
    * 4 - start a Transaction
    * 5 - prepare the statement
    * 6 - for each JSON Object in data
    *       6.1 - if Object does not have an ID throw an error.
    *       6.2 - delete by ID
    *       6.3 - if params user or context is not null  insert Track/Audit data
    * 7 - commit the Transaction
    * 8 - call notify() to notify subscribers for this Schema and this CRUD operation
    */
    void del(const std::string &name, const nlohmann::json &data, const std::string &user = "", const std::string &context = "");

private:
    std::unique_ptr<DDLVisitor> visitor_; // holds the correct SQL generator
    std::unique_ptr<SQLConnection> conn_;
    OrmSchemaMap catalog;
};
