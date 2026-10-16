/**
 * @file  ConfigLexer.cpp
 * @brief Tokenizer of configuration text.
 *        설정 텍스트의 토큰 분리기.
 */
#include "config/ConfigLexer.hpp"

#include "config/ConfigError.hpp"

namespace {

/**
 * @brief True for the whitespace that separates tokens.
 *        토큰을 나누는 공백 문자이면 true.
 * @param[in] c Character. / 문자.
 * @return Whether c is a space, tab, CR or LF.
 *         c가 공백, 탭, CR, LF 중 하나인지.
 */
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/**
 * @brief True for a character that can be part of a word token.
 *        단어 토큰에 들어갈 수 있는 문자이면 true.
 * @param[in] c Character. / 문자.
 * @return False for whitespace, "{", "}", ";" and "#".
 *         공백, "{", "}", ";", "#"이면 false.
 */
bool isWordChar(char c) {
  return !isSpace(c) && c != '{' && c != '}' && c != ';' && c != '#';
}

/**
 * @brief Builds a token (C++98 has no brace initialization in an
 *        expression).
 *        토큰을 만든다 (C++98에서는 식 안에서 중괄호 초기화를 쓸 수 없다).
 * @param[in] type Kind. / 종류.
 * @param[in] text Text. / 텍스트.
 * @param[in] line Line. / 줄 번호.
 * @return The token. / 만든 토큰.
 */
Token makeToken(TokenType type, const std::string& text, int line) {
  Token token;
  token.type = type;
  token.text = text;
  token.line = line;
  return token;
}

}  // namespace

std::vector<Token> tokenizeConfig(const std::string& text,
                                  const std::string& source) {
  std::vector<Token> tokens;
  int line = 1;
  std::string::size_type i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '\n') {
      ++line;
      ++i;
    } else if (isSpace(c)) {
      ++i;
    } else if (c == '#') {
      while (i < text.size() && text[i] != '\n') ++i;
    } else if (c == '{') {
      tokens.push_back(makeToken(TOKEN_OPEN_BRACE, "{", line));
      ++i;
    } else if (c == '}') {
      tokens.push_back(makeToken(TOKEN_CLOSE_BRACE, "}", line));
      ++i;
    } else if (c == ';') {
      tokens.push_back(makeToken(TOKEN_SEMICOLON, ";", line));
      ++i;
    } else if (c == '"' || c == '\'') {
      throw ConfigError(source, line, "quoted strings are not supported");
    } else {
      const std::string::size_type start = i;
      while (i < text.size() && isWordChar(text[i]) && text[i] != '"' &&
             text[i] != '\'')
        ++i;
      tokens.push_back(
          makeToken(TOKEN_WORD, text.substr(start, i - start), line));
    }
  }
  tokens.push_back(makeToken(TOKEN_END, "", line));
  return tokens;
}
