/*
 * Settings live in $XDG_CONFIG_HOME/komaro-digest/ (default ~/.config/...):
 *   config    optional, written by hand:  host=..., port=..., db=..., refresh=...,
 *             power_db=... (database of the INA219 sensors, default ina219),
 *             zoom=1..8 (SDL window size, desktop build only),
 *             title=... (SDL window title, desktop build only)
 *   state     written by the app: sensor=<name>, unit=C|F, autorotate=0|1 and
 *             power_sensor=<name>, the choices made on the Sensors and Options pages
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "influx.h"

typedef struct {
    influx_conn_t conn;
    char sensor[INFLUX_NAME_LEN]; /* empty = not chosen yet */
    int refresh_s;                /* seconds between re-reads of the data */
    char unit;                    /* 'C' or 'F': the temperature shown large */
    int autorotate;               /* 1 = Current page steps through the sensors */
    char power_db[32];            /* database of the INA219 sensors (Power page) */
    char power_sensor[INFLUX_NAME_LEN]; /* sensor tag value shown on the Power page */
    int zoom;                     /* SDL window zoom from the config file; 0 = not set */
    char title[64];               /* SDL window title from the config file; empty = not set */
} app_config_t;

/* Defaults, then the files, then command line options:
 * -H host  -P port  -d database  -s sensor  -u C|F  -r seconds */
void config_load(app_config_t *cfg, int argc, char **argv);

/* Remember the sensors, the unit and the autorotate switch for the next start. */
void config_save_state(const char *sensor, char unit, int autorotate, const char *power_sensor);

#endif
