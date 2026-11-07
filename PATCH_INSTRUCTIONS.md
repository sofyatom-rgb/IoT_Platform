# PATCH: расширенное тестовое покрытие IoT Platform

1. Создайте commit или резервную копию текущего проекта.
2. Откройте ZIP-архив и распакуйте его **с заменой файлов** в корень проекта `ars` — туда, где расположен корневой `CMakeLists.txt`. Пути внутри архива относительные.
3. Инструкции по PostgreSQL-интеграционным тестам находятся в [`docs/POSTGRES_TESTS.md`](docs/POSTGRES_TESTS.md).

## Быстрый набор unit/component-тестов (без PostgreSQL)

```sh
cmake -S . -B build-tests \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=OFF \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=OFF \
  -DBUILD_CODEC_TESTING=OFF
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests --output-on-failure -V
```

## Реальные SQL-репозитории + PostgreSQL

Набор требует Docker, Drogon, OpenSSL и JsonCpp:

```sh
./scripts/run-postgres-tests.sh
```

Скрипт использует отдельную БД `iot_platform_test` на порту `5433` и запускает только `repository_integration_tests`. База приложения `iot_platform` не меняется.

Чтобы пересоздать **только тестовую БД**:

```sh
./scripts/run-postgres-tests.sh --reset
```

Остальные тесты с реальными Drogon-контроллерами и локальным HTTP transport можно включить вручную; подробные команды находятся в [README.md](README.md).

В исходниках определено 113 `TEST_CASE`. Не все наборы запускались в среде, где создавался архив: сборка PostgreSQL/Drogon-наборов требует локальных зависимостей и работающего Docker PostgreSQL.
