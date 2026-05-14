#include "MemInfo.h"

namespace custom_htop {

unsigned long long MemInfo::mem_used() const {
    if (mem_total <= mem_free + buffers + cache) {
        return 0;
    }
    return mem_total - mem_free - buffers - cache;
}

unsigned long long MemInfo::swap_used() const {
    if (swap_total <= swap_free) {
        return 0;
    }
    return swap_total - swap_free;
}

}
