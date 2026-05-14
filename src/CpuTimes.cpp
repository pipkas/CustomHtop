#include "CpuTimes.h"

namespace custom_htop {

unsigned long long CpuTimes::total() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

unsigned long long CpuTimes::idle_all() const {
    return idle + iowait;
}

}
