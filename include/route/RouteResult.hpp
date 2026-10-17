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
    /**
     * @brief Store the body's files in uploadStore.
     *        본문의 파일을 uploadStore에 저장한다.
     */
    UPLOAD,
    /** Delete the file fsPath. / fsPath 파일을 지운다. */
    DELETE,
    /** Run the script fsPath. / fsPath 스크립트를 실행한다. */
    CGI,
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
   * @brief STATIC_FILE, DIRECTORY_LISTING, DELETE, CGI: file system path
   *        of the requested resource (for CGI, the script).
   *        STATIC_FILE, DIRECTORY_LISTING, DELETE, CGI: 요청한 리소스의
   *        파일 시스템 경로 (CGI는 스크립트).
   */
  std::string fsPath;
  /** UPLOAD: directory to store in. / UPLOAD: 저장할 디렉토리. */
  std::string uploadStore;
  /** CGI: program that runs the script. / CGI: 스크립트를 실행할 프로그램. */
  std::string interpreter;
  /**
   * @brief CGI: URI path of the script, decoded.
   *        CGI: 디코딩한 스크립트 URI 경로.
   */
  std::string scriptName;
  /**
   * @brief CGI: the path after the script, decoded, possibly empty.
   *        CGI: 스크립트 뒤의 경로. 디코딩했고 비어 있을 수 있다.
   */
  std::string pathInfo;
  /**
   * @brief CGI: pathInfo mapped like a request path, empty if it cannot
   *        be (RFC 3875 section 4.1.6).
   *        CGI: pathInfo를 요청 경로처럼 매핑한 것. 매핑할 수 없으면 빈 값
   *        (RFC 3875 4.1.6절).
   */
  std::string pathTranslated;
  /**
   * @brief Normalized request path (for listings and upload locations).
   *        정규화한 요청 경로 (목록과 업로드 Location용).
   */
  std::string urlPath;
};

#endif
