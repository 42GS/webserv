/**
 * @file  UploadHandler.cpp
 * @brief Upload storage.
 *        업로드 저장.
 */
#include "handler/UploadHandler.hpp"

#include <cstddef>
#include <cstdio>
#include <set>
#include <vector>

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FileWriter.hpp"
#include "http/HeaderNames.hpp"
#include "http/MimeTypes.hpp"
#include "http/Multipart.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

namespace {

/**
 * @brief Longest file name in bytes.
 *        파일 이름의 최대 바이트 수.
 *
 * Ubuntu 22.04 refuses longer names with "File name too long"; macOS
 * counted characters and took more (measured 2026-10-17). A longer name
 * is the client's error, not a failed write.
 * Ubuntu 22.04는 더 긴 이름을 "File name too long"으로 거부했고, macOS는
 * 글자 수로 세어 더 받았다 (2026-10-17 실측). 더 긴 이름은 쓰기 실패가
 * 아니라 클라이언트 오류다.
 */
const std::string::size_type kMaxFileNameBytes = 255;
/** Status of a check that found nothing wrong. / 문제를 찾지 못한 검사의 상태.
 */
const int kNoFailure = 0;

/**
 * @brief A file part with the name and path it is stored under.
 *        저장할 이름과 경로를 정한 파일 part.
 */
struct StoredFile {
  /** Safe name and path in the store. / 안전한 이름과 저장 위치 안의 경로. */
  UploadedFile file;
  /** The part in the body. / 본문 안의 part. */
  FormPart part;
};

/**
 * @brief A result with @p status and nothing else.
 *        @p status만 있는 결과.
 * @param[in] status Status code. / 상태 코드.
 * @return The result. / 결과.
 */
UploadResult resultWith(int status) {
  UploadResult result;
  result.status = status;
  return result;
}

/**
 * @brief The file name to store a "filename" under.
 *        "filename"을 저장할 파일 이름.
 *
 * Anything up to the last "/" or "\" is a directory path, which must not
 * be used (RFC 7578 section 4.2); "\" is the separator of Windows paths.
 * 마지막 "/"나 "\"까지는 디렉토리 경로라서 쓰면 안 된다 (RFC 7578
 * 4.2절). "\"는 Windows 경로의 구분자다.
 *
 * @param[in]  filename "filename" as sent. / 보낸 그대로의 "filename".
 * @param[out] name     The last path segment. / 경로의 마지막 조각.
 * @return False if that is empty, "." or "..", or longer than
 *         kMaxFileNameBytes.
 *         그것이 비었거나 "."이나 ".."이거나, kMaxFileNameBytes보다 길면
 *         false.
 */
bool safeFileName(const std::string& filename, std::string& name) {
  const std::string::size_type separator = filename.find_last_of("/\\");
  name = separator == std::string::npos ? filename
                                        : filename.substr(separator + 1);
  return !name.empty() && name != "." && name != ".." &&
         name.size() <= kMaxFileNameBytes;
}

/**
 * @brief A path inside a directory.
 *        디렉토리 안의 경로.
 * @param[in] directory Directory, with or without a final "/".
 *                      디렉토리. 끝에 "/"가 있어도 없어도 된다.
 * @param[in] name      File name. / 파일 이름.
 * @return The path. / 경로.
 */
std::string pathIn(const std::string& directory, const std::string& name) {
  if (!directory.empty() && directory[directory.size() - 1] == '/')
    return directory + name;
  return directory + "/" + name;
}

/**
 * @brief Names and paths for the file parts, checked before anything is
 *        written.
 *        아무것도 쓰기 전에 검사한, 파일 part의 이름과 경로.
 *
 * An empty "filename" names no file, so such a part is skipped like
 * other fields.
 * 빈 "filename"은 파일을 가리키지 않으므로, 그런 part는 다른 필드처럼
 * 건너뛴다.
 *
 * @param[in]  store Upload directory. / 업로드 디렉토리.
 * @param[in]  parts Parts of the body. / 본문의 part.
 * @param[out] files Files to store. / 저장할 파일.
 * @return Status kNoFailure if they can all be stored, else the failure
 *         as for storeUpload(): 400, 413, or 409 with the name.
 *         모두 저장할 수 있으면 상태가 kNoFailure, 아니면
 *         storeUpload()와 같은 실패: 400, 413, 또는 이름이 있는 409.
 */
UploadResult planFiles(const std::string& store,
                       const std::vector<FormPart>& parts,
                       std::vector<StoredFile>& files) {
  std::set<std::string> names;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (!parts[i].hasFilename || parts[i].filename.empty()) continue;
    // Checked first, so that no more names are looked up on disk.
    // 디스크에서 이름을 더 찾지 않도록 먼저 검사한다.
    if (files.size() == limits::kMaxUploadFiles)
      return resultWith(http_status::kContentTooLarge);

    StoredFile stored;
    if (!safeFileName(parts[i].filename, stored.file.name))
      return resultWith(http_status::kBadRequest);
    stored.file.path = pathIn(store, stored.file.name);
    if (!names.insert(stored.file.name).second ||
        fileKind(stored.file.path) != FILE_MISSING) {
      UploadResult result = resultWith(http_status::kConflict);
      result.conflict = stored.file.name;
      return result;
    }

    stored.part = parts[i];
    files.push_back(stored);
  }

