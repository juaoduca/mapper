#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <map>
#include "dbpool.hpp"
#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"
#include "snowflake.hpp"
#include "sqlconnection.hpp"
#include "ulid.hpp"

using OrmSchemaMap = std::map<std::string, std::shared_ptr<OrmSchema>>;

using namespace std::literals::chrono_literals;
// using Json = Json;

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage {
public:
    Storage(const std::string& db_path, Dialect dialect); // constructor
    ~Storage() = default;

    /**
     * @brief initialize catalog_ table to hold JSONSchemas for all tables in ORM
     *
     * This method performs the following steps:
     * 1. Parses the JSONSchema string for the hardcoded schema_catalog string
     * 2. Create and Hydrate an OrmSchema object with schema_catalog JSON Object
     * 3. call DDLVistor to produce the create table script for schema_catalog
     * 4. call exec() method to create the table on DB
     * 5. call addSchema() to add the schema to the in-memory catalog_.
     * 6. repeat the above steps for the hardcoded schema_versions string.
     *
     * @return True success false fail
     */
    bool init_catalog();

    /**
     * @brief Adds a JSONSchema string to the system.
     *
     * This method performs the following steps:
     *
     * 1. Parses the JSONSchema string to a SJON Object
     * 2. Create an OrmSchema object an hydrate it with JSONSchema Object
     * 3. calls addSchema() passing the OrmSchema object
     *
     * @param JSONSchema The JSONSchema string to be added.
     * @param conn If not null, also adds the schema to the DB catalog_ table.
     * @return {boolean} true success false fail;
     */
    bool addSchema(std::string& JSONSchema, SQLConnection* conn = nullptr);

    /**
     * @brief Adds a schema object to the system.
     *
     * This method performs the following steps:
     *
     * 1. Adds the OrmSchema to the in-memory catalog_ if not exists.
     * 2. call Storage::insert() to add to DB if @p conn is not null.
     *
     * @param schema The schema object to be added.
     * @param conn If not null, call Storage::insert() to add to DB
     * @return bool true success false fail
     */
    bool addSchema(OrmSchema& schema, SQLConnection* conn = nullptr);

    /**
     * @brief Removes a schema from system
     *
     * This method performs the following steps:
     *
     * 1. find the OrmSchema
     *
     * 2. kill the orm schema
     *
     * 3. remove from catalog_
     *
     * 4. remove from DB
     *
     * @param name the schema name to be removed
     * @return True if schema was removed from system and DB, false otherwise
     */
    bool remSchema(std::string& name);

    /**
     * @brief Retrive an OrmSchema pointer from catalog_
     *
     * @param name the schema name to be retrieved
     * @param schema pointer to the OrmSchema instance
     * @return true  if Schema was found , false otherwise
     */
    bool getSchema(std::string& name, OrmSchema& schema);

    /**
     * @brief helper to retrive SQLConnection from pool and keep Lease in scope
     *
     * @param pool
     * @param fn
     * @return
     */
    template <class F>
    auto with_conn(pool::DbIntent intent, F&& fn) -> std::optional<std::invoke_result_t<F, SQLConnection&>> {
        auto ac = dbpool_->acquire(intent, 1000ms);
        if (!ac.ok) return std::nullopt;
        // Keep the lease alive for the whole scope; it will release on destruction.
        auto& lease = ac.lease;
        SQLConnection& conn = lease.conn();

        return std::optional<std::invoke_result_t<F, SQLConnection&>> {
            std::forward<F>(fn)(conn) // call the lambda function to operate with conn while lease is alive
        };
    }

    // Convenience: same as above, but return a default value when acquire fails.
    template <class F, class T = std::invoke_result_t<F, SQLConnection&>>
    T with_conn_fb(pool::DbIntent intent, F&& fn, T fallback) {
        auto r = with_conn(intent, std::forward<F>(fn));
        return r ? *r : std::move(fallback);
    }

    template <class F>
    auto with_tr(pool::DbIntent intent, F&& fn) -> std::optional<std::invoke_result_t<F, SQLConnection&>> {
        auto ac = dbpool_->acquire(intent, 1000ms);
        if (!ac.ok) return std::nullopt;
        auto& lease = ac.lease; // keep lease alive for whole TX
        SQLConnection& conn = lease.conn();

        if (!conn.begin()) throw std::runtime_error("begin() failed");
        try {
            using R = std::invoke_result_t<F, SQLConnection&>;
            R result = std::forward<F>(fn)(conn);

            if (!conn.commit()) {
                conn.rollback();
                throw std::runtime_error("commit() failed - transaction rolled back");
            }
            return std::optional<R> { std::move(result) };
        } catch (...) {
            conn.rollback();
            throw; // propagate
        }
    }

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
    int insert(const std::string& schemaName, jval& data, const std::string& trackinfo);

    /**
     * @brief Insert or Upsert data to DB by @p schema OrmSchema object
     *
     * This method performs the following steps:
     *
     * 1. call DMLVisitor to create Insert/Upsert SQL statement
     *       1.1. if ID is present and ID > 0 and ID is not empty => UPSERT
     *       1.2. else INSERT
     * 2. Do not start a transaction(TX) - must be controoled by caller
     * 3. prepare the statement
     * 4. check if data is a JSON Array or a JSON Object
     * 5. for each JSON Object in data
     *       5.1. if INSERT generate ID by IDKind prop of OrmField
     *       5.2. bind the params and insert/upsert
     *       5.3. if param track is not null  insert Track/Audit data
     * 6. do not commit or rollback - trhow error - caller control TX
     * 7. call notify() to notify subscribers for this Schema and this CRUD operation
     *
     * @param conn Acquired conn ref - conn controls the Transaction state
     * @param schema the OrmSchema object used to insert data to
     * @param data a Json instance with the data to be inserted - array or object
     * @param trackinfo a pointer to track info for tracking and audit
     * @return number of rows affected
     */
    int insert(SQLConnection& conn, OrmSchema& schema, jval& data, const std::string& trackinfo);

    /**
     * @brief Update data into Schema table
     *
     * This method performs the following steps:
     *
     * 1 - find the OrmSchema by schemaName
     * 2 - acquire a conn from pool
     * 3 - call conn::begin() - to start a TR if not Started
     * 4 - call overloaded Storage::update(conn, schema, data, track);
     * 5 - call conn::commit() or conn::rollback() - if TR is Started
     *
     * @param schemaName the schemaName to be inserted to DB
     * @param data a JSON instance with the data to be inserted
     * @param trackinfo a pointer to track info for tracking and audit
     * @return number of rows affected
     */
    int update(const std::string& schemaName, jval& value, const std::string& trackinfo);

/**
     * @brief update data to DB by @p schema OrmSchema object
     *
     * This method performs the following steps:
     *
     * 1. call DMLVisitor to create update SQL statement
     *       1.1. if ID is not present - throw an error
     *       1.2. else UPDATE
     * 2. Do not start a transaction(TX) - must be controled by caller
     * 3. prepare the statement
     * 4. check if data is a JSON Array or a JSON Object
     * 5. for each JSON Object in data
     *       5.1. check if ID exists
     *       5.2. bind the params and update
     *       5.3. if param track is not null  insert Track/Audit data
     * 6. do not commit or rollback - throw error - caller control TX
     * 7. call notify() to notify subscribers for this Schema and this CRUD operation
     *
     * @param conn Acquired conn ref - conn controls the Transaction state
     * @param schema the OrmSchema object used to UPDATE data to
     * @param data a Json instance with the data to be UPDATED - array or object
     * @param trackinfo a pointer to track info for tracking and audit
     * @return number of rows affected
     */
    int update(SQLConnection& conn, OrmSchema& schema, jval& value, const std::string& trackinfo);

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
    void del(const std::string& name, const jval& value, const std::string& user = "", const std::string& context = "");

private:
    SnowflakeIdGenerator snowflake_;
    OrmSchemaMap catalog_;
    std::unique_ptr<pool::IDbPool> dbpool_;
    std::unique_ptr<DDLVisitor> ddlVisitor_;
    std::unique_ptr<DMLVisitor> dmlVisitor_;
    // std::unique_ptr<QRYVisitor> qryVisitor_;
    void create_id(OrmProp& prop, jdoc& doc, std::string key);
};