#include "http/HeaderMap.hpp"

#include "http/Ascii.hpp"

namespace {

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

void HeaderMap::clear() { fields_.clear(); }

std::size_t HeaderMap::size() const { return fields_.size(); }

const HeaderMap::Field& HeaderMap::at(std::size_t index) const {
  return fields_.at(index);
}
