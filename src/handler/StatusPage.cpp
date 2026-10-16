#include "handler/StatusPage.hpp"

#include <sstream>

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FileReader.hpp"
#include "http/HeaderNames.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusText.hpp"

namespace {

std::string defaultPage(int status) {
  std::ostringstream html;
  html << "<!DOCTYPE html>\n<html><head><title>" << status << " "
       << reasonPhrase(status) << "</title></head>\n<body><h1>" << status << " "
       << reasonPhrase(status) << "</h1></body></html>\n";
  return html.str();
}

// The page at "uri" as a regular file, or false.
bool readConfiguredPage(const std::string& uri, const Router& router,
                        std::string& fsPath, std::string& content) {
  return router.mapToFile(uri, fsPath) && fileKind(fsPath) == FILE_REGULAR &&
         readWholeFile(fsPath, limits::kMaxErrorPageSize, content);
}

}  // namespace

Response buildStatusPage(int status,
                         const std::map<int, std::string>& configuredPages,
                         const Router& router) {
  Response response;
  response.status = status;
  const std::map<int, std::string>::const_iterator page =
      configuredPages.find(status);
  std::string fsPath;
  if (page != configuredPages.end() &&
      readConfiguredPage(page->second, router, fsPath, response.body)) {
    response.headers.add(header_name::kContentType, mimeTypeFor(fsPath));
  } else {
    response.body = defaultPage(status);
    response.headers.add(header_name::kContentType, mime_types::kHtmlType);
  }
  return response;
}
