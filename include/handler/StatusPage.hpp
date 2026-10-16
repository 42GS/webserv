#ifndef HANDLER_STATUSPAGE_HPP
#define HANDLER_STATUSPAGE_HPP

#include <map>
#include <string>

#include "http/Response.hpp"
#include "route/Router.hpp"

// A response with the given status and its page: the file configured with
// error_page for that status (mapped through the locations like any URI)
// or, if there is none or it cannot be read, a built-in page
// (raw/subject.md:143). Used for errors and for the body of redirects.
// Never fails.
Response buildStatusPage(int status,
                         const std::map<int, std::string>& configuredPages,
                         const Router& router);

#endif
