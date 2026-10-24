/*
 * Small TCP helpers that do not use getaddrinfo(): a statically linked glibc
 * cannot load the NSS modules at run time (it crashes on the Pi), so names are
 * resolved here from a literal address, /etc/hosts or a DNS A query.
 */
#ifndef NET_H
#define NET_H

#include <netinet/in.h>

/* Resolve host to an IPv4 address. Returns 0 on success, -1 on failure. */
int net_resolve_ipv4(const char *host, struct in_addr *out);

/* Connect a TCP socket with a timeout (seconds). Returns the fd or -1. The
 * socket gets send/receive timeouts of the same length. */
int net_connect(const char *host, int port, int timeout_s);

#endif
