#include "ipv4.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE *stream)
{
    fprintf(stream, "Usage:\n  ipv4-routing-lab subnet <IPv4/CIDR>\n"
                    "  ipv4-routing-lab --help\n");
}

static void print_address(const char *label, uint32_t address)
{
    char buffer[16];
    ipv4_format(address, buffer, sizeof(buffer));
    printf("%-16s%s\n", label, buffer);
}

int main(int argc, char **argv)
{
    Ipv4Cidr cidr;
    SubnetInfo info;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "subnet") != 0) {
        usage(stderr);
        return 2;
    }
    if (!cidr_parse(argv[2], &cidr) || !subnet_calculate(cidr, &info)) {
        fprintf(stderr, "Error: invalid IPv4/CIDR: %s\n", argv[2]);
        return 2;
    }
    print_address("Address:", cidr.address);
    printf("%-16s/%u\n", "Prefix:", cidr.prefix_length);
    print_address("Subnet mask:", info.mask);
    print_address("Network:", info.network);
    print_address("Broadcast:", info.broadcast);
    if (info.usable_hosts) {
        print_address("First host:", info.first_host);
        print_address("Last host:", info.last_host);
    } else {
        printf("First host:     N/A (traditional host rule)\n"
               "Last host:      N/A (traditional host rule)\n");
    }
    printf("%-16s%" PRIu64 "\n", "Usable hosts:", info.usable_hosts);
    return 0;
}
