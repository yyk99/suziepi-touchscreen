#include "influx.h"

#include "json.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BODY (512 * 1024)

static void urlencode(const char *s, char *out, size_t cap)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;

    for (; *s && n + 4 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[n++] = (char)c;
        } else {
            out[n++] = '%';
            out[n++] = hex[c >> 4];
            out[n++] = hex[c & 15];
        }
    }
    out[n] = '\0';
}

/* Run an InfluxQL query. Returns the malloc'd JSON body, or NULL. */
static char *influx_query(const influx_conn_t *c, const char *ql)
{
    char qenc[512], dbenc[96], req[1024];
    int fd = net_connect(c->host, c->port, c->timeout_s);

    if (fd < 0)
        return NULL;
    urlencode(ql, qenc, sizeof(qenc));
    urlencode(c->db, dbenc, sizeof(dbenc));
    int rl = snprintf(req, sizeof(req),
                      "GET /query?db=%s&q=%s HTTP/1.0\r\n"
                      "Host: %s:%d\r\nAccept: application/json\r\nConnection: close\r\n\r\n",
                      dbenc, qenc, c->host, c->port);
    if (rl <= 0 || rl >= (int)sizeof(req) || send(fd, req, (size_t)rl, 0) != rl) {
        close(fd);
        return NULL;
    }

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    while (buf) {
        if (len + 1024 >= cap) {
            if (cap >= MAX_BODY) {
                free(buf);
                buf = NULL;
                break;
            }
            char *nb = realloc(buf, cap * 2);
            if (!nb) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = nb;
            cap *= 2;
        }
        ssize_t r = recv(fd, buf + len, cap - len - 1, 0);
        if (r < 0) {
            free(buf);
            buf = NULL;
            break;
        }
        if (r == 0)
            break;
        len += (size_t)r;
    }
    close(fd);
    if (!buf)
        return NULL;
    buf[len] = '\0';

    char *body = strstr(buf, "\r\n\r\n");
    if (strncmp(buf, "HTTP/1.", 7) != 0 || !strstr(buf, " 200") || strstr(buf, " 200") > buf + 12 ||
        !body) {
        free(buf);
        return NULL;
    }
    body = strdup(body + 4);
    free(buf);
    return body;
}

time_t influx_parse_time(const char *s)
{
    struct tm tm;
    int y, mo, d, h, mi, sec;

    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6)
        return (time_t)-1;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = sec;
    return timegm(&tm);
}

/* results[0].series[0], or NULL. Sets *err when the reply carries an error. */
static const jnode_t *first_series(const jnode_t *root, int *err)
{
    const jnode_t *r0 = json_at(json_get(root, "results"), 0);

    *err = 0;
    if (!r0 || json_get(root, "error") || json_get(r0, "error")) {
        *err = 1;
        return NULL;
    }
    return json_at(json_get(r0, "series"), 0);
}

int influx_parse_measurements(const char *json, influx_names_t *out)
{
    jnode_t *root = json_parse(json);
    int err, rc = 0;

    memset(out, 0, sizeof(*out));
    if (!root)
        return -1;
    const jnode_t *s = first_series(root, &err);
    if (err) {
        rc = -1;
    } else if (s) {
        for (const jnode_t *row = json_get(s, "values") ? json_get(s, "values")->child : NULL;
             row && out->count < INFLUX_MAX_MEASUREMENTS; row = row->next) {
            const jnode_t *name = json_at(row, 0);
            if (name && name->type == J_STR)
                snprintf(out->name[out->count++], INFLUX_NAME_LEN, "%s", name->str);
        }
    }
    json_free(root);
    return rc;
}

int influx_parse_reading(const char *json, influx_reading_t *out)
{
    jnode_t *root = json_parse(json);
    int err, rc = 1;

    memset(out, 0, sizeof(*out));
    if (!root)
        return -1;
    const jnode_t *s = first_series(root, &err);
    if (err) {
        rc = -1;
    } else if (s) {
        const jnode_t *cols = json_get(s, "columns");
        const jnode_t *row = json_at(json_get(s, "values"), 0);
        if (cols && row) {
            int i = 0;
            rc = 0;
            for (const jnode_t *c = cols->child; c; c = c->next, i++) {
                const jnode_t *v = json_at(row, i);
                if (c->type != J_STR || !v)
                    continue;
                if (strcmp(c->str, "time") == 0 && v->type == J_STR) {
                    out->time = influx_parse_time(v->str);
                } else if (v->type == J_NUM && out->nfields < INFLUX_MAX_FIELDS) {
                    influx_field_t *f = &out->fields[out->nfields++];
                    snprintf(f->name, sizeof(f->name), "%s", c->str);
                    f->value = v->num;
                }
            }
        }
    }
    json_free(root);
    return rc;
}

int influx_get_measurements(const influx_conn_t *c, influx_names_t *out)
{
    char *body = influx_query(c, "SHOW MEASUREMENTS");
    int rc;

    if (!body)
        return -1;
    rc = influx_parse_measurements(body, out);
    free(body);
    return rc;
}

int influx_get_reading(const influx_conn_t *c, const char *measurement, influx_reading_t *out)
{
    char ql[160];

    /* The name is put inside double quotes: refuse anything unusual. */
    for (const char *p = measurement; *p; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '_' || *p == '-' || *p == '.'))
            return -1;
    }
    snprintf(ql, sizeof(ql), "SELECT * FROM \"%s\" ORDER BY time DESC LIMIT 1", measurement);

    char *body = influx_query(c, ql);
    int rc;
    if (!body)
        return -1;
    rc = influx_parse_reading(body, out);
    free(body);
    return rc;
}

int influx_has_field(const influx_reading_t *r, const char *name)
{
    for (int i = 0; i < r->nfields; i++)
        if (strcmp(r->fields[i].name, name) == 0)
            return 1;
    return 0;
}

double influx_field(const influx_reading_t *r, const char *name, double def)
{
    for (int i = 0; i < r->nfields; i++)
        if (strcmp(r->fields[i].name, name) == 0)
            return r->fields[i].value;
    return def;
}
