#ifndef ROUTE_READER_H
#define ROUTE_READER_H
#include "route.h"
/* On failure count is zero; no partially loaded table is published. */
int route_read(const char *filename, Route routes[ROUTE_LIMIT], size_t *count,
               char *error, size_t error_capacity);
#endif
