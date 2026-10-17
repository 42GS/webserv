/**
 * @file  CgiEnv.cpp
 * @brief Building the CGI meta-variables.
 *        CGI 메타변수 만들기.
 *
 * Section numbers refer to RFC 3875; the table with line references is
 * in wiki concepts/cgi section 2.
 * 절 번호는 RFC 3875의 것이다. 줄 번호가 있는 표는 위키 concepts/cgi 2절에
 * 있다.
 */
#include "cgi/CgiEnv.hpp"

#include <algorithm>
#include <cstddef>
#include <locale>
#include <sstream>

#include "fs/FilePath.hpp"
#include "http/Ascii.hpp"
#include "http/HeaderNames.hpp"

namespace {

/** GATEWAY_INTERFACE (section 4.1.4). */
const char* const kGatewayInterface = "CGI/1.1";
/** SERVER_SOFTWARE (section 4.1.17). */
const char* const kServerSoftware = "webserv/1.0";
/**
 * @brief REDIRECT_STATUS: php-cgi only checks that it is set (wiki
 *        concepts/cgi "php-cgi가 추가로 요구하는 변수").
 *        REDIRECT_STATUS: php-cgi는 있는지만 본다 (위키 concepts/cgi
 *        "php-cgi가 추가로 요구하는 변수").
 */
const char* const kRedirectStatus = "200";
/**
 * @brief Prefix of request header variables (section 4.1.18).
 *        요청 헤더 변수의 접두어 (4.1.18절).
 */
const char* const kHttpPrefix = "HTTP_";

/**
 * @brief Header fields not passed as HTTP_ variables.
 *        HTTP_ 변수로 넘기지 않는 헤더 필드.
 *
 * Authorization carries credentials and Content-Length/Content-Type have
 * their own variables (SHOULD remove, section 4.1.18). Transfer-Encoding
 * no longer describes the body, which has been decoded (section 4.2).
 * Authorization은 인증 정보이고 Content-Length·Content-Type은 자기 변수가
 * 있다 (빼야 함 SHOULD, 4.1.18절). Transfer-Encoding은 해제한 본문을 더는
 * 설명하지 않는다 (4.2절).
 */
const char* const kHiddenHeaders[] = {"authorization", "content-length",
                                      "content-type", "transfer-encoding"};
/** Number of entries in kHiddenHeaders. / kHiddenHeaders의 항목 수. */
const std::size_t kHiddenHeaderCount =
    sizeof(kHiddenHeaders) / sizeof(kHiddenHeaders[0]);

/**
 * @brief True for a header field that gets no HTTP_ variable.
 *        HTTP_ 변수를 만들지 않는 헤더 필드이면 true.
 * @param[in] name Field name. / 필드 이름.
 * @return Whether it is hidden. / 빼는 필드인지.
 */
bool isHidden(const std::string& name) {
  const std::string lowered = toAsciiLower(name);
  for (std::size_t i = 0; i < kHiddenHeaderCount; ++i) {
    if (lowered == kHiddenHeaders[i]) return true;
  }
  return false;
}

/**
 * @brief The variable name of a header field: upper case, "-" as "_",
 *        with "HTTP_" in front.
 *        헤더 필드의 변수 이름: 대문자로, "-"는 "_"로, 앞에 "HTTP_".
 * @param[in] name Field name. / 필드 이름.
 * @return The variable name. / 변수 이름.
 */
std::string variableFor(const std::string& name) {
  std::string variable(kHttpPrefix);
  for (std::string::size_type i = 0; i < name.size(); ++i)
    variable +=
        name[i] == '-' ? '_' : std::toupper(name[i], std::locale::classic());
  return variable;
}

/**
 * @brief Appends one HTTP_ variable per header name; repeated fields are
 *        joined with ", " (MUST, section 4.1.18).
 *        헤더 이름마다 HTTP_ 변수를 하나 붙인다. 같은 필드가 여러 개면
 *        ", "로 합친다 (MUST, 4.1.18절).
 * @param[in]     request The request. / 요청.
 * @param[in,out] env     Environment to extend. / 늘릴 환경.
 */
void addHeaderVariables(const Request& request, std::vector<std::string>& env) {
  std::vector<std::string> done;
  for (std::size_t i = 0; i < request.headers.size(); ++i) {
    const std::string& name = request.headers.at(i).first;
    const std::string variable = variableFor(name);
    if (isHidden(name) ||
        std::find(done.begin(), done.end(), variable) != done.end())
      continue;
    done.push_back(variable);
    const std::vector<std::string> values = request.headers.all(name);
    std::string joined;
    for (std::size_t j = 0; j < values.size(); ++j)
      joined += (j == 0 ? "" : ", ") + values[j];
    env.push_back(variable + "=" + joined);
  }
}

/**
 * @brief Decimal text of a number.
 *        수의 십진 텍스트.
 * @param[in] value The number. / 수.
 * @return The text. / 텍스트.
 */
std::string decimal(std::size_t value) {
  std::ostringstream text;
  text << value;
  return text.str();
}

}  // namespace

std::vector<std::string> buildCgiEnvironment(const Request& request,
                                             const RouteResult& script,
                                             const ListenContext& where) {
  std::vector<std::string> env;
  env.push_back(std::string("GATEWAY_INTERFACE=") + kGatewayInterface);
  env.push_back("REQUEST_METHOD=" + request.method);
  env.push_back("QUERY_STRING=" + request.query);
  env.push_back("SCRIPT_NAME=" + script.scriptName);
  env.push_back("SERVER_NAME=" + where.localAddress);
  env.push_back("SERVER_PORT=" + decimal(where.localPort));
  env.push_back("SERVER_PROTOCOL=" + request.version);
  env.push_back(std::string("SERVER_SOFTWARE=") + kServerSoftware);
  env.push_back("REMOTE_ADDR=" + where.remoteAddress);
  // No name lookup is done; the address may stand in (section 4.1.9).
  // 이름을 찾지 않으므로 주소로 대신한다 (4.1.9절).
  env.push_back("REMOTE_HOST=" + where.remoteAddress);
  // If and only if the request has a body, after decoding (section 4.1.2).
  // 요청에 본문이 있을 때만, 해제한 길이로 (4.1.2절).
  if (request.headers.has(header_name::kContentLength) ||
      request.headers.has(header_name::kTransferEncoding))
    env.push_back("CONTENT_LENGTH=" + decimal(request.body.size()));
  const std::string* contentType =
      request.headers.first(header_name::kContentType);
  if (contentType != 0) env.push_back("CONTENT_TYPE=" + *contentType);
  if (!script.pathInfo.empty()) {
    env.push_back("PATH_INFO=" + script.pathInfo);
    if (!script.pathTranslated.empty() && script.pathTranslated[0] == '/')
      env.push_back("PATH_TRANSLATED=" + script.pathTranslated);
  }
  env.push_back(std::string("REDIRECT_STATUS=") + kRedirectStatus);
  env.push_back("SCRIPT_FILENAME=" + (script.fsPath[0] == '/'
                                          ? script.fsPath
                                          : baseName(script.fsPath)));
  addHeaderVariables(request, env);
  return env;
}
