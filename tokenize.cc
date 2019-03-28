#include "tokenize.h"

#include <unordered_map>
#include <unordered_set>

#include "text.h"

namespace ppt {

bool IsCharacterLiteralEncodingPrefix(const std::string& s) {
  static std::unordered_set<std::string> S = {"u", "U", "L"};

  return S.count(s);
}

bool IsStringLiteralEncodingPrefix(const std::string& s) {
  static std::unordered_set<std::string> S = {"u8", "u", "U", "L"};

  return S.count(s);
}

bool IsRawStringLiteralPrefix(const std::string& s) {
  static std::unordered_set<std::string> S = {"u8R", "uR", "UR", "LR", "R"};

  return S.count(s);
}

bool IsDigraphKeyword(const std::string& s) {
  static std::unordered_set<std::string> S = {
      "new", "delete", "and", "and_eq", "bitand", "bitor", "compl",
      "not", "not_eq", "or",  "or_eq",  "xor",    "xor_eq"};

  return S.count(s);
}

bool IsSimpleEscapeChar(int c) {
  static std::unordered_set<int> simple_escape_chars = {
      '\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'};

  return simple_escape_chars.count(c);
}

struct UniversalCharacterNameDecoder {
  std::vector<int> decode(int input) {
    switch (state) {
      case 0:
        if (input == '\\') {
          acc.clear();
          acc.push_back(input);
          state = 1;
          return {};
        } else
          return {input};

      case 1:
        acc.push_back(input);

        if (input == 'u') {
          digits = 4;
          state = 2;
          code_point = 0;
          return {};
        } else if (input == 'U') {
          digits = 8;
          state = 2;
          code_point = 0;
          return {};
        } else if (input == '\\') {
          state = 1;
          acc.clear();
          acc.push_back('\\');
          return {'\\'};
        } else {
          state = 0;
          return {'\\', input};
        }

      case 2:
        acc.push_back(input);

        if (!IsHexDigit(input)) {
          state = 0;
          return acc;
        } else {
          code_point = (code_point << 4) + HexCharToValue(input);
          digits--;

          if (digits == 0) {
            state = 0;

            return {code_point};
          } else
            return {};
        }

      default:
        throw std::logic_error("invalid state");
    }
  }

 private:
  int digits = 0;
  int state = 0;
  int code_point = 0;

  std::vector<int> acc;
};

struct TrigraphDecoder {
  std::vector<int> decode(int input) {
    static std::unordered_map<int, int> trigraphs = {
        {'=', '#'}, {'/', '\\'}, {'\'', '^'}, {'(', '['}, {')', ']'},
        {'!', '|'}, {'<', '{'},  {'>', '}'},  {'-', '~'}};

    switch (state) {
      case 0:
        if (input == '?') {
          state = 1;
          return {};
        } else
          return {input};

      case 1:
        if (input == '?') {
          state = 2;
          return {};
        } else {
          state = 0;
          return {'?', input};
        }

      case 2:
        if (input == '?') {
          state = 2;
          return {'?'};
        } else {
          state = 0;

          auto it = trigraphs.find(input);
          if (it == trigraphs.end()) {
            return {'?', '?', input};
          } else
            return {it->second};
        }

      default:
        throw std::logic_error("invalid state");
    }
  }

 private:
  int state = 0;
};

struct LineSplicer {
  std::vector<int> decode(int input) {
    switch (state) {
      case 0:
        if (input == '\\') {
          state = 1;
          return {};
        } else
          return {input};

      case 1:
        if (input == '\n') {
          state = 0;
          return {};
        } else if (input == '\r') {
          state = 2;
          return {};
        } else if (input == '\\')
          return {'\\'};
        else {
          state = 0;
          return {'\\', input};
        }

      case 2:
        if (input == '\n') {
          state = 0;
          return {};
        } else {
          state = 0;
          return {'\\', '\r', input};
        }

      default:
        throw std::logic_error("invalid state");
    }
  }

