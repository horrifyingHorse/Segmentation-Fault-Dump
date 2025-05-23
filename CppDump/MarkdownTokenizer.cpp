#include <cmath>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class TokenType {
  NONE = 0,
  H1 = 1,
  H2 = 2,
  H3 = 3,
  H4 = 4,
  H5 = 5,
  H6 = 6,
  TEXT,
  BOLD,
  ITALIC,
  BOLD_ITALIC,
  CODE,
  CODEBLOCK,
};

const std::string TokenStr(const TokenType& token) {
  // clang-format off
  switch (token) {
    case TokenType::NONE: { return std::move("Token::NONE"); }
    case TokenType::H1: { return std::move("Token::H1"); }
    case TokenType::H2: { return std::move("Token::H2"); }
    case TokenType::H3: { return std::move("Token::H3"); }
    case TokenType::H4: { return std::move("Token::H4"); }
    case TokenType::H5: { return std::move("Token::H5"); }
    case TokenType::H6: { return std::move("Token::H6"); }
    case TokenType::TEXT: { return std::move("Token::TEXT"); }
    case TokenType::BOLD: { return std::move("Token::BOLD"); }
    case TokenType::ITALIC: { return std::move("Token::ITALIC"); }
    case TokenType::BOLD_ITALIC: { return std::move("Token::BOLD_ITALIC"); }
    case TokenType::CODE: { return std::move("Token::CODE"); }
    case TokenType::CODEBLOCK: { return std::move("Token::CODEBLOCK"); }
  }
  // clang-format on
}

typedef std::pair<TokenType, std::string> Token;
typedef std::vector<Token> Tokens;

class Tokenizer {
 public:
  Tokenizer() { Tokenizer::populateMarkerMap(); }

  void tokenizer(const std::string& line) {
    begin = line.begin();
    int count = 1;
    for (it = line.begin(); it != line.end(); ++it) {
      if (*it == '*') {
        count = lookAhead(line, '*');
        if (count == 1) {
          TokenUtil(TokenType::ITALIC, "*");
        } else if (count == 2) {
          TokenUtil(TokenType::BOLD, "**");
        } else if (count == 3) {
          TokenUtil(TokenType::BOLD_ITALIC, "***");
        } else {
          TokenUtil(TokenType::NONE, StrCreat("*", count));
        }
      }
      if (*it == '_') {
        count = lookAhead(line, '_');
        if (count == 1) {
          TokenUtil(TokenType::ITALIC, "_");
        } else if (count == 2) {
          TokenUtil(TokenType::BOLD, "__");
        } else if (count == 3) {
          TokenUtil(TokenType::BOLD_ITALIC, "___");
        } else {
          TokenUtil(TokenType::NONE, StrCreat("_", count));
        }
      }

      if (updateBegin) {
        begin = it + count;
        it = it + --count;
        updateBegin = false;
      }
    }
    PushText();
    FormatCorrections();
  }

  void debug() {
    for (auto& token : tokens) {
      std::cout << TokenStr(token.first) << "\t\t" << token.second << "\n";
    }
  }

 private:
  std::string::const_iterator begin, it;
  bool updateBegin = false;
  bool followsWhiteSpace = false;
  bool renderBlank = false;
  typedef struct Stack {
    std::string marker;
    // metadata
    int index = 0;
    bool toErase = true;
  } Stack;
  std::deque<Stack> syntaxStack;
  inline static std::unordered_map<std::string, TokenType> markerMap;

  Tokens tokens;

  inline static void populateMarkerMap() {
    Tokenizer::markerMap["*"] = TokenType::ITALIC;
    Tokenizer::markerMap["**"] = TokenType::BOLD;
    Tokenizer::markerMap["***"] = TokenType::BOLD_ITALIC;
    Tokenizer::markerMap["_"] = TokenType::ITALIC;
    Tokenizer::markerMap["__"] = TokenType::BOLD;
    Tokenizer::markerMap["___"] = TokenType::BOLD_ITALIC;
  }

  std::string StrCreat(const std::string& s, int n) {
    std::stringstream ss;
    while (n--) {
      ss << s;
    }
    return ss.str();
  }

  void ClearStack() {
    while (!syntaxStack.empty()) {
      syntaxStack.pop_back();
    }
  }

  bool ToPop(const Token& token) {
    std::cout << syntaxStack.front().marker << " : " << token.second << "\n";
    return (!syntaxStack.empty() && syntaxStack.front().marker == token.second);
  }

