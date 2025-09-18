#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "lib.hpp"
#include "ulid.hpp"
#include "dbpool.hpp"
#include "snowflake.hpp"
#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"
// // #include "schemaupdate.hpp"
// // #include "sqlconnection.hpp"


using namespace std::literals::chrono_literals;


class SchemaCatalog;

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage : public std::enable_shared_from_this<Storage> {
private:
    std::unique_ptr<SnowflakeIdGenerator> snowflake_ ;
    std::unique_ptr<DbPool              > dbpool_    ;
    std::shared_ptr<DDLVisitor          > ddlVisitor_;
    std::shared_ptr<DMLVisitor          > dmlVisitor_;
    std::shared_ptr<SchemaCatalog       > catalog_   ;

    bool createSchemaStorage(const OrmSchema& schema, SQLConnection &conn);

    bool applyUpdates(const OrmSchema* old_schema, const OrmSchema& new_schema) const;

    bool insertCatalog(const OrmSchema& new_schema, SQLConnection *conn = nullptr);

    friend SchemaCatalog;
public:
    Storage(const std::string &db_path, Dialect dialect); // constructor
    ~Storage() = default;
    /*************** BOOTSTRAP CATALOG *************/
    bool init_catalog();

    /*************** SCHEMA MANAGE *************/
    bool addSchema(std::string &JSONSchema);

    bool addSchema(OrmSchema& schema);

    bool remSchema(std::string &name);

    /*************** SCHEMA INITALIZATION *************/
    bool init_schemas(const std::vector<std::string> &schema_names);

    bool init_schema(const OrmSchema &schema);

    /*************** SCHEMA ACCESS *************/
    bool getSchema(std::string &name, OrmSchema& schema);

    /*************** DML FUNCTIONS *************/
    void create_id(const std::shared_ptr<OrmProp> idprop, jval &val, jdaloc &aloc, SQLConnection *conn = nullptr);

    int insert(const std::string &schemaName, jval& data, const std::string &trackinfo="", const std::string &key="");

    int insert(SQLConnection &conn, const OrmSchema &schema, jval &data, const std::string &trackinfo="", const std::string &key="");

    int update(const std::string &schemaName, jval& value, const std::string &trackinfo);

    int update(SQLConnection& conn, OrmSchema& schema, jval& value, const std::string &trackinfo);

    void del(const std::string &name, const jval& value, const std::string &user = "", const std::string &context = "");

    bool select(std::string &sql, std::string &json_string); //execute SQL with resultset

    template <class F>
    auto with_conn(pool::DbIntent intent, F&& fn) -> std::optional<std::invoke_result_t<F, SQLConnection&>> {
        auto ac = dbpool_->acquire(intent, 1000ms);
        if (!ac.ok) return std::nullopt;
        auto& lease = ac.lease; // lease keeps conn - on out of scope - release
        SQLConnection& conn = lease.conn();

        return std::optional<std::invoke_result_t<F, SQLConnection&>> {
            std::forward<F>(fn)(conn) // call lambda to operate conn while lease in scope
        };
    }

    template <class F, class T = std::invoke_result_t<F, SQLConnection&>>
    T with_conn(pool::DbIntent intent, F&& fn, T fallback) {
        auto r = with_conn(intent, std::forward<F>(fn));
        return r ? *r : std::move(fallback);
    }

    template <class F>
    auto with_tr(pool::DbIntent intent, F&& fn) const -> std::optional<std::invoke_result_t<F, SQLConnection&>> {
        auto ac = dbpool_->acquire(intent, 1000ms);
        if (!ac.ok) return std::nullopt;
        auto& lease = ac.lease; // keep lease alive for whole transction until out of scope - release
        SQLConnection& conn = lease.conn();

        if (!conn.begin()) THROW("begin() failed");
        try {
            using R = std::invoke_result_t<F, SQLConnection&>;

            R result = std::forward<F>(fn)(conn);

            if (!conn.commit()) {
                conn.rollback();
                THROW("commit() failed - transaction rolled back");
            }
            return std::optional<R> { std::move(result) };
        } catch (...) {
            conn.rollback();
            throw; // propagate
        }
    }

};