#pragma once

#include <vector>

#include "token_stream.h"

namespace ppt {

enum TokenKind {
  OPERATOR = 0,
  IDENTIFIER = 1,
  NUMBER = 2,
  CHARACTER_LITERAL = 3,
  STRING_LITERAL = 4,
  HEADER_NAME = 5
};

struct Token {
  TokenKind kind;
  std::string spelling;
};

struct VectorTokenStream : TokenStream {
  void emit_whitespace_sequence() override {}

  void emit_new_line() override {}
  void emit_raw_newline(uint32_t pos) override { write_newline(pos); }

  void emit_header_name(const std::string& spelling) override {
    write_token(HEADER_NAME, spelling);
  }

  void emit_identifier(const std::string& spelling) override {
    write_token(IDENTIFIER, spelling);
  }

  void emit_pp_number(const std::string& spelling) override {
    write_token(NUMBER, spelling);
  }

  void emit_character_literal(const std::string& spelling) override {
    write_token(CHARACTER_LITERAL, spelling);
  }

  void emit_user_defined_character_literal(
      const std::string& spelling) override {
    write_token(CHARACTER_LITERAL, spelling);
  }

  void emit_string_literal(const std::string& spelling) override {
    write_token(STRING_LITERAL, spelling);
  }

  void emit_user_defined_string_literal(const std::string& spelling) override {
    write_token(STRING_LITERAL, spelling);
  }

  void emit_preprocessing_op_or_punc(const std::string& spelling) override {
    write_token(OPERATOR, spelling);
  }

  void emit_non_whitespace_char(const std::string& spelling) override {
    if (spelling.size() == 1 && !std::isprint(spelling[0]))
      throw std::runtime_error("non whitespace code = " +
                               std::to_string(int(spelling[0])));
    else
      throw std::runtime_error("non whitespace char '" + spelling + "'");
  }

  void emit_eof() override {}

  std::vector<Token> tokens;
  struct NewLine {
    uint32_t token_id;
    uint32_t file_offset;
  };
  std::vector<NewLine> newlines_tokens;

 private:
  void write_token(TokenKind token_kind, const std::string& data) {
    tokens.push_back(Token{token_kind, data});
  }

  void write_newline(uint32_t pos) {
    newlines_tokens.push_back(NewLine{uint32_t(tokens.size()), pos});
  }
};

}  // namespace ppt
