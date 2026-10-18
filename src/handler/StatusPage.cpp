/**
 * @file  StatusPage.cpp
 * @brief Configured and built-in status pages.
 *        설정한 상태 페이지와 내장 상태 페이지.
 */
#include "handler/StatusPage.hpp"

#include <sstream>

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FileReader.hpp"
#include "http/HeaderNames.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusText.hpp"

namespace {

/**
 * @brief The built-in page for a status.
 *        상태 코드의 내장 페이지.
 * @param[in] status Status code. / 상태 코드.
 * @return HTML naming the code and its reason. / 코드와 이유 문구를 담은 HTML.
 */
std::string defaultPage(int status) {
  std::ostringstream html;
  html << "<!DOCTYPE html>\n<html><head><title>" << status << " "
       << reasonPhrase(status) << "</title></head>\n<body><h1>" << status << " "
       << reasonPhrase(status) << "</h1></body></html>\n";
  return html.str();
}

/**
 * @brief Reads the page at @p uri if it maps to a regular file.
 *        @p uri가 일반 파일로 매핑되면 그 페이지를 읽는다.
 * @param[in]  uri     error_page URI. / error_page URI.
 * @param[in]  router  Maps the URI. / URI를 매핑한다.
 * @param[out] fsPath  The file, for its media type. / 미디어 타입을 정할 파일
 * 경로.
 * @param[out] content Page content. / 페이지 내용.
 * @return False if there is no such readable file within the size limit.
 *         크기 상한 안에서 읽을 수 있는 그런 파일이 없으면 false.
 */
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
