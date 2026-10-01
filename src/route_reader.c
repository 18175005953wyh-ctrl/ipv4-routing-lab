#include "route_reader.h"
#include <stdio.h>
#include <string.h>

static int fail(char *error, size_t capacity, size_t line, const char *message)
{
    if (error && capacity) (void)snprintf(error, capacity, "CSV line %zu: %s", line, message);
    return 0;
}

/* Binary input makes CRLF, embedded NUL and control bytes explicit. */
static int read_line(FILE *file, char *buffer, size_t capacity)
{
    size_t length = 0;
    int ch, invalid = 0, seen = 0;
    while ((ch = fgetc(file)) != EOF) {
        seen = 1;
        if (ch == '\n') break;
        if (ch == 0 || length + 1 >= capacity) invalid = 1;
        else buffer[length++] = (char)ch;
    }
    if (ferror(file)) return -2;
    if (!seen) return 0;
    if (length && buffer[length - 1] == '\r') --length;
    buffer[length] = '\0';
    return invalid ? -1 : 1;
}

static int parse_record(char *line, Route *route, char *error, size_t capacity, size_t number)
{
    char *fields[4], *p;
    size_t i;
    Ipv4Cidr cidr;
    SubnetInfo subnet;
    fields[0] = line;
    for (i = 1; i < 4; ++i) {
        p = strchr(fields[i - 1], ',');
        if (!p) return fail(error, capacity, number, "expected exactly four fields");
        *p = '\0';
        fields[i] = p + 1;
    }
    if (strchr(fields[3], ',')) return fail(error, capacity, number, "expected exactly four fields");
    if (!cidr_parse(fields[0], &cidr) || !subnet_calculate(cidr, &subnet))
        return fail(error, capacity, number, "invalid destination CIDR");
    route->network = subnet.network;
    route->mask = subnet.mask;
    route->prefix_length = cidr.prefix_length;
    route->direct = strcmp(fields[1], "direct") == 0;
    if (!route->direct && !ipv4_parse(fields[1], &route->next_hop))
        return fail(error, capacity, number, "next_hop must be IPv4 or direct");
    if (!*fields[2] || strlen(fields[2]) >= INTERFACE_CAPACITY)
        return fail(error, capacity, number, "interface must contain 1-63 bytes");
    for (p = fields[2]; *p; ++p)
        if ((unsigned char)*p < 32 || (unsigned char)*p == 127 || *p == '"')
            return fail(error, capacity, number, "interface must not contain control characters or quotes");
    strcpy(route->interface_name, fields[2]);
    if (!decimal_parse(fields[3], UINT32_MAX, &route->metric))
        return fail(error, capacity, number, "metric must be an integer from 0 to 4294967295");
    return 1;
}

int route_read(const char *filename, Route routes[ROUTE_LIMIT], size_t *count,
               char *error, size_t error_capacity)
{
    FILE *file;
    Route pending[ROUTE_LIMIT] = {0};
    char line[1024];
    size_t number = 0, used = 0;
    int status, ok = 1;
    if (count) *count = 0;
    if (!filename || !routes || !count) return fail(error, error_capacity, 0, "invalid reader arguments");
    file = fopen(filename, "rb");
    if (!file) return fail(error, error_capacity, 0, "cannot open route file");
    while ((status = read_line(file, line, sizeof(line))) != 0) {
        ++number;
        if (status < 0) {
            ok = fail(error, error_capacity, number, status == -2 ? "read failure" : "line too long or contains NUL");
            break;
        }
        if (number == 1) {
            if (strcmp(line, "destination,next_hop,interface,metric") != 0) {
                ok = fail(error, error_capacity, number, "expected destination,next_hop,interface,metric header");
                break;
            }
        } else {
            if (used == ROUTE_LIMIT) {
                ok = fail(error, error_capacity, number, "route limit exceeded (256)");
                break;
            }
            if (!parse_record(line, &pending[used], error, error_capacity, number)) { ok = 0; break; }
            ++used;
        }
    }
    if (fclose(file) != 0 && ok) ok = fail(error, error_capacity, number, "close failure");
    if (ok && used == 0) ok = fail(error, error_capacity, number ? number : 1, "empty route table");
    if (!ok) return 0;
    memcpy(routes, pending, used * sizeof(Route));
    *count = used;
    if (error && error_capacity) error[0] = '\0';
    return 1;
}
