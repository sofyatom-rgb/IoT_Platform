#pragma once

#include <drogon/drogon.h>

#include <stdexcept>

// Общий DbClient, созданный в main. Контроллеры берут его отсюда вместо
// drogon::app().getDbClient(), чтобы конфигурация БД была единственной.
class DbClientProvider {
 public:
  static void set(drogon::orm::DbClientPtr db) { instance().db_ = std::move(db); }

  static drogon::orm::DbClientPtr get() {
    if (!instance().db_) {
      throw std::runtime_error("Database client is not initialized");
    }
    return instance().db_;
  }

 private:
  static DbClientProvider &instance() {
    static DbClientProvider provider;
    return provider;
  }

  drogon::orm::DbClientPtr db_;
};
