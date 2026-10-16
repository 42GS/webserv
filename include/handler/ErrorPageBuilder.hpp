#ifndef HANDLER_ERRORPAGEBUILDER_HPP
#define HANDLER_ERRORPAGEBUILDER_HPP

#include "config/ServerConfig.hpp"
#include "http/Response.hpp"

// A response with the given status and the server's error page for it:
// the configured file (mapped through the locations like any URI) or, if
// there is none or it cannot be read, a built-in page (raw/subject.md:143).
// Never fails.
Response buildErrorPage(int status, const ServerConfig& server);

#endif