 private:
  int state = 0;
};

struct NewlineEnder {
  std::vector<int> decode(int input) {
    switch (state) {
      case 0:
        if (input == -1)
          return {-1};
        else if (input == '\n') {
          state = 2;
          return {input};
        } else {
          state = 1;
          return {input};
        }

      case 1:
        if (input == '\n') {
          state = 2;
          return {input};
        } else if (input == -1) {
          return {'\n', -1};
        } else
          return {input};

      case 2:
        if (input == '\n')
          return {input};
        else if (input == -1) {
          return {-1};
        } else {
          state = 1;
          return {input};
        }

      default:
        throw std::logic_error("invalid state");
    }
  }

 private:
  int state = 0;
};

struct Tokenizer {
  uint32_t rawpos;

  TokenStream& output;

  int header_name_state = 1;

  void emit_whitespace_sequence() { output.emit_whitespace_sequence(); }

  void emit_new_line() {
    header_name_state = 1;

    output.emit_new_line();
  }

  void emit_header_name(const std::string& data) {
    header_name_state = 0;

    output.emit_header_name(data);
  }

  void emit_identifier(const std::string& data) {
    if (header_name_state == 2 && data == "include")
      header_name_state = 3;
    else
      header_name_state = 0;

    output.emit_identifier(data);
  }

  void emit_pp_number(const std::string& data) {
    header_name_state = 0;

    output.emit_pp_number(data);
  }

  void emit_character_literal(const std::string& data) {
    header_name_state = 0;

    output.emit_character_literal(data);
  }

  void emit_user_defined_character_literal(const std::string& data) {
    header_name_state = 0;

    output.emit_user_defined_character_literal(data);
  }

  void emit_string_literal(const std::string& data) {
    header_name_state = 0;

    output.emit_string_literal(data);
  }

  void emit_user_defined_string_literal(const std::string& data) {
    header_name_state = 0;

    output.emit_user_defined_string_literal(data);
  }

  void emit_preprocessing_op_or_punc(const std::string& data) {
    if (header_name_state == 1 && (data == "#" || data == "%:"))
      header_name_state = 2;
    else
      header_name_state = 0;

    output.emit_preprocessing_op_or_punc(data);
  }

  void emit_non_whitespace_char(const std::string& data) {
    header_name_state = 0;

    output.emit_non_whitespace_char(data);
  }

  void emit_eof() {
    header_name_state = 0;

    output.emit_eof();
  }

  Tokenizer(TokenStream& output) : output(output) {}

  enum states {
    start,
    equals,
    colon,
    hash,
    langle,
    langle2,
    langle_colon,
    langle_colon2,
    rangle,
    rangle2,
    percent,
    percent_colon,
    percent_colon_percent,
    asterisk,
    plus,
    dash,
    dash_rangle,
    hat,
    ampersand,
    bar,
    exclamation,
    dot,
    dot2,
    pp_number,
    pp_number_e,
    identifier,
    whitespace,
    forward_slash,
    whitespace_forward_slash,
    inline_comment,
    inline_comment_ending,
    single_line_comment,
    character_literal,
    character_literal_backslash,
    character_literal_hex,
    character_literal_suffix,
    user_defined_character_literal,
    string_literal,
    string_literal_backslash,
    string_literal_hex,
    string_literal_suffix,
    user_defined_string_literal,
    raw_string_literal,
    raw_string_body,
    header_name_h,
    header_name_q,
    done
  };

  int state = start;

  bool raw_mode = false;
  Utf8Decoder utf8_decoder;
  UniversalCharacterNameDecoder ucn_decoder;
  TrigraphDecoder trigraph_decoder;
  LineSplicer line_splicer;
  NewlineEnder line_ender;

  std::string filename = "<stdin>";
  size_t linenum = 1;

  void process(int c0) {
    if (c0 == '\n') {
      output.emit_raw_newline(rawpos);
      linenum++;
    }

    if (!raw_mode) {
      for (int c1 : utf8_decoder.decode(c0))
        for (int c2 : trigraph_decoder.decode(c1))
          for (int c3 : ucn_decoder.decode(c2))
            for (int c4 : line_splicer.decode(c3))
              for (int c5 : line_ender.decode(c4)) {
                lookahead = c5;
                next_state();
              }
    } else {
      for (int c1 : utf8_decoder.decode(c0)) {
        lookahead = c1;
        next_state();
      }
    }
  }

