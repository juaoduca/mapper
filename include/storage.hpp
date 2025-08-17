#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "orm.hpp"
#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"
#include "dbpool.hpp"
#include "sqlconnection.hpp"

using OrmSchemaMap = std::map<std::string, std::unique_ptr<OrmSchema>>;

using JSONData = nlohmann::json;

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage
{
public:
    Storage(const std::string &db_path, Dialect dialect); // constructor
    ~Storage(); // destructor

    /**
    * @brief initialize catalog table to hold JSONSchemas for all tables in ORM
    *
    * This method performs the following steps:\n
    * 1. Parses the JSONSchema string for the hardcoded schema_catalog string \n
    * 2. Create and Hydrate an OrmSchema object with schema_catalog JSON Object \n
    * 2. call DDLVistor to produce the create table script for schema_catalog \n
    * 3. call exec() method to create the table on DB \n
    * 4. call addSchema() to add the schema to the in-memory catalog. \n
    * 5. repeat the above steps for the hardcoded schema_versions string. \n
    *
    * @return The index of the schema in the catalog map.
    */
    bool init_catalog();

    /**
    * @brief Adds a JSONSchema string to the system.
    *
    * This method performs the following steps:
    * 1. Parses the JSONSchema string to a SJON Object
    * 2. Create an OrmSchema object an hydrate it with JSONSchema Object
    * 3. calls addSchema() passing the OrmSchema object
    *
    * @param JSONSchema The JSONSchema string to be added.
    * @param conn If not null, also adds the schema to the DB catalog table.
    * @return inserted schema index on in-memory catalog or -1 fail;
    */
    int addSchema(std::string &JSONSchema, SQLConnection* conn=nullptr);

    /**
    * @brief Adds a schema object to the system.
    *
    * This method performs the following steps:
    * 1. Adds the OrmSchema to the in-memory catalog if not exists.
    * 2. call Storage::insert() to add to DB if @p conn is not null.
    *
    * @param schema The schema object to be added.
    * @param conn If not null, call Storage::insert() to add to DB
    * @return bool true success false fail
    */
    bool addSchema(OrmSchema& schema, SQLConnection* conn=nullptr);

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

    bool execDDL(std::string sql); // executes SQL direct to DB
    int execDML(std::string sql, const std::vector<std::string>& params = {}); // executes SQL direct to DB

    /**
    * @brief Insert or Upsert data to DB by @p schemaName
    *
    * This method performs the following steps:
    * 1 - find the OrmSchema by schemaName
    * 2 - acquire a conn from pool
    * 3 - call conn::begin() - to start a TR if not Started
    * 4 - call overloaded Storage::insert(conn, schema, data, track);
    * 5 - call conn::commit() or conn::rollback() - if TR is Started
    *
    * @param schemaName the schemaName to be inserted to DB
    * @param data a JSON instance with the data to be inserted
    * @param trackinfo a pointer to track info for tracking and audit
    * @return number of rows affected
    */
    int insert(const std::string &schemaName, JSONData &data, const std::string &trackinfo);

/**
    * @brief Insert or Upsert data to DB by @p schema OrmSchema object
    *
    * This method performs the following steps:
    * 1 - call DMLVisitor to create Insert/Upsert SQL statement
    *       1.1 - if ID is present and ID > 0 and ID is not empty => UPSERT
    *       1.2 - else INSERT
    * 2 - Start a Transaction conn::begin()
    * 3 - prepare the statement
    * 4 - check if data is a JSON Array or a JSON Object
    * 5 - for each JSON Object in data
    *       5.1 - if INSERT generate ID by IDKind prop of OrmField
    *       5.2 - bind the params and insert/upsert
    *       5.3 - if param track is not null  insert Track/Audit data
    * 6 - commit or rollback the Transaction conn::commit() conn::rollback();
    * 7 - call notify() to notify subscribers for this Schema and this CRUD operation
    *
    * @param conn Acquired conn ref - conn controls the Transaction state
    * @param schema the OrmSchema object used to insert data to
    * @param data a JSONData instance with the data to be inserted - array or object
    * @param trackinfo a pointer to track info for tracking and audit
    * @return number of rows affected
    */
    int insert(SQLConnection& conn, OrmSchema& schema, JSONData &data, const std::string &trackinfo);

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
    * @param tr a pointer to user info for trank and audit
    * @param trackinfo a pointer to context infor for track and audit
    * @return int
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
    OrmSchemaMap catalog;
    std::shared_ptr<pool::IDbPool> dbpool_;
    std::unique_ptr<DDLVisitor> ddlVisitor_;
    std::unique_ptr<DMLVisitor> dmlVisitor_;
    // std::unique_ptr<QRYVisitor> qryVisitor_;
};
