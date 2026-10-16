#include "handler/StaticHandler.hpp"

#include <sys/stat.h>

#include "core/Limits.hpp"
#include "handler/FileReader.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusCodes.hpp"

bool serveStaticFile(const RouteResult& route, Response& response) {
  struct stat info;
  if (stat(route.fsPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
    return false;
  const std::size_t size = static_cast<std::size_t>(info.st_size);
  response.status = http_status::kOk;
  response.headers.add("Content-Type", mimeTypeFor(route.fsPath));
  if (size <= limits::kInMemoryFileLimit)
    return readWholeFile(route.fsPath, limits::kInMemoryFileLimit,
                         response.body);
  response.bodyFile = route.fsPath;
  response.bodyFileSize = size;
  return true;
}
