/* registry.c — the cohesive apps registry (C11).
 *
 * Minimal JSON parser handling exactly the shape apps.json uses:
 * string values, numbers, booleans, arrays of objects. Anything
 * malformed is rejected (returns NULL) rather than silently
 * accepted — the registry is the device's source of truth.
 */

#include "registry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct registry {
    registry_app *apps;
    size_t count;
    size_t cap;
};

/* --- tiny JSON tokenizer --- */

typedef struct jp {
    const char *s;
    const char *p;
    int err;
} jp;

static void jskip(jp *j)
{
    while (*j->p && isspace((unsigned char)*j->p)) {
        j->p++;
    }
}

static int jchar(jp *j, char c)
{
    jskip(j);
    if (*j->p == c) {
        j->p++;
        return 1;
    }
    return 0;
}

static char *jstring(jp *j)
{
    jskip(j);
    if (*j->p != '"') {
        j->err = 1;
        return NULL;
    }
    j->p++;
    const char *start = j->p;
    while (*j->p && *j->p != '"') {
        if (*j->p == '\\') {
            j->p++; /* skip escaped char */
        }
        j->p++;
    }
    if (*j->p != '"') {
        j->err = 1;
        return NULL;
    }
    size_t len = (size_t)(j->p - start);
    char *out = malloc(len + 1);
    if (out == NULL) {
        j->err = 1;
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    j->p++;
    return out;
}

/* --- registry implementation --- */

static registry_app *reg_add(registry *r)
{
    if (r->count == r->cap) {
        size_t ncap = r->cap ? r->cap * 2 : 8;
        registry_app *na = realloc(r->apps, ncap * sizeof(registry_app));
        if (na == NULL) {
            return NULL;
        }
        r->apps = na;
        r->cap = ncap;
    }
    registry_app *a = &r->apps[r->count];
    memset(a, 0, sizeof(*a));
    r->count++;
    return a;
}

registry *registry_load(const char *json)
{
    if (json == NULL) {
        return NULL;
    }
    registry *r = calloc(1, sizeof(*r));
    if (r == NULL) {
        return NULL;
    }
    jp j = { json, json, 0 };
    if (!jchar(&j, '{')) {
        registry_free(r);
        return NULL;
    }
    /* version field */
    if (jchar(&j, '"')) {
        j.p--; /* put back the quote; jstring wants to read it */
        char *k = jstring(&j);
        if (k == NULL) {
            registry_free(r);
            return NULL;
        }
        free(k);
        if (!jchar(&j, ':')) {
            registry_free(r);
            return NULL;
        }
        jskip(&j);
        while (*j.p && *j.p != ',' && *j.p != '}') {
            j.p++; /* skip version value */
        }
    }
    if (!jchar(&j, ',')) {
        registry_free(r);
        return NULL;
    }
    /* apps array */
    char *k = jstring(&j);
    if (k == NULL || strcmp(k, "apps") != 0) {
        free(k);
        registry_free(r);
        return NULL;
    }
    free(k);
    if (!jchar(&j, ':') || !jchar(&j, '[')) {
        registry_free(r);
        return NULL;
    }
    /* each app object */
    for (;;) {
        jskip(&j);
        if (*j.p == ']') {
            j.p++;
            break;
        }
        if (!jchar(&j, '{')) {
            registry_free(r);
            return NULL;
        }
        registry_app *a = reg_add(r);
        if (a == NULL) {
            registry_free(r);
            return NULL;
        }
        for (;;) {
            char *fk = jstring(&j);
            if (fk == NULL) {
                registry_free(r);
                return NULL;
            }
            if (!jchar(&j, ':')) {
                free(fk);
                registry_free(r);
                return NULL;
            }
            jskip(&j);
            if (*j.p == '"') {
                char *v = jstring(&j);
                if (v == NULL) {
                    free(fk);
                    registry_free(r);
                    return NULL;
                }
                if (strcmp(fk, "id") == 0) {
                    snprintf(a->id, sizeof(a->id), "%s", v);
                } else if (strcmp(fk, "name") == 0) {
                    snprintf(a->name, sizeof(a->name), "%s", v);
                } else if (strcmp(fk, "type") == 0) {
                    snprintf(a->type, sizeof(a->type), "%s", v);
                } else if (strcmp(fk, "endpoint") == 0) {
                    snprintf(a->endpoint, sizeof(a->endpoint), "%s", v);
                }
                free(v);
            } else if (strncmp(j.p, "true", 4) == 0) {
                if (strcmp(fk, "default") == 0) {
                    a->is_default = 1;
                }
                j.p += 4;
            } else {
                /* number or false: skip token */
                while (*j.p && *j.p != ',' && *j.p != '}') {
                    j.p++;
                }
            }
            free(fk);
            if (jchar(&j, '}')) {
                break;
            }
            if (!jchar(&j, ',')) {
                registry_free(r);
                return NULL;
            }
        }
        jskip(&j);
        if (*j.p == ',') {
            j.p++;
            continue;
        }
        if (*j.p == ']') {
            j.p++;
            break;
        }
        registry_free(r);
        return NULL;
    }
    jskip(&j);
    if (*j.p != '}') {
        registry_free(r);
        return NULL;
    }
    return r;
}

registry *registry_load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1 << 20)) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    registry *r = registry_load(buf);
    free(buf);
    return r;
}

size_t registry_count(const registry *r)
{
    return r ? r->count : 0;
}

const registry_app *registry_get(const registry *r, size_t i)
{
    if (r == NULL || i >= r->count) {
        return NULL;
    }
    return &r->apps[i];
}

const registry_app *registry_default(const registry *r)
{
    if (r == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < r->count; i++) {
        if (r->apps[i].is_default) {
            return &r->apps[i];
        }
    }
    return r->count ? &r->apps[0] : NULL;
}

const registry_app *registry_find(const registry *r, const char *id)
{
    if (r == NULL || id == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < r->count; i++) {
        if (strcmp(r->apps[i].id, id) == 0) {
            return &r->apps[i];
        }
    }
    return NULL;
}

void registry_free(registry *r)
{
    if (r == NULL) {
        return;
    }
    free(r->apps);
    free(r);
}
