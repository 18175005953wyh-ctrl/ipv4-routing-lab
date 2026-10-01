#include "route.h"

const Route *route_lookup(const Route *routes, size_t count, uint32_t destination)
{
    const Route *best = NULL;
    size_t i;
    if (!routes) return NULL;
    for (i = 0; i < count; ++i) {
        const Route *candidate = &routes[i];
        if ((destination & candidate->mask) != candidate->network) continue;
        if (!best || candidate->prefix_length > best->prefix_length ||
            (candidate->prefix_length == best->prefix_length && candidate->metric < best->metric))
            best = candidate;
    }
    return best;
}
