#ifndef IPV4_H
#define IPV4_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t address;
    unsigned prefix_length;
} Ipv4Cidr;

typedef struct {
    uint32_t mask, network, broadcast, first_host, last_host;
    uint64_t usable_hosts;
} SubnetInfo;

/* Strict decimal syntax; leading zeroes are rejected except the number 0. */
int decimal_parse(const char *text, uint32_t maximum, uint32_t *value);
int ipv4_parse(const char *text, uint32_t *address);
void ipv4_format(uint32_t address, char *buffer, size_t capacity);
int cidr_parse(const char *text, Ipv4Cidr *cidr);
int subnet_calculate(Ipv4Cidr cidr, SubnetInfo *info);

#endif
