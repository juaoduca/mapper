#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include <memory>

#include "catch.hpp"
#include "dbpool.hpp"
#include "sqlconnection.hpp"


using namespace std::chrono_literals;
// using pool::AcquirePolicy;
// using DbIntent;
// using Lease;
// using PoolAcquireError;
// using PoolStats;

// -----------Fake SQLStatement ---------

class FakeSQLStatement : public SQLStatement {
public:
    struct BoundValue {
        int idx;
        const jval& value;
        std::string type;
    };

    explicit FakeSQLStatement(int rows_to_affect = 1)
        : rows_to_affect_(rows_to_affect), exec_count_(0) {}

    void bind(int idx, const jval& value, const PropType& type) override {
        bound_values_.push_back({idx, value, proptype(type) });
    }

    int exec() override {
        ++exec_count_;
        return rows_to_affect_;
    }

    void bind_null(int idx) override {

    }

    void bind_text(int idx, std::string value) override {

    }

    // Helpers for tests
    const std::vector<BoundValue>& boundValues() const { return bound_values_; }
    int execCount() const { return exec_count_; }
    void setRowsToAffect(int rows) { rows_to_affect_ = rows; }

private:
    int rows_to_affect_;
    int exec_count_;
    std::vector<BoundValue> bound_values_;
};

// ---------- Fake connections ----------
class FakeConn : public SQLConnection
{
public:
    explicit FakeConn(int id) : id_(id) {}
    int id() const { return id_; }

    virtual void createDB(const std::string &dsn) override {};

    void connect(const std::string &dsn) override
    {
        // No-op for fake
    }

    void disconnect() noexcept override
    {
        // No-op for fake
    }


    std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams) const override {
        return std::make_unique<FakeSQLStatement>();
    };

    bool begin () override { return true;};
    bool commit() override {return true;};
    void rollback() override {/*no op*/};

    u_int64_t nextValue(const std::string &name) override {return 1;}

    bool execDDL(const std::string &sql) override
    {
        return true; // Always succeed
    }

    int execDML(const std::string &dml, void *result) override
    {
        return false; // Pretend 1 row affected
    }

    bool execGET(const std::string &sql, char **result) override { return false; }

private:
    int id_;
};

// ---------- Deterministic FakePool (no templates) ----------
class FakePool : public pool::IDbPool
{
public:
    FakePool(std::size_t readCap, std::size_t writeCap, bool writerPriority, pool::AcquirePolicy pol)
        : read_cap_(readCap), write_cap_(writeCap), writer_priority_(writerPriority), policy_(pol) {}

    pool::IDbPool::AcquireResult acquire(pool::DbIntent intent, std::chrono::milliseconds to = std::chrono::milliseconds::zero())
    {
        auto deadline = std::chrono::steady_clock::now() + (to.count() > 0 ? to : policy_.acquire_timeout);
        std::unique_lock<std::mutex> lk(mx_);
        ++waiters_;
        auto on_exit = Finally([&]
                               { --waiters_; });

        auto can_get = [&]
        {
            if (intent == pool::DbIntent::Read)
            {
                if (writer_priority_ && writers_waiting_ > 0)
                    return false;
                return in_use_read_ < read_cap_;
            }
            else
            {
                return in_use_write_ < write_cap_;
            }
        };

        if (intent == pool::DbIntent::Write)
            ++writers_waiting_;
        while (!can_get())
        {
            if (cv_.wait_until(lk, deadline) == std::cv_status::timeout)
            {
                if (intent == pool::DbIntent::Write)
                    --writers_waiting_;
                return {false, pool::Lease(nullptr, nullptr, intent), pool::PoolAcquireError::Timeout};
            }
            if (shutdown_)
            {
                if (intent == pool::DbIntent::Write)
                    --writers_waiting_;
                return {false, pool::Lease(nullptr, nullptr, intent), pool::PoolAcquireError::Shutdown};
            }
        }

        // Checkout
        std::shared_ptr<SQLConnection> c = std::make_shared<FakeConn>(++total_created_);
        if (intent == pool::DbIntent::Read)
            ++in_use_read_;
        else
            ++in_use_write_;
        in_use_ = in_use_read_ + in_use_write_;

        if (intent == pool::DbIntent::Write)
            --writers_waiting_;

        return {true, pool::Lease(this, std::move(c), intent), pool::PoolAcquireError::Timeout};
    }

    pool::PoolStats stats() const
    {
        std::scoped_lock lk(mx_);
        return pool::PoolStats{total_created_, in_use_, waiters_};
    }

