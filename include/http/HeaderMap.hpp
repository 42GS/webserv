#ifndef HTTP_HEADERMAP_HPP
#define HTTP_HEADERMAP_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Header fields in arrival order. Names compare case-insensitively
// (RFC 9112 section 5); the same name may appear on several lines, so
// duplicates (Host) can be detected and Set-Cookie lines kept apart.
class HeaderMap {
 public:
  void add(const std::string& name, const std::string& value);
  bool has(const std::string& name) const;
  // The value of the first line with this name, or 0 if there is none.
  const std::string* first(const std::string& name) const;
  std::vector<std::string> all(const std::string& name) const;
  std::size_t count(const std::string& name) const;
  void clear();

  // Fields in arrival order, for writing a response head.
  typedef std::pair<std::string, std::string> Field;
  std::size_t size() const;
  const Field& at(std::size_t index) const;

 private:
  std::vector<Field> fields_;
};

#endif
