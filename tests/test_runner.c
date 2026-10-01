#include "ipv4.h"
#include "route.h"
#include <stdio.h>
#include <string.h>

static unsigned checks = 0, failures = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    ++failures; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

static Route make_route(const char *text, uint32_t metric)
{
    Route route = {0};
    Ipv4Cidr cidr;
    SubnetInfo subnet;
    if (!cidr_parse(text, &cidr) || !subnet_calculate(cidr, &subnet)) {
        ++failures;
        return route;
    }
    route.network = subnet.network;
    route.mask = subnet.mask;
    route.prefix_length = cidr.prefix_length;
    route.metric = metric;
    return route;
}

int main(void)
{
    uint32_t address = 123, number;
    char buffer[16], tiny[4];
    Ipv4Cidr cidr;
    SubnetInfo subnet;
    Route routes[5], snapshot[5];
    size_t i;
    const char *bad_ips[] = {"192.168.1", "192.168.1.256", "192.168.-1.1", "192.168.1.1abc",
        "1..2.3", "", ".1.2.3", "1.2.3.", "1.2.3.4.5", "01.2.3.4", "+1.2.3.4", " 1.2.3.4", "1.2.3.4 ", "999999999999.2.3.4"};
    const char *bad_cidrs[] = {"1.2.3.4", "1.2.3.4/", "1.2.3.4/33", "1.2.3.4/-1",
        "1.2.3.4/24abc", "1.2.3.4/24/1", "1.2.3.4/024", "/24", "1.2.3.256/24"};
    CHECK(ipv4_parse("192.168.1.10", &address) && address == UINT32_C(0xc0a8010a));
    CHECK(ipv4_parse("0.0.0.0", &address) && address == 0);
    CHECK(ipv4_parse("255.255.255.255", &address) && address == UINT32_MAX);
    for (i = 0; i < sizeof(bad_ips) / sizeof(bad_ips[0]); ++i) CHECK(!ipv4_parse(bad_ips[i], &address));
    CHECK(address == UINT32_MAX); /* Failed parses leave output untouched. */
    CHECK(!ipv4_parse(NULL, &address));
    CHECK(!ipv4_parse("1.2.3.4", NULL));
    ipv4_format(UINT32_C(0xc0a8010a), buffer, sizeof(buffer));
    CHECK(strcmp(buffer, "192.168.1.10") == 0);
    CHECK(ipv4_parse(buffer, &address) && address == UINT32_C(0xc0a8010a));
    ipv4_format(UINT32_MAX, tiny, sizeof(tiny));
    CHECK(tiny[3] == '\0');
    for (i = 0; i < sizeof(bad_cidrs) / sizeof(bad_cidrs[0]); ++i) CHECK(!cidr_parse(bad_cidrs[i], &cidr));
    CHECK(cidr_parse("192.168.10.37/24", &cidr) && cidr.prefix_length == 24);
    CHECK(subnet_calculate(cidr, &subnet) && subnet.mask == UINT32_C(0xffffff00));
    CHECK(subnet.network == UINT32_C(0xc0a80a00));
    CHECK(subnet.broadcast == UINT32_C(0xc0a80aff));
    CHECK(subnet.first_host == UINT32_C(0xc0a80a01) && subnet.last_host == UINT32_C(0xc0a80afe));
    CHECK(subnet.usable_hosts == 254);
    CHECK(cidr_parse("192.168.10.37/0", &cidr) && subnet_calculate(cidr, &subnet));
    CHECK(subnet.mask == 0 && subnet.network == 0 && subnet.broadcast == UINT32_MAX);
    CHECK(subnet.usable_hosts == UINT64_C(4294967294));
    CHECK(subnet.first_host == 1 && subnet.last_host == UINT32_MAX - 1);
    CHECK(cidr_parse("192.168.10.37/31", &cidr) && subnet_calculate(cidr, &subnet) && subnet.usable_hosts == 0);
    CHECK(subnet.network == UINT32_C(0xc0a80a24) && subnet.broadcast == UINT32_C(0xc0a80a25));
    CHECK(cidr_parse("192.168.10.37/32", &cidr) && subnet_calculate(cidr, &subnet));
    CHECK(subnet.network == cidr.address && subnet.broadcast == cidr.address && subnet.usable_hosts == 0);
    CHECK(cidr_parse("10.0.0.3/30", &cidr) && subnet_calculate(cidr, &subnet) && subnet.usable_hosts == 2);
    cidr.prefix_length = 33;
    CHECK(!subnet_calculate(cidr, &subnet));
    CHECK(decimal_parse("4294967295", UINT32_MAX, &number) && number == UINT32_MAX);
    CHECK(!decimal_parse("4294967296", UINT32_MAX, &number));
    CHECK(!decimal_parse("99999999999999999999999", UINT32_MAX, &number));
    routes[0] = make_route("10.1.0.0/16", 0);
    routes[1] = make_route("0.0.0.0/0", 0);
    routes[2] = make_route("10.1.2.37/24", 100);
    routes[3] = make_route("10.0.0.0/8", 0);
    routes[4] = make_route("10.1.2.0/24", 10);
    CHECK(ipv4_parse("10.1.2.45", &address));
    CHECK(route_lookup(routes, 4, address) == &routes[2]);
    CHECK(routes[2].network == UINT32_C(0x0a010200));
    CHECK(route_lookup(routes, 5, address) == &routes[4]);
    routes[4].metric = 100;
    memcpy(snapshot, routes, sizeof(routes));
    CHECK(route_lookup(routes, 5, address) == &routes[2]);
    CHECK(memcmp(snapshot, routes, sizeof(routes)) == 0);
    CHECK(route_lookup(routes, 5, 0) == &routes[1]);
    CHECK(route_lookup(routes, 5, UINT32_MAX) == &routes[1]);
    CHECK(route_lookup(routes, 1, 0) == NULL);
    CHECK(route_lookup(routes, 0, address) == NULL);
    CHECK(route_lookup(NULL, 0, address) == NULL);
    printf("Core checks: %u, failures: %u\n", checks, failures);
    return failures ? 1 : 0;
}
