#ifndef CORE_SIGNALS_HPP
#define CORE_SIGNALS_HPP

// Signal setup. SIGPIPE is ignored so that writing to a closed socket or
// pipe returns an error instead of killing the server. SIGINT and SIGTERM
// only set a flag; the event loop checks it and shuts down cleanly.
// SIGCHLD keeps its default action so that waitpid() can collect children.
namespace signals {

void install();
bool stopRequested();

}  // namespace signals

#endif
