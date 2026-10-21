/**
 * @file  Multipart.hpp
 * @brief multipart/form-data bodies (RFC 7578, RFC 2046 section 5.1).
 *        multipart/form-data 본문 (RFC 7578, RFC 2046 5.1절).
 */
#ifndef HTTP_MULTIPART_HPP
#define HTTP_MULTIPART_HPP

#include <string>
#include <vector>

/**
 * @brief One part of a form-data body.
 *        form-data 본문의 part 하나.
 *
 * The content stays in the body and is given as a range, so that a large
 * upload is not copied.
 * 내용은 본문 안에 두고 범위로만 알려 준다. 큰 업로드를 복사하지 않기
 * 위해서다.
 */
struct FormPart {
  /** @brief A part without a file name. / 파일 이름이 없는 part. */
  FormPart() : hasFilename(false), contentStart(0), contentLength(0) {}

  /** "name" parameter, the form field. / "name" parameter, 폼 필드. */
  std::string name;
  /**
   * @brief True if a "filename" parameter was given.
   *        "filename" parameter가 있었으면 true.
   */
  bool hasFilename;
  /**
   * @brief "filename" as sent, possibly with a path.
   *        보낸 그대로의 "filename". 경로가 있을 수 있다.
   */
  std::string filename;
  /** Offset of the content in the body. / 본문 안에서 내용의 시작 위치. */
  std::string::size_type contentStart;
  /** Length of the content. / 내용의 길이. */
  std::string::size_type contentLength;
};

/**
 * @brief True if the media type of Content-Type is multipart/form-data.
 *        Content-Type의 미디어 타입이 multipart/form-data이면 true.
 * @param[in] contentType Content-Type value. / Content-Type 값.
 * @return Whether it is form data. / form data인지.
 */
bool isFormData(const std::string& contentType);

/**
 * @brief The boundary parameter of a multipart Content-Type.
 *        multipart Content-Type의 boundary parameter.
 * @param[in]  contentType Content-Type value. / Content-Type 값.
 * @param[out] boundary    The boundary. / boundary.
 * @return False if the parameters are malformed or the boundary is
 *         missing or not 1 to 70 bchars without a final space (RFC 2046
 *         section 5.1.1).
 *         parameter가 잘못됐거나, boundary가 없거나, 끝이 공백이 아닌
 *         bchars 1~70자가 아니면 false (RFC 2046 5.1.1절).
 */
bool formDataBoundary(const std::string& contentType, std::string& boundary);

/**
 * @brief Splits a form-data body into parts.
 *        form-data 본문을 part로 나눈다.
 *
 * The preamble and the epilogue are ignored. The CRLF before each
 * delimiter belongs to the delimiter, not to the part before it.
 * preamble과 epilogue는 무시한다. 구분자 앞의 CRLF는 앞 part가 아니라
 * 구분자에 속한다.
 *
 * @param[in]  body     Request body. / 요청 본문.
 * @param[in]  boundary Boundary from formDataBoundary().
 *                      formDataBoundary()가 준 boundary.
 * @param[out] parts    The parts in order. / 순서대로의 part.
 * @return False if there is no first or close delimiter, a delimiter line
 *         has other text, or a part lacks "Content-Disposition: form-data"
 *         with a name (RFC 7578 section 4.2).
 *         첫 구분자나 마지막 구분자가 없거나, 구분자 줄에 다른 글자가
 *         있거나, part에 name이 있는 "Content-Disposition: form-data"가
 *         없으면 false (RFC 7578 4.2절).
 */
bool parseFormData(const std::string& body, const std::string& boundary,
                   std::vector<FormPart>& parts);

#endif
