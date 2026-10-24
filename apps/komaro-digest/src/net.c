#include "net.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define DNS_PORT 53
#define MAX_NAMESERVERS 3
#define MAX_SEARCH 4

static int lookup_hosts_file(const char *host, struct in_addr *out)
{
    FILE *f = fopen("/etc/hosts", "r");
    char line[512];
    int found = -1;

    if (!f)
        return -1;
    while (found < 0 && fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';
        char *save;
        char *addr = strtok_r(line, " \t\r\n", &save);
        struct in_addr a;
        if (!addr || inet_pton(AF_INET, addr, &a) != 1)
            continue;
        for (char *name = strtok_r(NULL, " \t\r\n", &save); name;
             name = strtok_r(NULL, " \t\r\n", &save)) {
            if (strcasecmp(name, host) == 0) {
                *out = a;
                found = 0;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

typedef struct {
    struct in_addr servers[MAX_NAMESERVERS];
    int nservers;
    char search[MAX_SEARCH][64];
    int nsearch;
} resolv_conf_t;

static void read_resolv_conf(resolv_conf_t *rc)
{
    FILE *f = fopen("/etc/resolv.conf", "r");
    char line[256];

    memset(rc, 0, sizeof(*rc));
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *save;
        char *key = strtok_r(line, " \t\r\n", &save);
        if (!key)
            continue;
        if (strcmp(key, "nameserver") == 0 && rc->nservers < MAX_NAMESERVERS) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v && inet_pton(AF_INET, v, &rc->servers[rc->nservers]) == 1)
                rc->nservers++;
        } else if (strcmp(key, "search") == 0 || strcmp(key, "domain") == 0) {
            for (char *v = strtok_r(NULL, " \t\r\n", &save);
                 v && rc->nsearch < MAX_SEARCH; v = strtok_r(NULL, " \t\r\n", &save)) {
                snprintf(rc->search[rc->nsearch++], sizeof(rc->search[0]), "%s", v);
            }
        }
    }
    fclose(f);
}

/* Build a DNS query for an A record. Returns the packet length or -1. */
static int build_query(unsigned short id, const char *name, unsigned char *buf, size_t cap)
{
    size_t n = 12;
    if (cap < 12 + strlen(name) + 2 + 4)
        return -1;
    memset(buf, 0, 12);
    buf[0] = (unsigned char)(id >> 8);
    buf[1] = (unsigned char)id;
    buf[2] = 0x01; /* recursion desired */
    buf[5] = 1;    /* one question */

    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len == 0 || len > 63)
            return -1;
        buf[n++] = (unsigned char)len;
        memcpy(buf + n, p, len);
        n += len;
        p += len;
        if (*p == '.')
            p++;
    }
    buf[n++] = 0;
    buf[n++] = 0; buf[n++] = 1; /* type A */
    buf[n++] = 0; buf[n++] = 1; /* class IN */
    return (int)n;
}

/* Skip a (possibly compressed) name. Returns the new offset or -1. */
static int skip_name(const unsigned char *b, int len, int off)
{
    while (off < len) {
        unsigned char c = b[off];
        if (c == 0)
            return off + 1;
        if ((c & 0xC0) == 0xC0)
            return off + 2 <= len ? off + 2 : -1;
        off += 1 + c;
    }
    return -1;
}

/* Find the first A record in a response. Returns 0 and fills out on success. */
static int parse_response(const unsigned char *b, int len, unsigned short id, struct in_addr *out)
{
    if (len < 12 || (unsigned)((b[0] << 8) | b[1]) != id)
        return -1;
    if (!(b[2] & 0x80) || (b[3] & 0x0F) != 0) /* not a response, or error */
        return -1;
    int qd = (b[4] << 8) | b[5];
    int an = (b[6] << 8) | b[7];
    int off = 12;

    for (int i = 0; i < qd; i++) {
        off = skip_name(b, len, off);
        if (off < 0 || off + 4 > len)
            return -1;
        off += 4;
    }
    for (int i = 0; i < an; i++) {
        off = skip_name(b, len, off);
        if (off < 0 || off + 10 > len)
            return -1;
        int type = (b[off] << 8) | b[off + 1];
        int rdlen = (b[off + 8] << 8) | b[off + 9];
        off += 10;
        if (off + rdlen > len)
            return -1;
        if (type == 1 && rdlen == 4) {
            memcpy(&out->s_addr, b + off, 4);
            return 0;
        }
        off += rdlen;
    }
    return -1;
}

static int dns_query(const struct in_addr *server, const char *name, struct in_addr *out)
{
    static unsigned short next_id = 0x4b4d;
    unsigned short id = next_id++;
    unsigned char q[300], r[512];
    int qlen = build_query(id, name, q, sizeof(q));
    int fd, rlen, rc = -1;

    if (qlen < 0)
        return -1;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

    struct timeval tv = { 2, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(DNS_PORT) };
    sa.sin_addr = *server;

    if (sendto(fd, q, (size_t)qlen, 0, (struct sockaddr *)&sa, sizeof(sa)) == qlen) {
        rlen = (int)recv(fd, r, sizeof(r), 0);
        if (rlen > 0)
            rc = parse_response(r, rlen, id, out);
    }
    close(fd);
    return rc;
}

int net_resolve_ipv4(const char *host, struct in_addr *out)
{
    if (!host || !*host)
        return -1;
    if (inet_pton(AF_INET, host, out) == 1)
        return 0;
    if (lookup_hosts_file(host, out) == 0)
        return 0;

    resolv_conf_t rc;
    char name[160];
    read_resolv_conf(&rc);

    for (int s = 0; s < rc.nservers; s++) {
        if (dns_query(&rc.servers[s], host, out) == 0)
            return 0;
        if (!strchr(host, '.')) {
            for (int d = 0; d < rc.nsearch; d++) {
                snprintf(name, sizeof(name), "%s.%s", host, rc.search[d]);
                if (dns_query(&rc.servers[s], name, out) == 0)
                    return 0;
            }
        }
    }
    return -1;
}

int net_connect(const char *host, int port, int timeout_s)
{
    struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons((unsigned short)port) };
    if (net_resolve_ipv4(host, &sa.sin_addr) != 0)
        return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        int err = 0;
        socklen_t el = sizeof(err);
        struct pollfd p = { .fd = fd, .events = POLLOUT };
        if (errno != EINPROGRESS || poll(&p, 1, timeout_s * 1000) != 1 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) < 0 || err != 0) {
            close(fd);
            return -1;
        }
    }
    fcntl(fd, F_SETFL, flags);

    struct timeval tv = { timeout_s, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return fd;
}
