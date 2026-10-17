/**
 * @file  DeleteHandler.cpp
 * @brief File removal.
 *        파일 지우기.
 */
#include "handler/DeleteHandler.hpp"

#include <cstdio>

#include "http/StatusCodes.hpp"

// std::remove: unlink() is not on the list of allowed functions (design
// decision 15).
// std::remove: unlink()는 허용 함수 목록에 없다 (설계 결정 15).
bool deleteFile(const RouteResult& route, Response& response) {
  if (std::remove(route.fsPath.c_str()) != 0) return false;
  response.status = http_status::kNoContent;
  return true;
}
