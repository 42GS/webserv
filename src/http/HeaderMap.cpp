/**
 * @file  HeaderMap.cpp
 * @brief Case-insensitive header field storage.
 *        이름을 대소문자 구분 없이 찾는 헤더 필드 저장소.
 */
#include "http/HeaderMap.hpp"

#include "http/Ascii.hpp"

namespace {

/**
 * @brief Compares two field names ignoring ASCII case.
 *        두 필드 이름을 ASCII 대소문자 구분 없이 비교한다.
 * @param[in] a Name. / 이름.
 * @param[in] b Name. / 이름.
 * @return Whether they name the same field. / 같은 필드 이름인지.
 */
bool sameName(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::string::size_type i = 0; i < a.size(); ++i) {
    if (toAsciiLower(a[i]) != toAsciiLower(b[i])) return false;
  }
  return true;
}

}  // namespace

void HeaderMap::add(const std::string& name, const std::string& value) {
  fields_.push_back(Field(name, value));
}

bool HeaderMap::has(const std::string& name) const { return first(name) != 0; }

const std::string* HeaderMap::first(const std::string& name) const {
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    if (sameName(fields_[i].first, name)) return &fields_[i].second;
  }
  return 0;
}

std::vector<std::string> HeaderMap::all(const std::string& name) const {
  std::vector<std::string> values;
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    if (sameName(fields_[i].first, name)) values.push_back(fields_[i].second);
  }
  return values;
}

std::size_t HeaderMap::count(const std::string& name) const {
  return all(name).size();
}

std::size_t HeaderMap::size() const { return fields_.size(); }

const HeaderMap::Field& HeaderMap::at(std::size_t index) const {
  return fields_.at(index);
}
