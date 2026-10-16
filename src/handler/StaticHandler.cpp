#include "handler/StaticHandler.hpp"

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FileReader.hpp"
#include "http/HeaderNames.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusCodes.hpp"

bool serveStaticFile(const RouteResult& route, Response& response) {
  std::size_t size = 0;
  if (!regularFileSize(route.fsPath, size)) return false;
  response.status = http_status::kOk;
  response.headers.add(header_name::kContentType, mimeTypeFor(route.fsPath));
  if (size <= limits::kInMemoryFileLimit)
    return readWholeFile(route.fsPath, limits::kInMemoryFileLimit,
                         response.body);
  response.bodyFile = route.fsPath;
  response.bodyFileSize = size;
  return true;
}
