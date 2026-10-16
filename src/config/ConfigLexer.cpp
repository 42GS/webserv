#include "config/ConfigLexer.hpp"

#include "config/ConfigError.hpp"

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool isWordChar(char c) {
  return !isSpace(c) && c != '{' && c != '}' && c != ';' && c != '#';
}

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
