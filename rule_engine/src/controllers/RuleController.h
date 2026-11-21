#pragma once

#include <drogon/drogon.h>

/// Регистрирует HTTP-маршруты управления правилами.
class RuleController {
 public:
  /// Подключает маршруты CRUD и проверки правил.
  void registerRoutes();
};
