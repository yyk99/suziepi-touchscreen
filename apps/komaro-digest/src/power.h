/*
 * Background model of the INA219 power sensors (voltage / current / power),
 * written to InfluxDB by ina219_to_influx.py: measurement "ina219" in its own
 * database, one "sensor" tag value per sensor. Same idea as model.h: a worker
 * thread talks to InfluxDB, LVGL code only calls power_snapshot().
 */
#ifndef POWER_H
#define POWER_H

#include "influx.h"

typedef struct {
    unsigned version;           /* increases whenever anything below changes */
    char selected[INFLUX_NAME_LEN];
    influx_names_t names;       /* the values of the "sensor" tag */
    int have_names;
    influx_reading_t reading;   /* latest sample of the selected sensor */
    int have_reading;
    int no_data;                /* selected sensor exists but has no samples */
    char error[96];             /* empty when the last request worked */
} power_snapshot_t;

/* Start the worker. conn->db is the INA219 database. selected may be empty:
 * the first sensor is then used once the list is known. */
void power_start(const influx_conn_t *conn, const char *selected, int refresh_seconds);

/* Switch to another sensor and fetch it immediately. */
void power_select(const char *name);

/* Re-read the list of sensors (and the reading) soon. */
void power_refresh(void);

void power_snapshot(power_snapshot_t *out);

#endif
