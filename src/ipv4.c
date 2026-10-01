#include "ipv4.h"
#include <stdio.h>
#include <string.h>

int decimal_parse(const char *text, uint32_t maximum, uint32_t *value)
{
    uint32_t result = 0;
    const char *p;
    if (!text || !value || !*text || (text[0] == '0' && text[1])) return 0;
    for (p = text; *p; ++p) {
        uint32_t digit;
        if (*p < '0' || *p > '9') return 0;
        digit = (uint32_t)(*p - '0');
        if (digit > maximum || result > (maximum - digit) / 10u) return 0;
        result = result * 10u + digit;
    }
    *value = result;
    return 1;
}

int ipv4_parse(const char *text, uint32_t *address)
{
    uint32_t result = 0;
    unsigned part;
    const char *p = text;
    if (!text || !address) return 0;
    for (part = 0; part < 4; ++part) {
        char octet[4];
        size_t length = 0;
        uint32_t value;
        while (*p && *p != '.') {
            if (length == 3) return 0;
            octet[length++] = *p++;
        }
        octet[length] = '\0';
        if (!decimal_parse(octet, 255, &value)) return 0;
        result = (result << 8) | value;
        if (part < 3) {
            if (*p != '.') return 0;
            ++p;
        } else if (*p) return 0;
    }
    *address = result;
    return 1;
}

void ipv4_format(uint32_t address, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0) return;
    (void)snprintf(buffer, capacity, "%u.%u.%u.%u",
        (unsigned)(address >> 24), (unsigned)((address >> 16) & 255u),
        (unsigned)((address >> 8) & 255u), (unsigned)(address & 255u));
}

int cidr_parse(const char *text, Ipv4Cidr *cidr)
{
    const char *slash;
    char address[16];
    size_t length;
    uint32_t prefix;
    Ipv4Cidr parsed;
    if (!text || !cidr || !(slash = strchr(text, '/'))) return 0;
    length = (size_t)(slash - text);
    if (length == 0 || length >= sizeof(address)) return 0;
    memcpy(address, text, length);
    address[length] = '\0';
    if (!ipv4_parse(address, &parsed.address) ||
        !decimal_parse(slash + 1, 32, &prefix)) return 0;
    parsed.prefix_length = (unsigned)prefix;
    *cidr = parsed;
    return 1;
}

int subnet_calculate(Ipv4Cidr cidr, SubnetInfo *info)
{
    if (!info || cidr.prefix_length > 32) return 0;
    info->mask = cidr.prefix_length == 0 ? 0 : UINT32_MAX << (32 - cidr.prefix_length);
    info->network = cidr.address & info->mask;
    info->broadcast = info->network | (uint32_t)~info->mask;
    info->usable_hosts = cidr.prefix_length <= 30
        ? (UINT64_C(1) << (32 - cidr.prefix_length)) - 2 : 0;
    info->first_host = info->usable_hosts ? info->network + 1u : 0;
    info->last_host = info->usable_hosts ? info->broadcast - 1u : 0;
    return 1;
}
