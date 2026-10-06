#include "json.h"

#include <stdlib.h>
#include <string.h>

// enough for any gltf header, one block, no per node malloc
#define POOL 1 << 20

static char *pool;
static size_t used;

static JVal *fresh(JKind kind) {
    if (used + sizeof(JVal) > POOL) return NULL;
    JVal *v = (JVal *)(pool + used);
    used += sizeof(JVal);
    v->kind = kind;
    v->num = 0.0;
    v->str = NULL;
    v->key = NULL;
    v->child = NULL;
    v->next = NULL;
    v->count = 0;
    return v;
}

static const char *skip(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static JVal *value(const char **pp);

static const char *string(const char **pp, const char **out) {
    const char *p = *pp;
    if (*p != '"') return NULL;
    p++;
    *out = p;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        p++;
    }
    if (*p != '"') return NULL;
    *(char *)p = '\0';
    *pp = p + 1;
    return p;
}

static JVal *value(const char **pp) {
    const char *p = skip(*pp);

    if (*p == '{' || *p == '[') {
        int obj = *p == '{';
        JVal *v = fresh(obj ? JOBJ : JARR);
        if (!v) return NULL;
        JVal *tail = NULL;
        p = skip(p + 1);
        while (*p && *p != (obj ? '}' : ']')) {
            JVal *item;
            const char *key = NULL;
            if (obj) {
                if (*p != '"') return NULL;
                if (!string(&p, &key)) return NULL;
                p = skip(p);
                if (*p != ':') return NULL;
                p++;
                item = value(&p);
            } else {
                item = value(&p);
            }
            if (!item) return NULL;
            item->key = key;
            if (tail) tail->next = item;
            else v->child = item;
            tail = item;
            v->count++;
            p = skip(p);
            if (*p == ',') p = skip(p + 1);
        }
        if (*p != (obj ? '}' : ']')) return NULL;
        *pp = p + 1;
        return v;
    }

    if (*p == '"') {
        const char *s;
        JVal *v = fresh(JSTR);
        if (!v || !string(&p, &s)) return NULL;
        v->str = s;
        *pp = p;
        return v;
    }

    if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0) {
        JVal *v = fresh(JBOOL);
        if (!v) return NULL;
        v->num = *p == 't';
        *pp = p + (*p == 't' ? 4 : 5);
        return v;
    }

    if (*p == 'n' && strncmp(p, "null", 4) == 0) {
        JVal *v = fresh(JNULL);
        if (!v) return NULL;
        *pp = p + 4;
        return v;
    }

    char *end;
    double d = strtod(p, &end);
    if (end == p) return NULL;
    JVal *v = fresh(JNUM);
    if (!v) return NULL;
    v->num = d;
    *pp = end;
    return v;
}

JVal *json_parse(const char *src) {
    pool = malloc(POOL);
    used = 0;
    if (!pool) return NULL;
    const char *p = src;
    JVal *root = value(&p);
    if (!root) {
        free(pool);
        pool = NULL;
    }
    return root;
}

JVal *json_get(const JVal *obj, const char *key) {
    if (!obj || obj->kind != JOBJ) return NULL;
    for (JVal *c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

JVal *json_at(const JVal *arr, int i) {
    if (!arr || arr->kind != JARR) return NULL;
    JVal *c = arr->child;
    while (c && i-- > 0) c = c->next;
    return c;
}

int json_floats(const JVal *arr, float *out, int n) {
    if (!arr || arr->kind != JARR) return 0;
    JVal *c = arr->child;
    int i = 0;
    for (; c && i < n; c = c->next) out[i++] = (float)c->num;
    return i == n;
}

void json_free(void) {
    free(pool);
    pool = NULL;
}
