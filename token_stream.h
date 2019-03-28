#pragma once

#include <string>

namespace ppt {

struct TokenStream {
  virtual void emit_whitespace_sequence() = 0;
  virtual void emit_new_line() = 0;
  virtual void emit_header_name(const std::string& spelling) = 0;
  virtual void emit_identifier(const std::string& spelling) = 0;
  virtual void emit_pp_number(const std::string& spelling) = 0;
  virtual void emit_character_literal(const std::string& spelling) = 0;
  virtual void emit_user_defined_character_literal(
      const std::string& spelling) = 0;
  virtual void emit_string_literal(const std::string& spelling) = 0;
  virtual void emit_user_defined_string_literal(
      const std::string& spelling) = 0;
  virtual void emit_preprocessing_op_or_punc(const std::string& spelling) = 0;
  virtual void emit_non_whitespace_char(const std::string& spelling) = 0;
  virtual void emit_raw_newline(uint32_t pos) = 0;
  virtual void emit_eof() = 0;

  virtual ~TokenStream() {}
};

}  // namespace ppt
