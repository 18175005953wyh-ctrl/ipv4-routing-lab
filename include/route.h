#ifndef ROUTE_H
#define ROUTE_H
#include "ipv4.h"
#define ROUTE_LIMIT 256
#define INTERFACE_CAPACITY 64
typedef struct {
    uint32_t network, mask, next_hop, metric;
    unsigned prefix_length;
    int direct;
    char interface_name[INTERFACE_CAPACITY];
} Route;
/* Returns an input-array element, or NULL. Never changes the route table. */
const Route *route_lookup(const Route *routes, size_t count, uint32_t destination);
#endif