  int lookahead;
  std::vector<int> accumulator;

  std::string accumulator_utf8() { return EncodeUtf8(accumulator); }

  struct action_t {};

  // add lookahead to accumulator, wait for next character
  action_t keep_wait(states s) {
    accumulator.push_back(lookahead);
    state = s;
    return action_t();
  }

  // discard accumulator and lookahead, wait for next character
  action_t clear_wait(states s) {
    accumulator.clear();
    state = s;
    return action_t();
  }

  // goto state s, do not touch accumulator or lookahead
  action_t keep_redirect(states s) {
    state = s;
    return next_state();
  }

  // discard accumulator, goto state s
  action_t clear_redirect(states s) {
    accumulator.clear();
    state = s;
    return next_state();
  }

  std::vector<int> raw_string_delim;
  size_t raw_string_match;

  action_t next_state() {
    switch (state) {
      case start:
        if (IsDigit(lookahead)) return keep_wait(pp_number);

        if (IsAllowedIdentifierFirstCharacter(lookahead))
          return keep_wait(identifier);

        switch (lookahead) {
          case ' ':
          case '\t':
          case '\v':
          case '\f':
          case '\r':
            return keep_wait(whitespace);

          case '\n':
            emit_new_line();
            return clear_wait(start);

          case '"':
            if (header_name_state == 3)
              return keep_wait(header_name_q);
            else
              return keep_wait(string_literal);

          case '\'':
            return keep_wait(character_literal);

          case '/':
            return keep_wait(forward_slash);

          case '.':
            return keep_wait(dot);

          case '{':
          case '}':
          case '[':
          case ']':
          case '(':
          case ')':
          case ';':
          case '?':
          case ',':
          case '~':
            emit_preprocessing_op_or_punc(std::string(1, char(lookahead)));
            return clear_wait(start);

          case '=':
            return keep_wait(equals);

          case ':':
            return keep_wait(colon);

          case '#':
            return keep_wait(hash);

          case '<':
            if (header_name_state == 3)
              return keep_wait(header_name_h);
            else
              return keep_wait(langle);

          case '>':
            return keep_wait(rangle);

          case '%':
            return keep_wait(percent);

          case '*':
            return keep_wait(asterisk);

          case '+':
            return keep_wait(plus);

          case '-':
            return keep_wait(dash);

          case '^':
            return keep_wait(hat);

          case '&':
            return keep_wait(ampersand);

          case '|':
            return keep_wait(bar);

          case '!':
            return keep_wait(exclamation);

          case -1:
            emit_eof();
            return clear_wait(done);

          default:
            emit_non_whitespace_char(EncodeUtf8({lookahead}));
            return clear_wait(start);
        }

      case equals:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("==");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("=");
            return clear_redirect(start);
        }

      case colon:
        switch (lookahead) {
          case '>':
            emit_preprocessing_op_or_punc(":>");
            return clear_wait(start);

          case ':':
            emit_preprocessing_op_or_punc("::");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc(":");
            return clear_redirect(start);
        }

      case hash:
        switch (lookahead) {
          case '#':
            emit_preprocessing_op_or_punc("##");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("#");
            return clear_redirect(start);
        }

      case langle:
        switch (lookahead) {
          case '<':
            return keep_wait(langle2);

          case ':':
            return keep_wait(langle_colon);

          case '%':
            emit_preprocessing_op_or_punc("<%");
            return clear_wait(start);

          case '=':
            emit_preprocessing_op_or_punc("<=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("<");
            return clear_redirect(start);
        }

      case langle2:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("<<=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("<<");
            return clear_redirect(start);
        }

      case langle_colon:
        switch (lookahead) {
          case ':':
            return keep_wait(langle_colon2);

          default:
            emit_preprocessing_op_or_punc("<:");
            return clear_redirect(start);
        }

      case langle_colon2:
        switch (lookahead) {
          case ':':
            emit_preprocessing_op_or_punc("<:");
            emit_preprocessing_op_or_punc("::");
            return clear_wait(start);

          case '>':
            emit_preprocessing_op_or_punc("<:");
            emit_preprocessing_op_or_punc(":>");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("<");
            emit_preprocessing_op_or_punc("::");
            return clear_redirect(start);
        }

      case rangle:
        switch (lookahead) {
          case '>':
            return keep_wait(rangle2);

          case '=':
            emit_preprocessing_op_or_punc(">=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc(">");
            return clear_redirect(start);
        }

      case rangle2:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc(">>=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc(">>");
            return clear_redirect(start);
        }

      case percent:
        switch (lookahead) {
          case '>':
            emit_preprocessing_op_or_punc("%>");
            return clear_wait(start);

          case ':':
            return keep_wait(percent_colon);

          case '=':
            emit_preprocessing_op_or_punc("%=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("%");
            return clear_redirect(start);
        }

      case percent_colon:
        switch (lookahead) {
          case '%':
            return keep_wait(percent_colon_percent);

          default:
            emit_preprocessing_op_or_punc("%:");
            return clear_redirect(start);
        }

      case percent_colon_percent:
        switch (lookahead) {
          case '>':
            emit_preprocessing_op_or_punc("%:");
            emit_preprocessing_op_or_punc("%>");
            return clear_wait(start);

          case ':':
            emit_preprocessing_op_or_punc("%:%:");
            return clear_wait(start);

          case '=':
            emit_preprocessing_op_or_punc("%:");
            emit_preprocessing_op_or_punc("%=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("%:");
            emit_preprocessing_op_or_punc("%");
            return clear_redirect(start);
        }

      case asterisk:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("*=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("*");
            return clear_redirect(start);
        }

      case plus:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("+=");
            return clear_wait(start);

          case '+':
            emit_preprocessing_op_or_punc("++");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("+");
            return clear_redirect(start);
        }

      case dash:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("-=");
            return clear_wait(start);

          case '-':
            emit_preprocessing_op_or_punc("--");
            return clear_wait(start);

          case '>':
            return keep_wait(dash_rangle);

          default:
            emit_preprocessing_op_or_punc("-");
            return clear_redirect(start);
        }

      case dash_rangle:
        switch (lookahead) {
          case '*':
            emit_preprocessing_op_or_punc("->*");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("->");
            return clear_redirect(start);
        }

      case hat:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("^=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("^");
            return clear_redirect(start);
        }

      case ampersand:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("&=");
            return clear_wait(start);

          case '&':
            emit_preprocessing_op_or_punc("&&");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("&");
            return clear_redirect(start);
        }

      case bar:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("|=");
            return clear_wait(start);

          case '|':
            emit_preprocessing_op_or_punc("||");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("|");
            return clear_redirect(start);
        }

      case exclamation:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("!=");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc("!");
            return clear_redirect(start);
        }

