/**
 * @file  UploadHandler.hpp
 * @brief Storing the files of a multipart/form-data POST.
 *        multipart/form-data POST의 파일 저장.
 */
#ifndef HANDLER_UPLOADHANDLER_HPP
#define HANDLER_UPLOADHANDLER_HPP

#include <string>
#include <vector>

#include "http/Request.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief A file an upload stored.
 *        업로드가 저장한 파일.
 */
struct UploadedFile {
  /** Name in the store. / 저장 위치 안의 이름. */
  std::string name;
  /** File system path. / 파일 시스템 경로. */
  std::string path;
};

/**
 * @brief What an upload did.
 *        업로드의 결과.
 */
struct UploadResult {
  /** @brief A result with status 0. / status가 0인 결과. */
  UploadResult() : status(0) {}

  /** 201, or the error status. / 201, 또는 오류 상태 코드. */
  int status;
  /** 201: the stored files in body order. / 201: 본문 순서대로 저장한 파일. */
  std::vector<UploadedFile> files;
};

/**
 * @brief Stores every file part of the body in the UPLOAD route's store.
 *        본문의 파일 part를 모두 UPLOAD 결정의 저장 위치에 저장한다.
 *
 * Only multipart/form-data is accepted (design decision 7). A part is a
 * file when its "filename" is not empty; other fields are ignored. The
 * name loses any directory part (RFC 7578 section 4.2). Nothing is stored
 * unless every file can be: an existing name is a conflict (design
 * decision 17), and a failed write removes what this request wrote.
 * multipart/form-data만 받는다 (설계 결정 7). "filename"이 비어 있지 않은
 * part가 파일이고, 다른 필드는 무시한다. 이름에서 디렉토리 부분은 버린다
 * (RFC 7578 4.2절). 모든 파일을 저장할 수 있을 때만 저장한다. 이미 있는
 * 이름은 충돌이고 (설계 결정 17), 쓰기에 실패하면 이 요청이 쓴 것을
 * 지운다.
 *
 * @param[in] route   UPLOAD decision. / UPLOAD 결정.
 * @param[in] request The request with its body. / 본문이 있는 요청.
 * @return 201; 415 for another media type; 400 for a malformed body, a
 *         bad file name or no file; 409 for a name that exists or repeats;
 *         500 if a write fails, as it does when the store is not a
 *         directory.
 *         201. 다른 미디어 타입이면 415. 본문이 잘못됐거나, 파일 이름이
 *         잘못됐거나, 파일이 없으면 400. 이름이 이미 있거나 겹치면 409.
 *         쓰기에 실패하면 500. 저장 위치가 디렉토리가 아닐 때도 그렇다.
 */
UploadResult storeUpload(const RouteResult& route, const Request& request);

#endif