  return resultWith(files.empty() ? http_status::kBadRequest : kNoFailure);
}

/**
 * @brief Writes the content of one part to its new file.
 *        part 하나의 내용을 새 파일에 쓴다.
 * @param[in] body   Request body holding the content. / 내용이 든 요청 본문.
 * @param[in] stored The part and its path. / part와 그 경로.
 * @return False if the file could not be written. / 쓰지 못하면 false.
 */
bool writeFile(const std::string& body, const StoredFile& stored) {
  return writeNewFile(stored.file.path, body.data() + stored.part.contentStart,
                      stored.part.contentLength);
}

/**
 * @brief Removes the first @p count files.
 *        앞의 @p count개 파일을 지운다.
 *
 * std::remove: design decision 15. If it fails too, the file stays;
 * nothing more can be done here.
 * std::remove: 설계 결정 15. 이것도 실패하면 파일이 남는다. 여기서 더 할
 * 수 있는 것이 없다.
 *
 * @param[in] files Files written. / 쓴 파일.
 * @param[in] count How many to remove. / 지울 개수.
 */
void removeFiles(const std::vector<StoredFile>& files, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i)
    static_cast<void>(std::remove(files[i].file.path.c_str()));
}

/**
 * @brief Writes the files, or none of them.
 *        파일을 모두 쓰거나 하나도 쓰지 않는다.
 * @param[in] body  Request body holding the contents. / 내용이 든 요청 본문.
 * @param[in] files Files to write. / 쓸 파일.
 * @return False if a write failed; the files written before are removed.
 *         쓰기에 실패하면 false. 그 전에 쓴 파일은 지운다.
 */
bool writeFiles(const std::string& body, const std::vector<StoredFile>& files) {
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (!writeFile(body, files[i])) {
      removeFiles(files, i);
      return false;
    }
  }
  return true;
}

}  // namespace

UploadResult storeUpload(const RouteResult& route, const Request& request) {
  const std::string* contentType =
      request.headers.first(header_name::kContentType);
  if (contentType == 0 || !isFormData(*contentType))
    return resultWith(http_status::kUnsupportedMediaType);

  std::string boundary;
  std::vector<FormPart> parts;
  if (!formDataBoundary(*contentType, boundary) ||
      !parseFormData(request.body, boundary, parts))
    return resultWith(http_status::kBadRequest);

  std::vector<StoredFile> files;
  const UploadResult failure = planFiles(route.uploadStore, parts, files);
  if (failure.status != kNoFailure) return failure;

  if (!writeFiles(request.body, files))
    return resultWith(http_status::kInternalServerError);

  UploadResult result = resultWith(http_status::kCreated);
  for (std::size_t i = 0; i < files.size(); ++i)
    result.files.push_back(files[i].file);
  return result;
}

Response createdResponse(const UploadResult& result,
                         const std::string& targetPath, const Router& router) {
  Response response;
  response.status = http_status::kCreated;
  response.headers.add(header_name::kContentType, mime_types::kTextType);

  const std::string directory = targetPath.substr(0, targetPath.rfind('/') + 1);
  for (std::size_t i = 0; i < result.files.size(); ++i) {
    const std::string uri = encodePath(directory + result.files[i].name);
    std::string mapped;
    const bool reachable =
        router.mapToFile(uri, mapped) && mapped == result.files[i].path;
    if (reachable && !response.headers.has(header_name::kLocation))
      response.headers.add(header_name::kLocation, uri);
    response.body += (reachable ? uri : result.files[i].name) + "\n";
  }

  return response;
}

Response conflictResponse(const UploadResult& result) {
  Response response;
  response.status = http_status::kConflict;
  response.headers.add(header_name::kContentType, mime_types::kTextType);
  response.body = "\"" + result.conflict +
                  "\" exists or is sent twice; nothing was stored.\n";
  return response;
}