    void shutdown()
    {
        std::scoped_lock lk(mx_);
        shutdown_ = true;
        cv_.notify_all();
    }

protected:
    void release(std::shared_ptr<SQLConnection> /*conn*/, pool::DbIntent intent)
    {
        std::scoped_lock lk(mx_);
        if (intent == pool::DbIntent::Read)
        {
            --in_use_read_;
        }
        else
        {
            --in_use_write_;
        }
        in_use_ = in_use_read_ + in_use_write_;
        cv_.notify_all();
    }

private:
    // tiny scope guard
    struct Finally
    {
        explicit Finally(std::function<void()> f) : f_(std::move(f)) {}
        ~Finally()
        {
            if (f_)
                f_();
        }
        std::function<void()> f_;
    };

    mutable std::mutex mx_;
    std::condition_variable cv_;
    std::size_t total_created_{0};
    std::size_t in_use_{0};
    std::size_t in_use_read_{0};
    std::size_t in_use_write_{0};
    std::size_t waiters_{0};
    std::size_t writers_waiting_{0};
    const std::size_t read_cap_;
    const std::size_t write_cap_;
    const bool writer_priority_;
    const pool::AcquirePolicy policy_;
    bool shutdown_{false};
};

// ------------------------ Tests ------------------------

TEST_CASE("RAII release returns connection (OO)", "[pool][raii][oo]")
{
    pool::AcquirePolicy pol;
    pol.acquire_timeout = 200ms;
    FakePool pool(/*readCap*/ 2, /*writeCap*/ 2, /*writerPriority*/ false, pol);

    auto a = pool.acquire(pool::DbIntent::Read);
    REQUIRE(a.ok);
    CHECK(pool.stats().in_use == 1u);
    {
        auto b = pool.acquire(pool::DbIntent::Read);
        REQUIRE(b.ok);
        CHECK(pool.stats().in_use == 2u);
        //b.lea­se;// auto‑releases at scope end
    }
    CHECK(pool.stats().in_use == 1u);
}

TEST_CASE("Acquire times out when exhausted (OO)", "[pool][timeout][oo]")
{
    pool::AcquirePolicy pol;
    pol.acquire_timeout = 100ms;
    FakePool pool(/*readCap*/ 0, /*writeCap*/ 0, /*writerPriority*/ false, pol);

    auto r = pool.acquire(pool::DbIntent::Read);
    REQUIRE_FALSE(r.ok);
    CHECK(r.error == pool::PoolAcquireError::Timeout);
}

TEST_CASE("SQLite-like writer priority (OO)", "[sqlite][fairness][oo]")
{
    pool::AcquirePolicy pol;
    pol.acquire_timeout = 500ms;
    FakePool pool(/*readCap*/ 1, /*writeCap*/ 1, /*writerPriority*/ true, pol);

    // occupy the only read slot
    std::optional<pool::Lease> r1 = pool.acquire(pool::DbIntent::Read).lease;
    REQUIRE((r1 && *r1));

    std::atomic<bool> writer_got{false}, late_reader_returned{false};
    std::thread writer([&]
                       {
    auto w = pool.acquire(pool::DbIntent::Write);
    REQUIRE(w.ok);
    writer_got = true;
    std::this_thread::sleep_for(50ms); });

    std::thread reader2([&]
                        {
    auto r2 = pool.acquire(pool::DbIntent::Read);
    REQUIRE(r2.ok);
    late_reader_returned = true; });

    std::this_thread::sleep_for(100ms);
    CHECK(writer_got.load());
    CHECK_FALSE(late_reader_returned.load()); // writer should go first

    writer.join();
    r1.reset(); // release the first reader
    reader2.join();
    CHECK(late_reader_returned.load());
}

TEST_CASE("Shutdown wakes waiters (OO)", "[pool][shutdown][oo]")
{
    pool::AcquirePolicy pol;
    pol.acquire_timeout = 10s;
    FakePool pool(/*readCap*/ 0, /*writeCap*/ 0, /*writerPriority*/ true, pol);

    std::atomic<bool> saw_shutdown{false};
    std::thread waiter([&]
                       {
    auto r = pool.acquire(pool::DbIntent::Write);
    if (!r.ok) saw_shutdown = (r.error == pool::PoolAcquireError::Shutdown); });

    std::this_thread::sleep_for(100ms);
    pool.shutdown();
    waiter.join();
    CHECK(saw_shutdown.load());
}
