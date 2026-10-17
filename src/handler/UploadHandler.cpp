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

#include "fs/FileInfo.hpp"
#include "fs/FileWriter.hpp"
#include "http/HeaderNames.hpp"
#include "http/Multipart.hpp"
#include "http/StatusCodes.hpp"

namespace {

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
 * @brief An upload that failed with @p status.
 *        @p status로 실패한 업로드.
 * @param[in] status Error status. / 오류 상태 코드.
 * @return The result. / 결과.
 */
UploadResult failure(int status) {
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
 * @return False if that is empty, "." or "..". / 그것이 비었거나 "."이나
 * ".."이면 false.
 */
bool safeFileName(const std::string& filename, std::string& name) {
  const std::string::size_type separator = filename.find_last_of("/\\");
  name = separator == std::string::npos ? filename
                                        : filename.substr(separator + 1);
  return !name.empty() && name != "." && name != "..";
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
 * A browser sends filename="" for a file input left empty, so such parts
 * are skipped like other fields.
 * 브라우저는 비워 둔 파일 입력에 filename=""을 보내므로, 그런 part는 다른
 * 필드처럼 건너뛴다.
 *
 * @param[in]  store Upload directory. / 업로드 디렉토리.
 * @param[in]  parts Parts of the body. / 본문의 part.
 * @param[out] files Files to store. / 저장할 파일.
 * @return 201 if they can all be stored, else 400 or 409 as for
 *         storeUpload().
 *         모두 저장할 수 있으면 201, 아니면 storeUpload()와 같은 400이나
 *         409.
 */
int planFiles(const std::string& store, const std::vector<FormPart>& parts,
              std::vector<StoredFile>& files) {
  std::set<std::string> names;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (!parts[i].hasFilename || parts[i].filename.empty()) continue;
    StoredFile stored;
    if (!safeFileName(parts[i].filename, stored.file.name))
      return http_status::kBadRequest;
    stored.file.path = pathIn(store, stored.file.name);
    if (!names.insert(stored.file.name).second ||
        fileKind(stored.file.path) != FILE_MISSING)
      return http_status::kConflict;
    stored.part = parts[i];
    files.push_back(stored);
  }
  return files.empty() ? http_status::kBadRequest : http_status::kCreated;
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
    if (writeNewFile(files[i].file.path,
                     body.data() + files[i].part.contentStart,
                     files[i].part.contentLength))
      continue;
    // std::remove: design decision 15. If it fails too, the file stays;
    // nothing more can be done here.
    // std::remove: 설계 결정 15. 이것도 실패하면 파일이 남는다. 여기서 더
    // 할 수 있는 것이 없다.
    for (std::size_t done = 0; done < i; ++done)
      static_cast<void>(std::remove(files[done].file.path.c_str()));
    return false;
  }
  return true;
}

}  // namespace

UploadResult storeUpload(const RouteResult& route, const Request& request) {
  const std::string* contentType =
      request.headers.first(header_name::kContentType);
  if (contentType == 0 || !isFormData(*contentType))
    return failure(http_status::kUnsupportedMediaType);
  std::string boundary;
  std::vector<FormPart> parts;
  if (!formDataBoundary(*contentType, boundary) ||
      !parseFormData(request.body, boundary, parts))
    return failure(http_status::kBadRequest);
  std::vector<StoredFile> files;
  const int status = planFiles(route.fsPath, parts, files);
  if (status != http_status::kCreated) return failure(status);
  if (!writeFiles(request.body, files))
    return failure(http_status::kInternalServerError);
  UploadResult result;
  result.status = http_status::kCreated;
  for (std::size_t i = 0; i < files.size(); ++i)
    result.files.push_back(files[i].file);
  return result;
}
