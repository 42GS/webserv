/**
 * @file  AutoindexHandler.cpp
 * @brief HTML directory listings.
 *        HTML 디렉토리 목록.
 */
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

/**
 * @brief Escapes the five HTML special characters.
 *        HTML 특수 문자 다섯 개를 이스케이프한다.
 * @param[in] text Text. / 텍스트.
 * @return Text safe in HTML content and attribute values.
 *         HTML 본문과 속성 값에 넣어도 안전한 텍스트.
 */
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

/**
 * @brief Names in the directory except "." and "..", sorted.
 *        "."과 ".."을 뺀 디렉토리 안의 이름, 정렬한 것.
 *
 * Subdirectories get a trailing "/".
 * 하위 디렉토리에는 끝에 "/"를 붙인다.
 *
 * @param[in]  directory Directory path ending in "/". / "/"로 끝나는 디렉토리
 * 경로.
 * @param[out] names     The names. / 이름 목록.
 * @return False if the directory cannot be opened. / 디렉토리를 열 수 없으면
 * false.
 */
bool readNames(const std::string& directory, std::vector<std::string>& names) {
  DIR* stream = opendir(directory.c_str());
  if (stream == 0) return false;
  for (dirent* entry = readdir(stream); entry != 0; entry = readdir(stream)) {
    const std::string name(entry->d_name);
    if (name == "." || name == "..") continue;
    names.push_back(fileKind(directory + name) == FILE_DIRECTORY ? name + "/"
                                                                 : name);
  }
  // Nothing to recover if closing fails. / 닫기에 실패해도 되돌릴 것이 없다.
  closedir(stream);
  std::sort(names.begin(), names.end());
  return true;
}

/**
 * @brief One list item linking to @p name.
 *        @p name으로 가는 링크 하나의 목록 항목.
 *
 * "./" keeps a name like "a:b" from being read as a URI scheme (RFC 3986
 * section 4.2).
 * "./"를 붙여서 "a:b" 같은 이름이 URI scheme으로 읽히지 않게 한다
 * (RFC 3986 4.2절).
 *
 * @param[in] name Entry name. / 항목 이름.
 * @return The HTML line. / HTML 한 줄.
 */
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
