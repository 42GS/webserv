#ifndef HANDLER_AUTOINDEXHANDLER_HPP
#define HANDLER_AUTOINDEXHANDLER_HPP

#include "http/Response.hpp"
#include "route/RouteResult.hpp"

// 200 with an HTML list of a DIRECTORY_LISTING route: names only, with a
// trailing "/" for directories (design 7). False if the directory cannot
// be read; the caller answers 403.
bool listDirectory(const RouteResult& route, Response& response);

#endif
