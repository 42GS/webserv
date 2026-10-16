/**
 * @file  AutoindexHandler.hpp
 * @brief Directory listings.
 *        디렉토리 목록.
 */
#ifndef HANDLER_AUTOINDEXHANDLER_HPP
#define HANDLER_AUTOINDEXHANDLER_HPP

#include "http/Response.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief 200 with an HTML list of a DIRECTORY_LISTING route.
 *        DIRECTORY_LISTING 결정에 HTML 목록으로 200을 답한다.
 *
 * Names only, with a trailing "/" for directories (design 7).
 * 이름만 보이고, 디렉토리는 뒤에 "/"를 붙인다 (설계 7절).
 *
 * @param[in]  route    The routing decision. / 라우팅 결정.
 * @param[out] response Response to fill. / 채울 응답.
 * @return False if the directory cannot be read; the caller answers 403.
 *         디렉토리를 읽을 수 없으면 false. 호출자가 403으로 답한다.
 */
bool listDirectory(const RouteResult& route, Response& response);

#endif
