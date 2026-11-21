#pragma once

#include <drogon/drogon.h>

// HTTP-слой входа: POST /auth/login.
class AuthController {
 public:
  void registerRoutes();
};