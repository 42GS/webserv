#ifndef CONFIG_CONFIGLEXER_HPP
#define CONFIG_CONFIGLEXER_HPP

#include <string>
#include <vector>

enum TokenType {
    TOKEN_WORD,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_SEMICOLON,
    TOKEN_END
};

struct Token {
    TokenType   type;
    std::string text;
    int         line;
};

// Splits NGINX-style configuration text into tokens. "#" starts a comment
// that runs to the end of the line. Quoted strings are not supported.
// Throws ConfigError on a character it cannot accept.
std::vector<Token> tokenizeConfig(const std::string& text, const std::string& source);

#endif
