/**
 * @file  StatusPage.hpp
 * @brief Error and redirect pages.
 *        오류·리다이렉트 페이지.
 */
#ifndef HANDLER_STATUSPAGE_HPP
#define HANDLER_STATUSPAGE_HPP

#include <map>
#include <string>

#include "http/Response.hpp"
#include "route/Router.hpp"

/**
 * @brief A response with the given status and its page.
 *        주어진 상태 코드와 그 페이지로 된 응답.
 *
 * The page is the file configured with error_page for that status (mapped
 * through the locations like any URI) or, if there is none or it cannot be
 * read, a built-in page (raw/subject.md:143). Used for errors and for the
 * body of redirects.
 * 페이지는 그 상태에 error_page로 지정한 파일(다른 URI처럼 location으로
 * 매핑)이고, 없거나 읽을 수 없으면 내장 페이지다 (raw/subject.md:143).
 * 오류와 리다이렉트의 본문에 쓴다.
 *
 * @param[in] status          Status code. / 상태 코드.
 * @param[in] configuredPages error_page settings. / error_page 설정.
 * @param[in] router          Maps the page URI to a file. / 페이지 URI를 파일로
 * 매핑한다.
 * @return The response; this never fails. / 응답. 실패하지 않는다.
 */
Response buildStatusPage(int status,
                         const std::map<int, std::string>& configuredPages,
                         const Router& router);

#endif
