/* InfluxDB 1.x client: plain HTTP GET on /query, JSON replies. */
#ifndef INFLUX_H
#define INFLUX_H

#include <time.h>

#define INFLUX_MAX_FIELDS 12
#define INFLUX_MAX_MEASUREMENTS 32
#define INFLUX_NAME_LEN 48

typedef struct {
    char host[64];
    int port;
    char db[32];
    int timeout_s;
} influx_conn_t;

typedef struct {
    int count;
    char name[INFLUX_MAX_MEASUREMENTS][INFLUX_NAME_LEN];
} influx_names_t;

typedef struct {
    char name[32];
    double value;
} influx_field_t;

typedef struct {
    time_t time;  /* UTC time of the sample */
    int nfields;
    influx_field_t fields[INFLUX_MAX_FIELDS];
} influx_reading_t;

/* Fill out with the measurement names. 0 = ok, -1 = failure. */
int influx_get_measurements(const influx_conn_t *c, influx_names_t *out);

/* Latest sample of a measurement. 0 = ok, 1 = measurement has no data,
 * -1 = failure. */
int influx_get_reading(const influx_conn_t *c, const char *measurement, influx_reading_t *out);

/* Value of a named field, or def when the reading has no such field. */
double influx_field(const influx_reading_t *r, const char *name, double def);
int influx_has_field(const influx_reading_t *r, const char *name);

/* Reply parsers (exposed for tests). Same return values as above. */
int influx_parse_measurements(const char *json, influx_names_t *out);
int influx_parse_reading(const char *json, influx_reading_t *out);
/* "2026-10-24T16:55:17.468Z" -> UTC time_t, or (time_t)-1. */
time_t influx_parse_time(const char *s);

#endif
