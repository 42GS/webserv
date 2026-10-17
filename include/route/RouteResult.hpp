/**
 * @file  RouteResult.hpp
 * @brief The decision of the router.
 *        router의 결정.
 */
#ifndef ROUTE_ROUTERESULT_HPP
#define ROUTE_ROUTERESULT_HPP

#include <string>

/**
 * @brief How a request is to be answered.
 *        요청에 어떻게 답할지.
 *
 * The CGI kind comes with step 7.
 * CGI 종류는 7단계에서 넣는다.
 */
struct RouteResult {
  /**
   * @brief Kinds of answers.
   *        답의 종류.
   */
  enum Kind {
    /** Serve fsPath. / fsPath 파일을 보낸다. */
    STATIC_FILE,
    /** List the directory fsPath. / fsPath 디렉토리 목록을 보낸다. */
    DIRECTORY_LISTING,
    /** Redirect to redirectTo. / redirectTo로 리다이렉트한다. */
    REDIRECT,
    /** Store the body's files in uploadStore. / 본문 파일을 uploadStore에 저장.
     */
    UPLOAD,
    /** Delete the file fsPath. / fsPath 파일을 지운다. */
    DELETE,
    /** Answer status with an error page. / status와 오류 페이지로 답한다. */
    ERROR
  };

  /** @brief An ERROR with status 0. / status가 0인 ERROR. */
  RouteResult() : kind(ERROR), status(0) {}

  /** Kind of answer. / 답의 종류. */
  Kind kind;
  /** Status of REDIRECT and ERROR. / REDIRECT와 ERROR의 상태 코드. */
  int status;
  /** REDIRECT: Location value. / REDIRECT: Location 값. */
  std::string redirectTo;
  /** ERROR 405: Allow value. / ERROR 405: Allow 값. */
  std::string allow;
  /**
   * @brief STATIC_FILE, DIRECTORY_LISTING, DELETE: file system path of
   *        the requested resource.
   *        STATIC_FILE, DIRECTORY_LISTING, DELETE: 요청한 리소스의 파일
   *        시스템 경로.
   */
  std::string fsPath;
  /** UPLOAD: directory to store in. / UPLOAD: 저장할 디렉토리. */
  std::string uploadStore;
  /**
   * @brief Normalized request path (for listings and upload locations).
   *        정규화한 요청 경로 (목록과 업로드 Location용).
   */
  std::string urlPath;
};

#endif