      case dot:
        if (IsDigit(lookahead)) return keep_wait(pp_number);

        switch (lookahead) {
          case '.':
            return keep_wait(dot2);

          case '*':
            emit_preprocessing_op_or_punc(".*");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc(".");
            return clear_redirect(start);
        }

      case dot2:
        if (IsDigit(lookahead)) {
          emit_preprocessing_op_or_punc(".");
          accumulator.clear();
          accumulator.push_back('.');
          return keep_wait(pp_number);
        }

        switch (lookahead) {
          case '.':
            emit_preprocessing_op_or_punc("...");
            return clear_wait(start);

          case '*':
            emit_preprocessing_op_or_punc(".");
            emit_preprocessing_op_or_punc(".*");
            return clear_wait(start);

          default:
            emit_preprocessing_op_or_punc(".");
            emit_preprocessing_op_or_punc(".");
            return clear_redirect(start);
        }

      case pp_number:
        if (lookahead == 'E' || lookahead == 'e')
          return keep_wait(pp_number_e);
        else if (IsAllowedIdentifierBodyCharacter(lookahead) ||
                 lookahead == '.')
          return keep_wait(pp_number);
        else {
          emit_pp_number(accumulator_utf8());
          return clear_redirect(start);
        }

      case pp_number_e:
        if (lookahead == '+' || lookahead == '-')
          return keep_wait(pp_number);
        else
          return keep_redirect(pp_number);

