#include "json.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 32

typedef struct {
    const char *p;
    int depth;
} parser_t;

static jnode_t *parse_value(parser_t *ps);

static void skip_ws(parser_t *ps)
{
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r')
        ps->p++;
}

static jnode_t *new_node(jtype_t type)
{
    jnode_t *n = calloc(1, sizeof(*n));
    if (n)
        n->type = type;
    return n;
}

static int hex4(const char *s, unsigned *out)
{
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return -1;
    }
    *out = v;
    return 0;
}

static size_t put_utf8(char *dst, unsigned cp)
{
    if (cp < 0x80) {
        dst[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        dst[0] = (char)(0xC0 | (cp >> 6));
        dst[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    dst[0] = (char)(0xE0 | (cp >> 12));
    dst[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dst[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

/* Parses a string starting at the opening quote; returns malloc'd text. */
static char *parse_string(parser_t *ps)
{
    if (*ps->p != '"')
        return NULL;
    ps->p++;

    /* The unescaped text is never longer than the escaped one. */
    const char *end = ps->p;
    while (*end && *end != '"') {
        if (*end == '\\' && end[1])
            end++;
        end++;
    }
    if (*end != '"')
        return NULL;

    char *out = malloc((size_t)(end - ps->p) + 1);
    if (!out)
        return NULL;
    char *w = out;
    while (ps->p < end) {
        char c = *ps->p++;
        if (c != '\\') {
            *w++ = c;
            continue;
        }
        c = *ps->p++;
        switch (c) {
        case 'b': *w++ = '\b'; break;
        case 'f': *w++ = '\f'; break;
        case 'n': *w++ = '\n'; break;
        case 'r': *w++ = '\r'; break;
        case 't': *w++ = '\t'; break;
        case 'u': {
            unsigned cp;
            if (ps->p + 4 > end || hex4(ps->p, &cp) != 0) {
                free(out);
                return NULL;
            }
            ps->p += 4;
            if (cp >= 0xD800 && cp <= 0xDFFF)
                cp = '?'; /* surrogate pairs are not supported */
            w += put_utf8(w, cp);
            break;
        }
        default: *w++ = c; break; /* \" \\ \/ */
        }
    }
    *w = '\0';
    ps->p = end + 1;
    return out;
}

static jnode_t *parse_container(parser_t *ps, jtype_t type)
{
    char close = type == J_ARR ? ']' : '}';
    jnode_t *n = new_node(type);
    jnode_t **tail;

    if (!n || ps->depth >= MAX_DEPTH) {
        free(n);
        return NULL;
    }
    ps->depth++;
    ps->p++; /* [ or { */
    tail = &n->child;
    skip_ws(ps);
    if (*ps->p == close) {
        ps->p++;
        ps->depth--;
        return n;
    }
    for (;;) {
        char *key = NULL;
        skip_ws(ps);
        if (type == J_OBJ) {
            key = parse_string(ps);
            skip_ws(ps);
            if (!key || *ps->p != ':') {
                free(key);
                goto fail;
            }
            ps->p++;
            skip_ws(ps);
        }
        jnode_t *v = parse_value(ps);
        if (!v) {
            free(key);
            goto fail;
        }
        v->key = key;
        *tail = v;
        tail = &v->next;
        n->count++;
        skip_ws(ps);
        if (*ps->p == ',') {
            ps->p++;
            continue;
        }
        if (*ps->p == close) {
            ps->p++;
            ps->depth--;
            return n;
        }
        goto fail;
    }
fail:
    json_free(n);
    return NULL;
}

static jnode_t *parse_value(parser_t *ps)
{
    skip_ws(ps);
    char c = *ps->p;
    jnode_t *n;

    if (c == '{')
        return parse_container(ps, J_OBJ);
    if (c == '[')
        return parse_container(ps, J_ARR);
    if (c == '"') {
        char *s = parse_string(ps);
        if (!s)
            return NULL;
        n = new_node(J_STR);
        if (!n) {
            free(s);
            return NULL;
        }
        n->str = s;
        return n;
    }
    if (strncmp(ps->p, "true", 4) == 0 || strncmp(ps->p, "false", 5) == 0) {
        int t = ps->p[0] == 't';
        n = new_node(J_BOOL);
        if (n) {
            n->num = t;
            ps->p += t ? 4 : 5;
        }
        return n;
    }
    if (strncmp(ps->p, "null", 4) == 0) {
        ps->p += 4;
        return new_node(J_NULL);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char *end;
        double v = strtod(ps->p, &end);
        if (end == ps->p)
            return NULL;
        n = new_node(J_NUM);
        if (n) {
            n->num = v;
            ps->p = end;
        }
        return n;
    }
    return NULL;
}

jnode_t *json_parse(const char *text)
{
    parser_t ps = { text, 0 };
    jnode_t *n = parse_value(&ps);
    if (!n)
        return NULL;
    skip_ws(&ps);
    if (*ps.p != '\0') {
        json_free(n);
        return NULL;
    }
    return n;
}

void json_free(jnode_t *n)
{
    while (n) {
        jnode_t *next = n->next;
        json_free(n->child);
        free(n->str);
        free(n->key);
        free(n);
        n = next;
    }
}

const jnode_t *json_get(const jnode_t *obj, const char *key)
{
    if (!obj || obj->type != J_OBJ)
        return NULL;
    for (const jnode_t *c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0)
            return c;
    return NULL;
}

const jnode_t *json_at(const jnode_t *arr, int idx)
{
    if (!arr || arr->type != J_ARR || idx < 0)
        return NULL;
    const jnode_t *c = arr->child;
    while (c && idx-- > 0)
        c = c->next;
    return c;
}
