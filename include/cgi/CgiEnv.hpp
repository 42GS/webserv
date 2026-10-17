/**
 * @file  CgiEnv.hpp
 * @brief The meta-variables of a CGI request (RFC 3875 section 4.1).
 *        CGI 요청의 메타변수 (RFC 3875 4.1절).
 */
#ifndef CGI_CGIENV_HPP
#define CGI_CGIENV_HPP

#include <string>
#include <vector>

#include "core/ListenContext.hpp"
#include "http/Request.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief "NAME=value" strings for execve() (wiki concepts/cgi section 2).
 *        execve()에 넘길 "NAME=value" 문자열 (위키 concepts/cgi 2절).
 *
 * The script runs in its own directory, so a path given to it must work
 * from there: SCRIPT_FILENAME is the file name unless the script path is
 * absolute, and PATH_TRANSLATED is left out unless it is absolute
 * (getcwd() is not an allowed function).
 * 스크립트는 자기 디렉토리에서 실행되므로, 넘기는 경로는 거기서 통해야
 * 한다. SCRIPT_FILENAME은 스크립트 경로가 절대 경로가 아니면 파일 이름이고,
 * PATH_TRANSLATED는 절대 경로일 때만 넣는다 (getcwd()는 허용 함수가 아니다).
 *
 * @param[in] request The request. / 요청.
 * @param[in] script  CGI decision of the router. / router의 CGI 결정.
 * @param[in] where   Both ends of the connection. / 연결의 두 끝.
 * @return The environment. / 환경.
 */
std::vector<std::string> buildCgiEnvironment(const Request& request,
                                             const RouteResult& script,
                                             const ListenContext& where);

#endif
