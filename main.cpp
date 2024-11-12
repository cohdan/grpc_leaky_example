#include <iostream>
#include <thread>
#include <csignal>

#include "server.h"

using std::cout;
using std::endl;

void term_signal_handler(int signum) {
    cout << "Starting sigterm handler" << endl;
    server::get_server().close_server();
}

void signal_handler(int signum, siginfo_t *siginfo, void *context) {
	cout << "Interrupt signal " << signum << " received" << endl; 
	if (signum == SIGTERM) {
		term_signal_handler(signum);
	}

	// Reset to default behavior and re-raise signal
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(signum, &sa, nullptr);
	raise(signum);
}

void register_signals() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    // Ignore SIGPIPE
    struct sigaction ignore_sa;
    ignore_sa.sa_handler = SIG_IGN;
    sigemptyset(&ignore_sa.sa_mask);
    ignore_sa.sa_flags = 0;
    sigaction(SIGPIPE, &ignore_sa, nullptr);

    std::vector<int> signals_to_handle = { SIGINT, SIGTERM, SIGQUIT, SIGABRT, SIGSEGV, SIGFPE, SIGILL, SIGBUS, SIGSYS, SIGSTOP, SIGUSR1 };
    for (int signum : signals_to_handle) {
        sigaction(signum, &sa, nullptr);
    }
}

int main() {
    register_signals();
    server::get_server().init_server();
    while (true) {
	    std::this_thread::yield();
    }
    server::get_server().close_server();
    return 0;
}
