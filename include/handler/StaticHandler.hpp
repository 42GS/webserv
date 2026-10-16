/**
 * @file  StaticHandler.hpp
 * @brief Static files.
 *        정적 파일.
 */
#ifndef HANDLER_STATICHANDLER_HPP
#define HANDLER_STATICHANDLER_HPP

#include "http/Response.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief 200 with the file of a STATIC_FILE route.
 *        STATIC_FILE 결정의 파일로 200을 답한다.
 *
 * Files up to limits::kInMemoryFileLimit are read into memory, larger
 * ones are streamed (design 8).
 * limits::kInMemoryFileLimit까지는 메모리로 읽고, 더 크면 나눠 보낸다
 * (설계 8절).
 *
 * @param[in]  route    The routing decision. / 라우팅 결정.
 * @param[out] response Response to fill. / 채울 응답.
 * @return False if the file cannot be read; the caller answers 500.
 *         파일을 읽을 수 없으면 false. 호출자가 500으로 답한다.
 */
bool serveStaticFile(const RouteResult& route, Response& response);

#endif
