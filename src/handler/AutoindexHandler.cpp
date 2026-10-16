#include "handler/AutoindexHandler.hpp"

#include <dirent.h>

#include <algorithm>
#include <string>
#include <vector>

#include "fs/FileInfo.hpp"
#include "http/HeaderNames.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

namespace {

std::string escapeHtml(const std::string& text) {
  std::string escaped;
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    switch (text[i]) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&#39;";
        break;
      default:
        escaped += text[i];
    }
  }
  return escaped;
}

// Names in the directory except "." and "..", sorted, with "/" appended to
// subdirectories.
bool readNames(const std::string& directory, std::vector<std::string>& names) {
  DIR* stream = opendir(directory.c_str());
  if (stream == 0) return false;
  for (dirent* entry = readdir(stream); entry != 0; entry = readdir(stream)) {
    const std::string name(entry->d_name);
    if (name == "." || name == "..") continue;
    names.push_back(fileKind(directory + name) == FILE_DIRECTORY ? name + "/"
                                                                 : name);
  }
  closedir(stream);  // nothing to recover if closing fails
  std::sort(names.begin(), names.end());
  return true;
}

// "./" keeps a name like "a:b" from being read as a URI scheme
// (RFC 3986 section 4.2).
std::string listItem(const std::string& name) {
  return "<li><a href=\"./" + escapeHtml(encodePath(name)) + "\">" +
         escapeHtml(name) + "</a></li>\n";
}

}  // namespace

bool listDirectory(const RouteResult& route, Response& response) {
  std::vector<std::string> names;
  if (!readNames(route.fsPath, names)) return false;
  const std::string title = "Index of " + escapeHtml(route.urlPath);
  std::string html =
      "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
      "<title>" +
      title + "</title></head>\n<body><h1>" + title + "</h1>\n<ul>\n";
  if (route.urlPath != "/") html += "<li><a href=\"../\">../</a></li>\n";
  for (std::size_t i = 0; i < names.size(); ++i) html += listItem(names[i]);
  html += "</ul>\n</body></html>\n";
  response.status = http_status::kOk;
  response.headers.add(header_name::kContentType, mime_types::kHtmlType);
  response.body = html;
  return true;
}
