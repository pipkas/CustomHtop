#include "StateUtil.h"

namespace custom_htop {

std::string state_name(char state) {
    switch (state) {
        case 'R': return "Running / Runnable";
        case 'S': return "Interruptible Sleep";
        case 'D': return "Uninterruptible Sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        case 't': return "Tracing Stop";
        case 'I': return "Idle";
        case 'X':
        case 'x': return "Dead";
        case 'K': return "Wakekill";
        case 'W': return "Waking";
        case 'P': return "Parked";
        default: return "Unknown";
    }
}

}
