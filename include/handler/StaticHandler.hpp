#ifndef HANDLER_STATICHANDLER_HPP
#define HANDLER_STATICHANDLER_HPP

#include "http/Response.hpp"
#include "route/RouteResult.hpp"

// 200 with the file of a STATIC_FILE route. Files up to 1 MB are read into
// memory, larger ones are streamed (design 8). False if the file cannot be
// read; the caller answers 500.
bool serveStaticFile(const RouteResult& route, Response& response);

#endif
