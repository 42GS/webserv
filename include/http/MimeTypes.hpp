/**
 * @file  MimeTypes.hpp
 * @brief Media types of files.
 *        파일의 미디어 타입.
 */
#ifndef HTTP_MIMETYPES_HPP
#define HTTP_MIMETYPES_HPP

#include <string>

/**
 * @brief Media type constants.
 *        미디어 타입 상수.
 */
namespace mime_types {

/** Type of generated pages. / 서버가 만드는 페이지의 타입. */
const char* const kHtmlType = "text/html";
/**
 * @brief Plain text: .txt files and the upload result bodies.
 *        일반 텍스트: .txt 파일과 업로드 결과 본문.
 */
const char* const kTextType = "text/plain";

}  // namespace mime_types

/**
 * @brief Media type for a file name by its extension.
 *        확장자로 파일 이름의 미디어 타입을 정한다.
 *
 * The extension is compared case-insensitively, as NGINX "types".
 * Unknown extensions get application/octet-stream (design 7).
 * 확장자는 NGINX "types"처럼 대소문자 구분 없이 비교한다. 모르는
 * 확장자는 application/octet-stream이다 (설계 7절).
 *
 * @param[in] fileName File name or path. / 파일 이름이나 경로.
 * @return The media type. / 미디어 타입.
 */
std::string mimeTypeFor(const std::string& fileName);

#endif
