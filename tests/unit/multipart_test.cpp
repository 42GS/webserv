#include "http/Multipart.hpp"

#include <string>
#include <vector>

#include "unit/TestFramework.hpp"

// Test IDs (F-xx) refer to the test plan in the wiki (analysis/test-plan).

namespace {

std::string contentOf(const std::string& body, const FormPart& part) {
  return body.substr(part.contentStart, part.contentLength);
}

bool parses(const std::string& body) {
  std::vector<FormPart> parts;
  return parseFormData(body, "b", parts);
}

}  // namespace

TEST_CASE(f05_f06_contentTypeAndBoundary) {
  CHECK(isFormData("Multipart/Form-Data; boundary=x"));
  CHECK(!isFormData("multipart/mixed; boundary=x"));
  CHECK(!isFormData("text/plain"));
  std::string boundary;
  CHECK(formDataBoundary("multipart/form-data; boundary=----Web12", boundary));
  CHECK_EQ(std::string("----Web12"), boundary);
  CHECK(formDataBoundary("multipart/form-data; boundary=\"a b:c\"", boundary));
  CHECK_EQ(std::string("a b:c"), boundary);
  CHECK(!formDataBoundary("multipart/form-data", boundary));
  CHECK(!formDataBoundary("multipart/form-data; boundary=\"\"", boundary));
  CHECK(!formDataBoundary("multipart/form-data; boundary=\"ab \"", boundary));
  CHECK(!formDataBoundary("multipart/form-data; boundary=\"a@b\"", boundary));
  CHECK(formDataBoundary(
      "multipart/form-data; boundary=" + std::string(70, 'x'), boundary));
  CHECK(!formDataBoundary(
      "multipart/form-data; boundary=" + std::string(71, 'x'), boundary));
}

TEST_CASE(f01_f03_partsAndContentWithoutTheDelimiterCrlf) {
  const std::string body =
      "--b\r\n"
      "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "line\r\n"
      "\r\n"
      "--b\r\n"
      "content-disposition: form-data; name=x\r\n"
      "\r\n"
      "v\r\n"
      "--b--\r\n";
  std::vector<FormPart> parts;
  CHECK(parseFormData(body, "b", parts));
  CHECK_EQ(2u, parts.size());
  if (parts.size() != 2) return;
  CHECK_EQ(std::string("f"), parts[0].name);
  CHECK(parts[0].hasFilename);
  CHECK_EQ(std::string("a.txt"), parts[0].filename);
  CHECK_EQ(std::string("line\r\n"), contentOf(body, parts[0]));
  CHECK_EQ(std::string("x"), parts[1].name);
  CHECK(!parts[1].hasFilename);
  CHECK_EQ(std::string("v"), contentOf(body, parts[1]));
}

TEST_CASE(f04_preambleEpiloguePaddingAndBinary) {
  const std::string content("a\0\r\n-\r\n--c", 10);
  const std::string body =
      "preamble\r\n--b  \t\r\n"
      "Content-Disposition: form-data; name=\"f\"; filename=\"z\"\r\n"
      "\r\n" +
      content + "\r\n--b--  \r\nepilogue --b\r\n";
  std::vector<FormPart> parts;
  CHECK(parseFormData(body, "b", parts));
  CHECK_EQ(1u, parts.size());
  if (parts.size() == 1) CHECK_EQ(content, contentOf(body, parts[0]));
}

TEST_CASE(f04_partWithEmptyContent) {
  const std::string body =
      "--b\r\nContent-Disposition: form-data; name=e; filename=\"\"\r\n"
      "\r\n\r\n--b--";
  std::vector<FormPart> parts;
  CHECK(parseFormData(body, "b", parts));
  CHECK_EQ(1u, parts.size());
  if (parts.size() == 1) CHECK_EQ(0u, parts[0].contentLength);
}

TEST_CASE(f06_malformedBodiesAreRejected) {
  const std::string disposition =
      "Content-Disposition: form-data; name=f\r\n\r\n";
  CHECK(!parses(""));
  CHECK(!parses("no delimiter"));
  CHECK(!parses("--b\r\n" + disposition + "x"));              // no close
  CHECK(!parses("--b\r\n" + disposition + "x\r\n--bx\r\n"));  // longer boundary
  CHECK(!parses("--b\r\n\r\nx\r\n--b--"));                    // no headers
  CHECK(!parses("--b\r\nContent-Type: text/plain\r\n\r\nx\r\n--b--"));
  CHECK(
      !parses("--b\r\nContent-Disposition: attachment; name=f\r\n\r\n"
              "x\r\n--b--"));
  CHECK(!parses("--b\r\nContent-Disposition: form-data\r\n\r\nx\r\n--b--"));
  CHECK(!parses("--b\r\n" + disposition.substr(0, disposition.size() - 2) +
                "Content-Disposition: form-data; name=g\r\n\r\nx\r\n--b--"));
  CHECK(!parses("--b\r\nno colon\r\n\r\nx\r\n--b--"));
  CHECK(parses("--b\r\n" + disposition + "x\r\n--b--"));
}
