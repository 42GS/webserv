/**
 * @file  Methods.hpp
 * @brief The request methods this server implements.
 *        이 서버가 구현한 요청 메서드.
 */
#ifndef HTTP_METHODS_HPP
#define HTTP_METHODS_HPP

#include <string>

/**
 * @brief Method names.
 *        메서드 이름.
 */
namespace methods {

/** GET */
const char* const kGet = "GET";
/** HEAD */
const char* const kHead = "HEAD";
/** POST */
const char* const kPost = "POST";
/** DELETE */
const char* const kDelete = "DELETE";

}  // namespace methods

/**
 * @brief True for GET, HEAD, POST and DELETE.
 *        GET, HEAD, POST, DELETE이면 true.
 *
 * The request parser answers 501 to any other method, and "limit_except"
 * accepts only these, so both use this one list.
 * 요청 파서는 다른 메서드에 501로 답하고 "limit_except"도 이것만 받으므로,
 * 둘 다 이 목록 하나를 쓴다.
 *
 * @param[in] method Method name, case-sensitive. / 메서드 이름, 대소문자 구분.
 * @return Whether it is implemented. / 구현한 메서드인지.
 */
bool isImplementedMethod(const std::string& method);

/**
 * @brief Length of the longest implemented method name.
 *        구현한 메서드 이름 중 가장 긴 것의 길이.
 *
 * A longer method is 501 even before the request line ends (RFC 9112
 * section 3).
 * 그보다 긴 메서드는 요청 줄이 끝나기 전에도 501이다 (RFC 9112 3절).
 *
 * @return The length. / 길이.
 */
std::string::size_type longestImplementedMethod();

#endif