  void FormatCorrections(std::deque<Stack> syntaxStack, int& correction) {
    std::deque<Stack> backupStack;
    while (!syntaxStack.empty() && syntaxStack.size() >= 2) {
      backupStack.clear();
      Stack TOS = syntaxStack.back();
      syntaxStack.pop_back();

      Stack TOSm1 = syntaxStack.back();
      syntaxStack.pop_back();
      if (TOS.marker[0] != TOSm1.marker[0]) {
        while (syntaxStack.size() >= 1 && TOS.marker[0] != TOSm1.marker[0]) {
          std::cout << TOS.marker << TOSm1.marker << "\n";
          backupStack.push_back(TOSm1);
          std::cout << "PUSHED: " << TOSm1.marker << "\n";
          TOSm1 = syntaxStack.back();
          syntaxStack.pop_back();
        }
        if (syntaxStack.size() == 0 && TOS.marker[0] != TOSm1.marker[0]) {
          syntaxStack.push_back(TOSm1);
          syntaxStack.push_back(TOS);
          break;
        }
        std::cout << "Broken!!\n";
      }

      if (TOS.marker.length() == TOSm1.marker.length()) {
        if (!TOS.toErase) {
          tokens.insert(tokens.begin() + TOS.index + correction++,
                        {Tokenizer::markerMap[TOS.marker], TOS.marker});
        }
        if (backupStack.size() > 0) {
          FormatCorrections(backupStack, correction);
        }
      } else if (TOS.marker.length() > TOSm1.marker.length()) {
        TOS.marker =
            TOS.marker.substr(0, TOS.marker.length() - TOSm1.marker.length());
        syntaxStack.push_back(std::move(TOS));

        if (syntaxStack.back().toErase) {
          tokens.erase(tokens.begin() + syntaxStack.back().index +
                       correction--);
          syntaxStack.back().toErase = false;
        }
        tokens.insert(tokens.begin() + syntaxStack.back().index + ++correction,
                      {Tokenizer::markerMap[TOSm1.marker], TOSm1.marker});
        if (backupStack.size() > 0) {
          FormatCorrections(backupStack, correction);
        }
      } else {
        TOSm1.marker =
            TOSm1.marker.substr(0, TOSm1.marker.length() - TOS.marker.length());
        syntaxStack.push_back(std::move(TOSm1));

        if (!TOS.toErase) {
          tokens.insert(tokens.begin() + TOS.index + correction++,
                        {Tokenizer::markerMap[TOS.marker], TOS.marker});
        }
        if (syntaxStack.back().toErase) {
          tokens.erase(tokens.begin() + syntaxStack.back().index +
                       correction--);
          syntaxStack.back().toErase = false;
        }
        tokens.insert(tokens.begin() + syntaxStack.back().index + ++correction,
                      {Tokenizer::markerMap[TOS.marker], TOS.marker});
        if (backupStack.size() > 0) {
          FormatCorrections(backupStack, correction);
        }
      }
    }
    while (!syntaxStack.empty()) {
      tokens.insert(tokens.begin() + syntaxStack.back().index + ++correction,
                    {TokenType::TEXT, syntaxStack.back().marker});
      if (syntaxStack.back().toErase) {
        tokens.erase(tokens.begin() + syntaxStack.back().index + --correction);
      }
      syntaxStack.pop_back();
    }
  }

  void FormatCorrections() {
    int i = -1;
    int correction = 0;
    std::deque<Stack> syntaxStack;
    for (const auto& token : tokens) {
      i++;
      if ((token.first < (TokenType)6 && token.first != (TokenType)0) ||
          token.first == TokenType::TEXT) {
        continue;
      }
      if (!ToPop(token)) {
        syntaxStack.push_front({token.second, i, true});
      } else {
        syntaxStack.pop_front();
      }
    }
    if (syntaxStack.empty()) {
      return;
    }

    FormatCorrections(syntaxStack, correction);
  }

  void PushText() {
    std::string text = (begin >= it) ? "" : std::string(begin, it);
    if (!text.empty()) {
      tokens.push_back({TokenType::TEXT, text});
    }
  }

  void TokenUtil(const TokenType& token, const std::string& marker) {
    updateBegin = true;
    PushText();
    tokens.push_back({token, marker});
  }

  int lookAhead(const std::string& line, char&& c) {
    int count = 1;
    while ((it + count) != line.end() && *(it + count) == c) {
      count++;
    }
    return count;
  }
};

int main() {
  Tokenizer t;
  // t.tokenizer("well, **this is *8ball* **\n");
  // t.tokenizer("this **ain't nothin*\n");
  // t.tokenizer("this *****ain't nothin*** this is cruel **\n");
  // t.tokenizer("this *is the *****ain't nothin** this is cruel **\n");
  // t.tokenizer("***So **it *works?* huh...**mornin*");
  // t.tokenizer("**how***bout*zis? where this text?");
  // t.tokenizer("**this should be**** entirely bold**");
  // t.tokenizer("__this should be____entirely bold__");
  // t.tokenizer("__this *should__ be** entirely bold__");
  t.tokenizer("__this *should be** entirely bold__");

  /* S T A C K
   *
   *
   * **
   * **
   *
   *
   */
  t.debug();
}
