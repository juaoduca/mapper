#pragma once
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <sqlconnection.hpp>
#include <stdexcept>

using PConn = std::shared_ptr<SQLConnection>;

PSQLConnection make_postgres_connection();
PSQLConnection make_sqlite_connection();

namespace pool {

enum class DbIntent { Read,
    Write };
enum class PoolAcquireError { Timeout,
    Shutdown };

struct PoolStats {
    std::size_t size { 0 };
    std::size_t in_use { 0 };
    std::size_t waiters { 0 };
};

struct AcquirePolicy {
    std::chrono::milliseconds acquire_timeout { 1500 }; // never block forever
    std::chrono::milliseconds max_lease_time { 0 }; // 0 = disabled (tests/guardrail)
};

// Forward decl
class IDbPool;

// --------- RAII Lease (no templates) ----------
class Lease {
public:
    Lease(IDbPool* owner, PConn conn, DbIntent intent)
        : owner_(owner)
        , conn_(std::move(conn))
        , intent_(intent) { }

    Lease(const Lease&) = delete; // disable copy constructor
    Lease& operator=(const Lease&) = delete; // disable copy assignment operator a=b (a receives b);

    Lease(Lease&& other) noexcept { move_from(other); }
    Lease& operator=(Lease&& other) noexcept {
        if (this != &other) {
            release_();
            move_from(other);
        }
        return *this;
    }

    ~Lease() { release_(); }

    SQLConnection& conn() const { return *conn_; }
    std::shared_ptr<SQLConnection> shared() const { return conn_; }
    DbIntent intent() const { return intent_; }
    explicit operator bool() const { return !!conn_; }

private:
    void release_();
    void move_from(Lease& o) noexcept {
        owner_ = o.owner_;
        o.owner_ = nullptr;
        conn_ = std::move(o.conn_);
        intent_ = o.intent_;
    }

    IDbPool* owner_ { nullptr };
    std::shared_ptr<SQLConnection> conn_ {};
    DbIntent intent_ { DbIntent::Read };

    friend class IDbPool;
};

// --------- Pool interface (polymorphic) ----------
class IDbPool {
public:
    virtual ~IDbPool() = default;

    struct AcquireResult {
        bool ok { false };
        Lease lease { nullptr, nullptr, DbIntent::Read };
        PoolAcquireError error { PoolAcquireError::Timeout };
    };

    virtual AcquireResult acquire(DbIntent intent,
        std::chrono::milliseconds timeoutOverride = std::chrono::milliseconds::zero())
        = 0;

    virtual PoolStats stats() const = 0;
    virtual void shutdown() = 0;

protected:
    // Only pools are allowed to “return” leases:
    virtual void release(std::shared_ptr<SQLConnection> conn, DbIntent intent) = 0;
    friend class Lease;
};

// Lease releases back to its owner (no friend gymnastics)
inline void Lease::release_() {
    if (owner_ && conn_) {
        owner_->release(conn_, intent_);
    }
    owner_ = nullptr;
    conn_.reset();
}

} // namespace mapper

class DbPool final : public pool::IDbPool {
public:
    DbPool(std::size_t capacity,
        std::string dsn,
        std::function<PSQLConnection()> factory,
        pool::AcquirePolicy policy = {})
        : cap_(capacity)
        , dsn_(std::move(dsn))
        , factory_(std::move(factory))
        , policy_(policy) { load_(); }

    AcquireResult acquire(
        pool::DbIntent intent,
        std::chrono::milliseconds to = std::chrono::milliseconds::zero()) override {
        auto deadline = std::chrono::steady_clock::now() + (to.count() ? to : policy_.acquire_timeout);

        std::unique_lock<std::mutex> lk(mx_);
        ++stats_.waiters;
        auto on_exit = Finally([&]() { --stats_.waiters; });

        while (!shutdown_ && free_.empty()) {
            if (cv_.wait_until(lk, deadline) == std::cv_status::timeout) {
                return { false, { nullptr, nullptr, pool::DbIntent::Read }, pool::PoolAcquireError::Timeout };
            }
        }
        if (shutdown_) {
            return { false, { nullptr, nullptr, pool::DbIntent::Read }, pool::PoolAcquireError::Shutdown };
        }

        auto conn = std::move(free_.front());
        free_.pop_front();
        ++stats_.in_use;

        return { true, pool::Lease { this, std::move(conn), intent }, {} };
    }

    pool::PoolStats stats() const override {
        std::lock_guard<std::mutex> lk(mx_);
        auto s = stats_;
        s.size = cap_;
        return s;
    }

    void shutdown() override {
        std::lock_guard<std::mutex> lk(mx_);
        shutdown_ = true;
        cv_.notify_all();
    }

protected:
    void release(std::shared_ptr<SQLConnection> conn, pool::DbIntent) override {
        std::lock_guard<std::mutex> lk(mx_);
        if (conn && !shutdown_) {
            free_.push_back(std::move(conn));
        }
        if (stats_.in_use)
            --stats_.in_use;
        cv_.notify_one();
    }

private:
    struct Finally {
        std::function<void()> f;
        explicit Finally(std::function<void()> fn)
            : f(std::move(fn)) { }
        ~Finally() {
            if (f) {
                f();
            }
        }
        // non-copyable
        Finally(const Finally&) = delete;
        Finally& operator=(const Finally&) = delete;
        // movable
        Finally(Finally&& other) noexcept
            : f(std::move(other.f)) { }
        Finally& operator=(Finally&& other) noexcept {
            f = std::move(other.f);
            return *this;
        }
    };

    void load_() {
        std::lock_guard<std::mutex> lk(mx_);
        free_.clear();
        for (std::size_t i = 0; i < cap_; ++i) {
            if (!factory_) throw std::runtime_error("DbPool: null connection factory");
            PSQLConnection up = factory_();
            if (!up) throw std::runtime_error("DbPool: factory returned null connection");
            up->connect(dsn_);
            // move unique_ptr -> shared_ptr with custom deleter
            std::shared_ptr<SQLConnection> sp(up.release(), [](SQLConnection* p) { delete p; });
            free_.push_back(std::move(sp));
        }
        stats_ = {};
        stats_.size = cap_;
    }

    std::size_t cap_;
    std::string dsn_;
    pool::AcquirePolicy policy_;

    mutable std::mutex mx_;
    std::condition_variable cv_;
    std::deque<std::shared_ptr<SQLConnection>> free_;
    bool shutdown_ { false };
    pool::PoolStats stats_;
    std::function<PSQLConnection()> factory_;
};
