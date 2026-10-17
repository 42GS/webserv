/**
 * @file  Methods.hpp
 * @brief The request methods this server implements.
 *        이 서버가 구현한 요청 메서드.
 */
#ifndef HTTP_METHODS_HPP
#define HTTP_METHODS_HPP

#include <cstddef>
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

/**
 * @brief The implemented methods, in the order Allow lists them.
 *        구현한 메서드. Allow에 적는 순서다.
 *
 * The request parser answers 501 to any other method, "limit_except"
 * accepts only these, and the router builds Allow from them, so all use
 * this one list.
 * 요청 파서는 다른 메서드에 501로 답하고, "limit_except"도 이것만 받고,
 * router는 이것으로 Allow를 만든다. 그래서 모두 이 목록 하나를 쓴다.
 */
extern const char* const kImplemented[];
/** Number of entries in kImplemented. / kImplemented의 항목 수. */
extern const std::size_t kImplementedCount;

}  // namespace methods

/**
 * @brief True for a method in methods::kImplemented.
 *        methods::kImplemented에 있는 메서드이면 true.
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
