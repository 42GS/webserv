/**
 * @file  PathResolver.hpp
 * @brief Normalization and encoding of URI paths.
 *        URI 경로의 정규화와 인코딩.
 */
#ifndef ROUTE_PATHRESOLVER_HPP
#define ROUTE_PATHRESOLVER_HPP

#include <string>

/**
 * @brief Turns the percent-encoded path of a request into a normalized one.
 *        요청의 퍼센트 인코딩된 경로를 정규화한다.
 *
 * Follows concepts/security "경로 정규화 절차": split into segments,
 * decode each once, merge repeated "/", resolve "." and "..". A trailing
 * "/" is kept, and a path ending in "." or ".." is treated as a directory,
 * as in RFC 3986 remove_dot_segments.
 * concepts/security "경로 정규화 절차"를 따른다: 조각으로 나누고, 조각마다
 * 한 번만 디코딩하고, 반복된 "/"를 합치고, "."과 ".."을 처리한다. 끝의
 * "/"는 남기고, "."이나 ".."으로 끝나는 경로는 RFC 3986
 * remove_dot_segments처럼 디렉토리로 본다.
 *
 * @param[in]  encodedPath Request path before decoding. / 디코딩 전 요청 경로.
 * @param[out] normalized  Decoded path starting with "/".
 *                         "/"로 시작하는 디코딩된 경로.
 * @return False, meaning 400, for a bad or NUL percent-encoding, an encoded
 *         "/" (design decision 3), or ".." above the root (design
 *         decision 2).
 *         잘못된 퍼센트 인코딩이나 NUL, 인코딩된 "/" (설계 결정 3), root
 *         위로 가는 ".." (설계 결정 2)이면 false. 400을 뜻한다.
 */
bool normalizePath(const std::string& encodedPath, std::string& normalized);

/**
 * @brief Percent-encodes a decoded path for use in a URI (Location, links).
 *        디코딩된 경로를 URI(Location, 링크)에 쓰도록 퍼센트 인코딩한다.
 *
 * Unreserved characters, sub-delims, ":", "@" and "/" stay as they are.
 * unreserved 문자, sub-delims, ":", "@", "/"는 그대로 둔다.
 *
 * @param[in] decodedPath Path to encode. / 인코딩할 경로.
 * @return The encoded path. / 인코딩한 경로.
 */
std::string encodePath(const std::string& decodedPath);

#endif
