#include "handler/ErrorPageBuilder.hpp"

#include <sys/stat.h>

#include <map>
#include <sstream>

#include "core/Limits.hpp"
#include "handler/FileReader.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusText.hpp"
#include "route/Router.hpp"

namespace {

std::string defaultPage(int status) {
  std::ostringstream html;
  html << "<!DOCTYPE html>\n<html><head><title>" << status << " "
       << reasonPhrase(status) << "</title></head>\n<body><h1>" << status << " "
       << reasonPhrase(status) << "</h1></body></html>\n";
  return html.str();
}

// The configured page as a regular file, or false.
bool readConfiguredPage(int status, const ServerConfig& server,
                        std::string& fsPath, std::string& content) {
  const std::map<int, std::string>::const_iterator page =
      server.errorPages.find(status);
  if (page == server.errorPages.end()) return false;
  if (!Router(server).mapToFile(page->second, fsPath)) return false;
  struct stat info;
  return stat(fsPath.c_str(), &info) == 0 && S_ISREG(info.st_mode) &&
         readWholeFile(fsPath, limits::kMaxErrorPageSize, content);
}

}  // namespace

Response buildErrorPage(int status, const ServerConfig& server) {
  Response response;
  response.status = status;
  std::string fsPath;
  if (readConfiguredPage(status, server, fsPath, response.body)) {
    response.headers.add("Content-Type", mimeTypeFor(fsPath));
  } else {
    response.body = defaultPage(status);
    response.headers.add("Content-Type", "text/html");
  }
  return response;
}