      case identifier:
        if (IsAllowedIdentifierBodyCharacter(lookahead))
          return keep_wait(identifier);
        else if (lookahead == '\'' &&
                 IsCharacterLiteralEncodingPrefix(accumulator_utf8()))
          return keep_wait(character_literal);
        else if (lookahead == '"' &&
                 IsStringLiteralEncodingPrefix(accumulator_utf8()))
          return keep_wait(string_literal);
        else if (lookahead == '"' &&
                 IsRawStringLiteralPrefix(accumulator_utf8())) {
          raw_mode = true;
          raw_string_delim.clear();
          raw_string_delim.push_back(')');
          return keep_wait(raw_string_literal);
        } else if (IsDigraphKeyword(accumulator_utf8())) {
          emit_preprocessing_op_or_punc(accumulator_utf8());
          return clear_redirect(start);
        } else {
          emit_identifier(accumulator_utf8());
          return clear_redirect(start);
        }

      case whitespace:
        if (IsSpace(lookahead) && lookahead != '\n')
          return keep_wait(whitespace);
        else if (lookahead == '/')
          return keep_wait(whitespace_forward_slash);
        else {
          emit_whitespace_sequence();
          return clear_redirect(start);
        }

      case forward_slash:
        switch (lookahead) {
          case '=':
            emit_preprocessing_op_or_punc("/=");
            return clear_wait(start);

          case '*':
            return keep_wait(inline_comment);

          case '/':
            return keep_wait(single_line_comment);

          default:
            emit_preprocessing_op_or_punc("/");
            return clear_redirect(start);
        }

      case whitespace_forward_slash:
        switch (lookahead) {
          case '=':
            emit_whitespace_sequence();
            emit_preprocessing_op_or_punc("/=");
            return clear_wait(start);

          case '*':
            return keep_wait(inline_comment);

          case '/':
            return keep_wait(single_line_comment);

          default:
            emit_whitespace_sequence();
            emit_preprocessing_op_or_punc("/");
            return clear_redirect(start);
        }

      case inline_comment:
        if (lookahead == '*')
          return keep_wait(inline_comment_ending);
        else if (lookahead == -1)
          throw std::logic_error("partial comment");
        else
          return keep_wait(inline_comment);

      case inline_comment_ending:
        if (lookahead == '*')
          return keep_wait(inline_comment_ending);
        else if (lookahead == '/')
          return keep_wait(whitespace);
        else
          return keep_wait(inline_comment);

      case single_line_comment:
        if (lookahead == '\n') {
          emit_whitespace_sequence();
          return clear_redirect(start);
        } else
          return keep_wait(single_line_comment);

      case character_literal:
        if (lookahead == '\'')
          return keep_wait(character_literal_suffix);
        else if (lookahead == '\\')
          return keep_wait(character_literal_backslash);
        else if (lookahead == '\n' || lookahead == -1)
          throw std::logic_error("unterminated character literal");
        else
          return keep_wait(character_literal);

      case character_literal_backslash:
        if (IsSimpleEscapeChar(lookahead) ||
            (lookahead >= '0' && lookahead <= '7'))
          return keep_wait(character_literal);
        else if (lookahead == 'x')
          return keep_wait(character_literal_hex);
        else
          throw std::logic_error("invalid escape sequence");

      case character_literal_hex:
        if (IsHexDigit(lookahead))
          return keep_wait(character_literal);
        else
          throw std::logic_error("invalid hex escape sequence");

      case character_literal_suffix:
        if (IsAllowedIdentifierFirstCharacter(lookahead))
          return keep_wait(user_defined_character_literal);
        else {
          emit_character_literal(accumulator_utf8());
          return clear_redirect(start);
        }

