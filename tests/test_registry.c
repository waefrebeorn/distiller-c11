/* test_registry.c — unit tests for the apps registry. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/registry/registry.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    const char *json =
        "{\"version\":1,"
        "\"apps\":["
        "{\"id\":\"clock\",\"name\":\"Clock\",\"type\":\"builtin\",\"default\":true},"
        "{\"id\":\"weather\",\"name\":\"Weather\",\"type\":\"service\","
        "\"endpoint\":\"http://localhost:8080/api/weather\"},"
        "{\"id\":\"ai-chat\",\"name\":\"AI Chat\",\"type\":\"ai\","
        "\"endpoint\":\"http://localhost:8081/v1/chat\"}"
        "]}";

    registry *r = registry_load(json);
    CHECK(r != NULL, "load valid json");
    CHECK(registry_count(r) == 3, "3 apps");
    CHECK(registry_count(NULL) == 0, "null count");

    const registry_app *a0 = registry_get(r, 0);
    CHECK(a0 != NULL && strcmp(a0->id, "clock") == 0, "app0 id clock");
    CHECK(strcmp(a0->name, "Clock") == 0, "app0 name Clock");
    CHECK(strcmp(a0->type, "builtin") == 0, "app0 type builtin");
    CHECK(a0->is_default == 1, "app0 is default");

    const registry_app *a1 = registry_get(r, 1);
    CHECK(strcmp(a1->endpoint, "http://localhost:8080/api/weather") == 0,
          "app1 endpoint parsed");

    const registry_app *d = registry_default(r);
    CHECK(d != NULL && strcmp(d->id, "clock") == 0, "default is clock");

    const registry_app *f = registry_find(r, "ai-chat");
    CHECK(f != NULL && strcmp(f->name, "AI Chat") == 0, "find ai-chat");
    CHECK(registry_find(r, "nope") == NULL, "find missing returns NULL");
    CHECK(registry_get(r, 99) == NULL, "get out of range NULL");

    registry_free(r);

    /* malformed json rejected */
    CHECK(registry_load("not json") == NULL, "garbage rejected");
    CHECK(registry_load("{\"version\":1}") == NULL, "missing apps rejected");
    CHECK(registry_load(NULL) == NULL, "null rejected");
    CHECK(registry_load_file("/nonexistent/registry.json") == NULL,
          "missing file rejected");

    /* empty apps array is valid */
    registry *e = registry_load("{\"version\":1,\"apps\":[]}");
    CHECK(e != NULL && registry_count(e) == 0, "empty apps array ok");
    CHECK(registry_default(e) == NULL, "no default in empty");
    registry_free(e);

    if (failures == 0) {
        printf("test_registry: ALL PASS\n");
        return 0;
    }
    printf("test_registry: %d FAILURES\n", failures);
    return 1;
}
