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
#include "http/Response.hpp"
#include "route/RouteResult.hpp"
#include "route/Router.hpp"

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
  /** 409: the name in conflict. / 409: 충돌한 이름. */
  std::string conflict;
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
 *         bad or too long file name, or no file; 413 for more than
 *         limits::kMaxUploadFiles files; 409 for a name that exists or
 *         repeats; 500 if a write fails, as it does when the store is not
 *         a directory.
 *         201. 다른 미디어 타입이면 415. 본문이 잘못됐거나, 파일 이름이
 *         잘못됐거나 너무 길거나, 파일이 없으면 400. 파일이
 *         limits::kMaxUploadFiles개보다 많으면 413. 이름이 이미 있거나
 *         겹치면 409. 쓰기에 실패하면 500. 저장 위치가 디렉토리가 아닐
 *         때도 그렇다.
 */
UploadResult storeUpload(const RouteResult& route, const Request& request);

/**
 * @brief The 201 response of a stored upload.
 *        저장한 업로드의 201 응답.
 *
 * Location names the first stored file and the body lists the stored
 * files (RFC 9110 section 9.3.3). A file's URI is the POST target's
 * directory plus its name, used only if it maps back to the stored file:
 * the store is a file system path that need not be under any location.
 * Location은 처음 저장한 파일을 가리키고 본문은 저장한 파일을 나열한다
 * (RFC 9110 9.3.3절). 파일의 URI는 POST 대상의 디렉토리 + 이름이고, 그
 * URI가 저장한 파일로 다시 매핑될 때만 쓴다. 저장 위치는 어느 location
 * 아래에도 없을 수 있는 파일 시스템 경로이기 때문이다.
 *
 * @param[in] result     A 201 result. / 201 결과.
 * @param[in] targetPath Normalized POST path. / 정규화한 POST 경로.
 * @param[in] router     Maps the URIs back. / URI를 다시 매핑한다.
 * @return The response. / 응답.
 */
Response createdResponse(const UploadResult& result,
                         const std::string& targetPath, const Router& router);

/**
 * @brief The 409 response, naming the file in conflict.
 *        충돌한 파일 이름을 알리는 409 응답.
 *
 * A 409 SHOULD let the user see the source of the conflict (RFC 9110
 * section 15.5.10), so this is used instead of an error page.
 * 409는 사용자가 충돌 원인을 알 수 있어야 하므로 (RFC 9110 15.5.10절,
 * SHOULD) 오류 페이지 대신 이것을 쓴다.
 *
 * @param[in] result A 409 result. / 409 결과.
 * @return The response. / 응답.
 */
Response conflictResponse(const UploadResult& result);

#endif
