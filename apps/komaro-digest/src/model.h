/*
 * Background data model. A worker thread talks to InfluxDB so that a slow or
 * unreachable server never blocks the UI; LVGL code only calls model_snapshot().
 */
#ifndef MODEL_H
#define MODEL_H

#include "influx.h"

typedef struct {
    unsigned version;           /* increases whenever anything below changes */
    char selected[INFLUX_NAME_LEN];
    influx_names_t names;
    int have_names;
    influx_reading_t reading;   /* latest sample of the selected sensor */
    int have_reading;
    int no_data;                /* selected sensor exists but has no samples */
    char error[96];             /* empty when the last request worked */
} model_snapshot_t;

/* Start the worker, which re-reads the data every refresh_seconds. selected
 * may be empty: the model then picks a default once the list of sensors is
 * known. */
void model_start(const influx_conn_t *conn, const char *selected, int refresh_seconds);

/* Switch to another sensor and fetch it immediately. */
void model_select(const char *name);

/* Re-read the list of sensors (and the reading) soon. */
void model_refresh(void);

/* Copy the current state. */
void model_snapshot(model_snapshot_t *out);

#endif
