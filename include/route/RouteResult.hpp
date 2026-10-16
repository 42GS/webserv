#ifndef ROUTE_ROUTERESULT_HPP
#define ROUTE_ROUTERESULT_HPP

#include <string>

#include "config/ServerConfig.hpp"

// How a request is to be answered. Upload, delete and CGI kinds come with
// steps 6 and 7.
struct RouteResult {
  enum Kind { STATIC_FILE, DIRECTORY_LISTING, REDIRECT, ERROR };

  RouteResult() : kind(ERROR), status(0), location(0) {}

  Kind kind;
  int status;              // REDIRECT and ERROR
  std::string redirectTo;  // REDIRECT: Location value
  std::string allow;       // ERROR 405: Allow value
  std::string fsPath;      // STATIC_FILE, DIRECTORY_LISTING
  std::string urlPath;     // normalized path (for listings)
  const LocationConfig* location;
};

#endif
