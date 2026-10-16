/**
 * @file  ConfigLexer.hpp
 * @brief First stage of configuration loading: text to tokens.
 *        설정 읽기의 첫 단계: 텍스트를 토큰으로 나눈다.
 */
#ifndef CONFIG_CONFIGLEXER_HPP
#define CONFIG_CONFIGLEXER_HPP

#include <string>
#include <vector>

/**
 * @brief Kinds of configuration tokens.
 *        설정 토큰의 종류.
 */
enum TokenType {
  TOKEN_WORD,        /**< Name or argument. / 이름이나 인자. */
  TOKEN_OPEN_BRACE,  /**< "{" */
  TOKEN_CLOSE_BRACE, /**< "}" */
  TOKEN_SEMICOLON,   /**< ";" */
  TOKEN_END          /**< End of input. / 입력의 끝. */
};

/**
 * @brief One token with the line it came from.
 *        토큰 하나와 그 토큰이 나온 줄.
 */
struct Token {
  /** Kind. / 종류. */
  TokenType type;
  /** Text as written. / 쓰인 그대로의 텍스트. */
  std::string text;
  /** 1-based line. / 1부터 세는 줄 번호. */
  int line;
};

/**
 * @brief Splits NGINX-style configuration text into tokens.
 *        NGINX 형식의 설정 텍스트를 토큰으로 나눈다.
 *
 * "#" starts a comment that runs to the end of the line. Quoted strings
 * are not supported.
 * "#"부터 줄 끝까지는 주석이다. 따옴표 문자열은 지원하지 않는다.
 *
 * @param[in] text   Configuration text. / 설정 텍스트.
 * @param[in] source Name used in error messages.
 *                   오류 메시지에 쓸 이름.
 * @return The tokens, always ending with TOKEN_END.
 *         토큰 목록. 항상 TOKEN_END로 끝난다.
 * @throws ConfigError On a quote character. / 따옴표 문자를 만나면.
 */
std::vector<Token> tokenizeConfig(const std::string& text,
                                  const std::string& source);

#endif
