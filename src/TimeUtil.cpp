#include "TimeUtil.h"

#include <iomanip>
#include <sstream>

namespace custom_htop {

std::string format_time_plus(unsigned long long jiffies, long ticks_per_second) {
    const double seconds_exact = static_cast<double>(jiffies) / static_cast<double>(ticks_per_second);
    const auto centiseconds = static_cast<unsigned long long>(seconds_exact * 100.0);
    const auto hours = centiseconds / 360000;
    const auto minutes = (centiseconds / 6000) % 60;
    const auto seconds = (centiseconds / 100) % 60;
    const auto centi = centiseconds % 100;

    std::ostringstream out;
    out << hours << ':'
        << std::setw(2) << std::setfill('0') << minutes << ':'
        << std::setw(2) << seconds << '.'
        << std::setw(2) << centi << std::setfill(' ');
    return out.str();
}

}
