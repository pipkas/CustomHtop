#include "SignalUtil.h"

#include <csignal>
#include <map>
#include <sstream>

namespace custom_htop {

std::string signal_name(int signum) {
    static const std::map<int, std::string> names = {
#ifdef SIGHUP
        {SIGHUP, "SIGHUP"},
#endif
#ifdef SIGINT
        {SIGINT, "SIGINT"},
#endif
#ifdef SIGQUIT
        {SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGILL
        {SIGILL, "SIGILL"},
#endif
#ifdef SIGTRAP
        {SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGABRT
        {SIGABRT, "SIGABRT"},
#endif
#ifdef SIGBUS
        {SIGBUS, "SIGBUS"},
#endif
#ifdef SIGFPE
        {SIGFPE, "SIGFPE"},
#endif
#ifdef SIGKILL
        {SIGKILL, "SIGKILL"},
#endif
#ifdef SIGUSR1
        {SIGUSR1, "SIGUSR1"},
#endif
#ifdef SIGSEGV
        {SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGUSR2
        {SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGPIPE
        {SIGPIPE, "SIGPIPE"},
#endif
#ifdef SIGALRM
        {SIGALRM, "SIGALRM"},
#endif
#ifdef SIGTERM
        {SIGTERM, "SIGTERM"},
#endif
#ifdef SIGSTKFLT
        {SIGSTKFLT, "SIGSTKFLT"},
#endif
#ifdef SIGCHLD
        {SIGCHLD, "SIGCHLD"},
#endif
#ifdef SIGCONT
        {SIGCONT, "SIGCONT"},
#endif
#ifdef SIGSTOP
        {SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGTSTP
        {SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
        {SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
        {SIGTTOU, "SIGTTOU"},
#endif
#ifdef SIGURG
        {SIGURG, "SIGURG"},
#endif
#ifdef SIGXCPU
        {SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
        {SIGXFSZ, "SIGXFSZ"},
#endif
#ifdef SIGVTALRM
        {SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGPROF
        {SIGPROF, "SIGPROF"},
#endif
#ifdef SIGWINCH
        {SIGWINCH, "SIGWINCH"},
#endif
#ifdef SIGIO
        {SIGIO, "SIGIO"},
#endif
#ifdef SIGPWR
        {SIGPWR, "SIGPWR"},
#endif
#ifdef SIGSYS
        {SIGSYS, "SIGSYS"},
#endif
    };

    const auto found = names.find(signum);
    if (found != names.end()) {
        return found->second;
    }

#ifdef SIGRTMIN
    const int rt_min = SIGRTMIN;
    const int rt_max = SIGRTMAX;
    if (signum >= rt_min && signum <= rt_max) {
        std::ostringstream name;
        name << "SIGRTMIN";
        if (signum > rt_min) {
            name << '+' << (signum - rt_min);
        }
        return name.str();
    }
#endif

    std::ostringstream fallback;
    fallback << "SIG" << signum;
    return fallback.str();
}

}
