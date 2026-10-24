/* Unit tests for the JSON parser, InfluxDB reply parsers and the resolver. */
#include "influx.h"
#include "json.h"
#include "net.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
            failures++;                                                          \
        }                                                                        \
    } while (0)

static void test_json(void)
{
    jnode_t *n = json_parse("{\"a\": [1, -2.5e1, \"x\\n\\u00e9\\\"\"], \"b\": {\"c\": null},"
                            " \"t\": true}");
    CHECK(n && n->type == J_OBJ && n->count == 3);
    const jnode_t *a = json_get(n, "a");
    CHECK(a && a->count == 3);
    CHECK(json_at(a, 0)->num == 1);
    CHECK(fabs(json_at(a, 1)->num + 25.0) < 1e-9);
    CHECK(strcmp(json_at(a, 2)->str, "x\n\xC3\xA9\"") == 0);
    CHECK(json_get(json_get(n, "b"), "c")->type == J_NULL);
    CHECK(json_get(n, "t")->num == 1);
    CHECK(json_get(n, "missing") == NULL);
    CHECK(json_at(a, 3) == NULL);
    json_free(n);

    CHECK(json_parse("") == NULL);
    CHECK(json_parse("{") == NULL);
    CHECK(json_parse("[1,]") == NULL);
    CHECK(json_parse("{\"a\" 1}") == NULL);
    CHECK(json_parse("[1] x") == NULL);
    CHECK(json_parse("\"abc") == NULL);
    n = json_parse("  [ ]  ");
    CHECK(n && n->count == 0);
    json_free(n);
}

static void test_time(void)
{
    CHECK(influx_parse_time("1970-01-01T00:00:00Z") == 0);
    CHECK(influx_parse_time("2026-10-24T16:55:17.46807499Z") == 1792860917);
    CHECK(influx_parse_time("garbage") == (time_t)-1);
}

static void test_measurements(void)
{
    const char *ok =
        "{\"results\":[{\"statement_id\":0,\"series\":[{\"name\":\"measurements\","
        "\"columns\":[\"name\"],\"values\":[[\"living_room\"],[\"sensor\"],[\"sensor2\"]]}]}]}";
    influx_names_t n;

    CHECK(influx_parse_measurements(ok, &n) == 0);
    CHECK(n.count == 3);
    CHECK(strcmp(n.name[1], "sensor") == 0);

    CHECK(influx_parse_measurements("{\"results\":[{\"statement_id\":0}]}", &n) == 0);
    CHECK(n.count == 0);
    CHECK(influx_parse_measurements("{\"results\":[{\"error\":\"db not found\"}]}", &n) == -1);
    CHECK(influx_parse_measurements("{\"error\":\"unauthorized\"}", &n) == -1);
    CHECK(influx_parse_measurements("not json", &n) == -1);
}

static void test_reading(void)
{
    const char *ok =
        "{\"results\":[{\"statement_id\":0,\"series\":[{\"name\":\"sensor\",\"columns\":"
        "[\"time\",\"accel_x\",\"humidity\",\"note\",\"temperature_c\"],\"values\":"
        "[[\"2026-10-24T16:55:17.46807499Z\",805,48,\"hi\",14.7]]}]}]}";
    influx_reading_t r;

    CHECK(influx_parse_reading(ok, &r) == 0);
    CHECK(r.time == 1792860917);
    CHECK(r.nfields == 3); /* the string column is skipped */
    CHECK(fabs(influx_field(&r, "temperature_c", 0) - 14.7) < 1e-9);
    CHECK(influx_field(&r, "humidity", 0) == 48);
    CHECK(influx_has_field(&r, "accel_x"));
    CHECK(!influx_has_field(&r, "note"));
    CHECK(influx_field(&r, "nothing", -1) == -1);

    CHECK(influx_parse_reading("{\"results\":[{\"statement_id\":0}]}", &r) == 1);
    CHECK(influx_parse_reading("{\"results\":[{\"error\":\"x\"}]}", &r) == -1);
}

static void test_resolve(void)
{
    struct in_addr a;

    CHECK(net_resolve_ipv4("192.168.1.20", &a) == 0);
    CHECK(a.s_addr == inet_addr("192.168.1.20"));
    CHECK(net_resolve_ipv4("", &a) == -1);
    CHECK(net_resolve_ipv4("no-such-host.invalid", &a) == -1);
}

int main(void)
{
    test_json();
    test_time();
    test_measurements();
    test_reading();
    test_resolve();
    if (failures)
        printf("%d check(s) failed\n", failures);
    else
        printf("all tests passed\n");
    return failures ? 1 : 0;
}
