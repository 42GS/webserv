/**
 * @file  DeleteHandler.hpp
 * @brief DELETE of a file.
 *        파일의 DELETE.
 */
#ifndef HANDLER_DELETEHANDLER_HPP
#define HANDLER_DELETEHANDLER_HPP

#include "http/Response.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief Removes the file of a DELETE route and answers 204.
 *        DELETE 결정의 파일을 지우고 204로 답한다.
 *
 * The router has already checked that it is a regular file in a
 * directory the server may change. 204: done, with nothing more to say
 * (RFC 9110 section 9.3.5).
 * router가 이미 서버가 바꿀 수 있는 디렉토리 안의 일반 파일인지 확인했다.
 * 204: 실행했고 더 알릴 것이 없다 (RFC 9110 9.3.5절).
 *
 * @param[in]  route    The routing decision. / 라우팅 결정.
 * @param[out] response Response to fill. / 채울 응답.
 * @return False if the file could not be removed; the caller answers 500.
 *         파일을 지우지 못하면 false. 호출자가 500으로 답한다.
 */
bool deleteFile(const RouteResult& route, Response& response);

#endif
