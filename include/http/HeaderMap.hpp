/**
 * @file  HeaderMap.hpp
 * @brief Header fields of a request or a response.
 *        요청이나 응답의 헤더 필드.
 */
#ifndef HTTP_HEADERMAP_HPP
#define HTTP_HEADERMAP_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Header fields in arrival order.
 *        도착한 순서대로 보관하는 헤더 필드.
 *
 * Names compare case-insensitively (RFC 9112 section 5). The same name may
 * appear on several lines, so duplicates (Host) can be detected and
 * Set-Cookie lines kept apart.
 * 이름은 대소문자를 구분하지 않고 비교한다 (RFC 9112 5절). 같은 이름이
 * 여러 줄에 나올 수 있어서, 중복(Host)을 찾아낼 수 있고 Set-Cookie 줄을
 * 따로 유지할 수 있다.
 */
class HeaderMap {
 public:
  /**
   * @brief Appends a field.
   *        필드를 끝에 넣는다.
   * @param[in] name  Field name. / 필드 이름.
   * @param[in] value Field value. / 필드 값.
   */
  void add(const std::string& name, const std::string& value);

  /**
   * @brief True if a field with this name exists.
   *        이 이름의 필드가 있으면 true.
   * @param[in] name Field name, any case. / 필드 이름, 대소문자 무관.
   * @return Whether it exists. / 있는지.
   */
  bool has(const std::string& name) const;

  /**
   * @brief Value of the first field with this name.
   *        이 이름을 가진 첫 필드의 값.
   * @param[in] name Field name, any case. / 필드 이름, 대소문자 무관.
   * @return Pointer to the value, or 0 if there is none.
   *         값을 가리키는 포인터. 없으면 0.
   */
  const std::string* first(const std::string& name) const;

  /**
   * @brief Values of every field with this name, in order.
   *        이 이름을 가진 모든 필드의 값, 순서대로.
   * @param[in] name Field name, any case. / 필드 이름, 대소문자 무관.
   * @return The values. / 값 목록.
   */
  std::vector<std::string> all(const std::string& name) const;

  /**
   * @brief Number of fields with this name.
   *        이 이름을 가진 필드 수.
   * @param[in] name Field name, any case. / 필드 이름, 대소문자 무관.
   * @return The count. / 개수.
   */
  std::size_t count(const std::string& name) const;

  /** One field: name and value. / 필드 하나: 이름과 값. */
  typedef std::pair<std::string, std::string> Field;

  /**
   * @brief Number of fields, for writing a response head.
   *        필드 수. 응답 머리를 쓸 때 쓴다.
   */
  std::size_t size() const;

  /**
   * @brief Field at a position in arrival order.
   *        도착 순서상 해당 위치의 필드.
   * @param[in] index 0 .. size() - 1.
   * @return The field. / 필드.
   */
  const Field& at(std::size_t index) const;

 private:
  /** Fields in arrival order. / 도착 순서의 필드. */
  std::vector<Field> fields_;
};

#endif
