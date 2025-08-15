#pragma once
#include <chrono>
#include <memory>
#include <cstddef>
#include <sqlconnection.hpp>

using PConn = std::shared_ptr<SQLConnection>;

namespace mapper {

enum class DbIntent { Read, Write };
enum class PoolAcquireError { Timeout, Shutdown };

struct PoolStats {
  std::size_t size{0};
  std::size_t in_use{0};
  std::size_t waiters{0};
};

struct AcquirePolicy {
  std::chrono::milliseconds acquire_timeout{1500};  // never block forever
  std::chrono::milliseconds max_lease_time{0};      // 0 = disabled (tests/guardrail)
};

// --------- Connections (polymorphic) ----------
// class SQLConnection {
// public:
//   virtual ~SQLConnection() = default;
//   // optional: health/metadata APIs; not needed for tests right now.
// };

// Forward decl
class IDbPool;

// --------- RAII Lease (no templates) ----------
class Lease {
public:
  Lease(IDbPool* owner, PConn conn, DbIntent intent)
  : owner_(owner), conn_(std::move(conn)), intent_(intent) {}

  Lease(const Lease&) = delete; // disable copy constructor
  Lease& operator=(const Lease&) = delete; // disable copy assignment operator a=b (a receives b);

  Lease(Lease&& other) noexcept { move_from(other); }
  Lease& operator=(Lease&& other) noexcept {
    if (this != &other) { release_(); move_from(other); }
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
    owner_  = o.owner_;
    o.owner_ = nullptr;
    conn_   = std::move(o.conn_);
    intent_ = o.intent_;
  }

  IDbPool* owner_{nullptr};
  std::shared_ptr<SQLConnection> conn_{};
  DbIntent intent_{DbIntent::Read};

  friend class IDbPool;
};

// --------- Pool interface (polymorphic) ----------
class IDbPool {
public:
  virtual ~IDbPool() = default;

  struct AcquireResult {
    bool ok{false};
    Lease lease{nullptr, nullptr, DbIntent::Read};
    PoolAcquireError error{PoolAcquireError::Timeout};
  };

  virtual AcquireResult acquire(DbIntent intent,
                                std::chrono::milliseconds timeoutOverride = std::chrono::milliseconds::zero()) = 0;

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

} // namespace mapper_oo
