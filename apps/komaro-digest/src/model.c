#include "model.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv;
static influx_conn_t conn;
static model_snapshot_t state;
static int refresh_s;
static int want_refresh;
static int want_names;

static void *worker(void *arg)
{
    (void)arg;
    for (;;) {
        char sel[INFLUX_NAME_LEN];
        influx_names_t names;
        influx_reading_t rd;
        int do_names, rn = 0, rr = 1;

        pthread_mutex_lock(&mu);
        snprintf(sel, sizeof(sel), "%s", state.selected);
        do_names = want_names || !state.have_names;
        want_names = 0;
        want_refresh = 0;
        pthread_mutex_unlock(&mu);

        if (do_names)
            rn = influx_get_measurements(&conn, &names);
        if (rn == 0 && do_names && !sel[0] && names.count > 0) {
            /* No sensor chosen yet: prefer "sensor", else the first one. */
            snprintf(sel, sizeof(sel), "%s", names.name[0]);
            for (int i = 0; i < names.count; i++)
                if (strcmp(names.name[i], "sensor") == 0)
                    snprintf(sel, sizeof(sel), "sensor");
        }
        if (sel[0])
            rr = influx_get_reading(&conn, sel, &rd);

        pthread_mutex_lock(&mu);
        if (rn == 0 && do_names) {
            state.names = names;
            state.have_names = 1;
        }
        if (!state.selected[0])
            snprintf(state.selected, sizeof(state.selected), "%s", sel);
        if (strcmp(state.selected, sel) == 0) { /* not switched meanwhile */
            if (rr == 0) {
                state.reading = rd;
                state.have_reading = 1;
                state.no_data = 0;
            } else if (rr == 1) {
                state.have_reading = 0;
                state.no_data = 1;
            }
        }
        if (rn < 0 || rr < 0)
            snprintf(state.error, sizeof(state.error), "Cannot read from %s:%d", conn.host,
                     conn.port);
        else
            state.error[0] = '\0';
        state.version++;

        /* Wait for the next period, or until someone asks for a refresh. */
        struct timespec until;
        clock_gettime(CLOCK_MONOTONIC, &until);
        until.tv_sec += refresh_s;
        int rc = 0;
        while (!want_refresh && rc != ETIMEDOUT)
            rc = pthread_cond_timedwait(&cv, &mu, &until);
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

void model_start(const influx_conn_t *c, const char *selected, int refresh_seconds)
{
    pthread_condattr_t attr;
    pthread_t th;

    conn = *c;
    refresh_s = refresh_seconds > 0 ? refresh_seconds : 1;
    snprintf(state.selected, sizeof(state.selected), "%s", selected ? selected : "");
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&cv, &attr);
    pthread_create(&th, NULL, worker, NULL);
    pthread_detach(th);
}

void model_select(const char *name)
{
    pthread_mutex_lock(&mu);
    snprintf(state.selected, sizeof(state.selected), "%s", name);
    state.have_reading = 0;
    state.no_data = 0;
    state.version++;
    want_refresh = 1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mu);
}

void model_refresh(void)
{
    pthread_mutex_lock(&mu);
    want_names = 1;
    want_refresh = 1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mu);
}

void model_snapshot(model_snapshot_t *out)
{
    pthread_mutex_lock(&mu);
    *out = state;
    pthread_mutex_unlock(&mu);
}
