#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_REFRESH_S 180
#define MIN_REFRESH_S 5

static int config_dir(char *out, size_t cap)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && *xdg)
        snprintf(out, cap, "%s/komaro-digest", xdg);
    else if (home && *home)
        snprintf(out, cap, "%s/.config/komaro-digest", home);
    else
        return -1;
    return 0;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static void set_unit(app_config_t *cfg, const char *val)
{
    if (val[0] == 'C' || val[0] == 'c')
        cfg->unit = 'C';
    else if (val[0] == 'F' || val[0] == 'f')
        cfg->unit = 'F';
}

static int parse_refresh(const char *val, int fallback)
{
    int v = atoi(val);

    return v >= MIN_REFRESH_S ? v : fallback;
}

static void read_config_file(const char *path, app_config_t *cfg)
{
    FILE *f = fopen(path, "r");
    char line[160];

    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq)
            continue;
        *eq = '\0';
        char *val = eq + 1;
        trim(line);
        trim(val);
        if (strcmp(line, "host") == 0)
            snprintf(cfg->conn.host, sizeof(cfg->conn.host), "%s", val);
        else if (strcmp(line, "port") == 0)
            cfg->conn.port = atoi(val);
        else if (strcmp(line, "db") == 0)
            snprintf(cfg->conn.db, sizeof(cfg->conn.db), "%s", val);
        else if (strcmp(line, "refresh") == 0)
            cfg->refresh_s = parse_refresh(val, cfg->refresh_s);
        else if (strcmp(line, "zoom") == 0)
            cfg->zoom = atoi(val);
        else if (strcmp(line, "title") == 0)
            snprintf(cfg->title, sizeof(cfg->title), "%s", val);
        else if (strcmp(line, "autorotate") == 0)
            cfg->autorotate = atoi(val) != 0;
        else if (strcmp(line, "sensor") == 0)
            snprintf(cfg->sensor, sizeof(cfg->sensor), "%s", val);
        else if (strcmp(line, "unit") == 0)
            set_unit(cfg, val);
    }
    fclose(f);
}

void config_load(app_config_t *cfg, int argc, char **argv)
{
    char dir[200], path[260];

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->conn.host, sizeof(cfg->conn.host), "silvana.home");
    cfg->conn.port = 8086;
    snprintf(cfg->conn.db, sizeof(cfg->conn.db), "komaro");
    cfg->conn.timeout_s = 5;
    cfg->unit = 'C';
    cfg->refresh_s = DEFAULT_REFRESH_S;

    if (config_dir(dir, sizeof(dir)) == 0) {
        snprintf(path, sizeof(path), "%s/config", dir);
        read_config_file(path, cfg);

        snprintf(path, sizeof(path), "%s/state", dir);
        read_config_file(path, cfg);
    }

    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "-H") == 0)
            snprintf(cfg->conn.host, sizeof(cfg->conn.host), "%s", argv[i + 1]);
        else if (strcmp(argv[i], "-P") == 0)
            cfg->conn.port = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-d") == 0)
            snprintf(cfg->conn.db, sizeof(cfg->conn.db), "%s", argv[i + 1]);
        else if (strcmp(argv[i], "-s") == 0)
            snprintf(cfg->sensor, sizeof(cfg->sensor), "%s", argv[i + 1]);
        else if (strcmp(argv[i], "-u") == 0)
            set_unit(cfg, argv[i + 1]);
        else if (strcmp(argv[i], "-r") == 0)
            cfg->refresh_s = parse_refresh(argv[i + 1], cfg->refresh_s);
    }
}

void config_save_state(const char *sensor, char unit, int autorotate)
{
    char dir[200], path[260], parent[200];

    if (config_dir(dir, sizeof(dir)) != 0)
        return;
    snprintf(parent, sizeof(parent), "%s", dir);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        mkdir(parent, 0755); /* ~/.config may not exist yet */
    }
    mkdir(dir, 0755);

    snprintf(path, sizeof(path), "%s/state", dir);
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "sensor=%s\nunit=%c\nautorotate=%d\n", sensor, unit, autorotate ? 1 : 0);
    fclose(f);
}
