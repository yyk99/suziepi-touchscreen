/*
 * Settings live in $XDG_CONFIG_HOME/komaro-digest/ (default ~/.config/...):
 *   config    optional, written by hand:  host=..., port=..., db=..., refresh=...,
 *             zoom=1..8 (SDL window size, desktop build only)
 *   state     written by the app: sensor=<name> and unit=C|F, the choices made on
 *             the Sensors page
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "influx.h"

typedef struct {
    influx_conn_t conn;
    char sensor[INFLUX_NAME_LEN]; /* empty = not chosen yet */
    int refresh_s;                /* seconds between re-reads of the data */
    char unit;                    /* 'C' or 'F': the temperature shown large */
    int zoom;                     /* SDL window zoom from the config file; 0 = not set */
} app_config_t;

/* Defaults, then the files, then command line options:
 * -H host  -P port  -d database  -s sensor  -u C|F  -r seconds */
void config_load(app_config_t *cfg, int argc, char **argv);

/* Remember the sensor and the unit for the next start. */
void config_save_state(const char *sensor, char unit);

#endif
