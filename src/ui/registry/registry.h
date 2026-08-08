#ifndef REGISTRY_H
#define REGISTRY_H

/* registry.h — the cohesive apps registry (C11, opaque API).
 *
 * Parses the shared apps.json that unites the v1 e-ink menu and
 * the CM5 web UI. Format truth from the cohesive-update design:
 *   {
 *     "version": 1,
 *     "apps": [
 *       {"id":"clock","name":"Clock","type":"builtin","default":true},
 *       {"id":"weather","name":"Weather","type":"service",
 *        "endpoint":"http://localhost:8080/api/weather"},
 *       {"id":"ai-chat","name":"AI Chat","type":"service",
 *        "endpoint":"http://localhost:8081/v1/chat"}
 *     ]
 *   }
 * Minimal JSON parser — no third-party deps, C11 only.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct registry registry;

typedef struct registry_app {
    char id[64];
    char name[96];
    char type[32];       /* builtin | service | ai */
    char endpoint[256];  /* may be "" */
    int is_default;
} registry_app;

/* Parse an apps.json file. Returns NULL on error. */
registry *registry_load_file(const char *path);

/* Parse apps.json from a string (for tests). */
registry *registry_load(const char *json);

/* Number of apps. */
size_t registry_count(const registry *r);

/* App by index (0..count-1), or NULL. */
const registry_app *registry_get(const registry *r, size_t i);

/* First default app, or NULL. */
const registry_app *registry_default(const registry *r);

/* App by id, or NULL. */
const registry_app *registry_find(const registry *r, const char *id);

void registry_free(registry *r);

#ifdef __cplusplus
}
#endif

#endif /* REGISTRY_H */