      case user_defined_character_literal:
        if (IsAllowedIdentifierBodyCharacter(lookahead))
          return keep_wait(user_defined_character_literal);
        else {
          emit_user_defined_character_literal(accumulator_utf8());
          return clear_redirect(start);
        }

      case string_literal:
        if (lookahead == '"')
          return keep_wait(string_literal_suffix);
        else if (lookahead == '\\')
          return keep_wait(string_literal_backslash);
        else if (lookahead == '\n' || lookahead == -1)
          throw std::logic_error("unterminated string literal");
        else
          return keep_wait(string_literal);

      case string_literal_backslash:
        if (IsSimpleEscapeChar(lookahead) ||
            (lookahead >= '0' && lookahead <= '7'))
          return keep_wait(string_literal);
        else if (lookahead == 'x')
          return keep_wait(string_literal_hex);
        else
          throw std::logic_error("invalid escape sequence");

      case string_literal_hex:
        if (IsHexDigit(lookahead))
          return keep_wait(string_literal);
        else
          throw std::logic_error("invalid hex escape sequence");

      case string_literal_suffix:
        if (IsAllowedIdentifierFirstCharacter(lookahead))
          return keep_wait(user_defined_string_literal);
        else {
          emit_string_literal(accumulator_utf8());
          return clear_redirect(start);
        }

      case user_defined_string_literal:
        if (IsAllowedIdentifierBodyCharacter(lookahead))
          return keep_wait(user_defined_string_literal);
        else {
          emit_user_defined_string_literal(accumulator_utf8());
          return clear_redirect(start);
        }

      case raw_string_literal:
        if (lookahead == '(') {
          raw_string_delim.push_back('"');
          raw_string_match = 0;

          if (raw_string_delim.size() > 18)
            throw std::logic_error("raw string delimiter too long");

          return keep_wait(raw_string_body);
        } else if (lookahead == ')' || lookahead == '\\' || IsSpace(lookahead))
          throw std::logic_error("invalid characters in raw string delimiter");
        else if (lookahead == -1)
          throw std::logic_error("unterminated raw string literal");
        else {
          raw_string_delim.push_back(lookahead);
          return keep_wait(raw_string_literal);
        }

      case raw_string_body:
        if (lookahead == -1)
          throw std::logic_error("unterminated raw string literal");
        else if (lookahead == raw_string_delim[raw_string_match]) {
          raw_string_match++;

          if (raw_string_match == raw_string_delim.size()) {
            raw_mode = false;
            return keep_wait(string_literal_suffix);
          } else
            return keep_wait(raw_string_body);
        } else if (lookahead == raw_string_delim[0]) {
          raw_string_match = 1;
          return keep_wait(raw_string_body);
        } else {
          raw_string_match = 0;
          return keep_wait(raw_string_body);
        }

      case header_name_h:
        switch (lookahead) {
          case '>':
            accumulator.push_back('>');
            emit_header_name(accumulator_utf8());
            return clear_wait(start);

          case -1:
          case '\n':
            throw std::logic_error("unterminated header name");

          default:
            return keep_wait(header_name_h);
        }

      case header_name_q:
        switch (lookahead) {
          case '"':
            accumulator.push_back('"');
            emit_header_name(accumulator_utf8());
            return clear_wait(start);

          case -1:
          case '\n':
            throw std::logic_error("unterminated header name");

          default:
            return keep_wait(header_name_q);
        }

      case done:
        throw std::logic_error("already done");

      default:
        throw std::logic_error("unknown state");
    }
  }
};

void Tokenize(const std::string& input, TokenStream& output) {
  Tokenizer tokenizer(output);

  for (tokenizer.rawpos = 0; tokenizer.rawpos < input.size();
       tokenizer.rawpos++) {
    unsigned char code_unit = input[tokenizer.rawpos];

    tokenizer.process(code_unit);
  }

  tokenizer.process(-1);
}

}  // namespace ppt
