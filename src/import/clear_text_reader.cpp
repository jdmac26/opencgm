#include "opencgm/clear_text_reader.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/security_limits.h"
#include <cctype>
#include <sstream>

namespace opencgm {

namespace {
bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$';
}

bool isIdentifierChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$' || ch == '-';
}

bool isNumberStart(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch)) || ch == '+' || ch == '-' || ch == '.';
}
} // namespace

DefaultClearTextReader::DefaultClearTextReader(
    std::istream& stream,
    CGMFile* cgm,
    ICommandFactory* factory)
    : stream_(stream)
    , cgm_(cgm)
    , factory_(factory)
    , currentLine_(1)
    , reachedEOF_(false) {}

void DefaultClearTextReader::readStatements() {
    statements_.clear();
    reachedEOF_ = false;
    currentLine_ = 1;
    while (true) {
        Token token = nextToken();
        if (token.type == TokenType::EndOfFile) {
            break;
        }
        if (token.type == TokenType::Invalid) {
            if (cgm_) {
                cgm_->addMessage(
                    Message(Severity::Unsupported,
                            ClassCode::DelimiterElement,
                            -1,
                            "Invalid token in clear text stream: \"" + token.lexeme + "\"",
                            "CLEAR_TEXT"));
            }
            continue;
        }
        if (token.type != TokenType::Identifier) {
            if (cgm_) {
                cgm_->addMessage(
                    Message(Severity::Unsupported,
                            ClassCode::DelimiterElement,
                            -1,
                            "Expected command identifier but found \"" + token.lexeme + "\"",
                            "CLEAR_TEXT"));
            }
            continue;
        }
        Statement stmt;
        stmt.command = token.lexeme;
        stmt.line = token.line;

        bool terminated = false;
        while (!terminated) {
            Token next = nextToken();
            if (next.type == TokenType::EndOfFile) {
                terminated = true;
                if (cgm_) {
                    cgm_->addMessage(
                        Message(Severity::Unsupported,
                                ClassCode::DelimiterElement,
                                -1,
                                "Unexpected EOF while reading clear text statement: " + stmt.command,
                                "CLEAR_TEXT"));
                }
                break;
            }
            if (next.type == TokenType::Symbol && next.lexeme == ";") {
                terminated = true;
                break;
            }
            if (next.type == TokenType::Invalid) {
                if (cgm_) {
                    cgm_->addMessage(
                        Message(Severity::Unsupported,
                                ClassCode::DelimiterElement,
                                -1,
                                "Invalid token in statement: " + stmt.command,
                                "CLEAR_TEXT"));
                }
                continue;
            }

            // SECURITY: Limit token accumulation to prevent DoS attacks
            if (stmt.tokens.size() >= security::MAX_TOKENS_PER_STATEMENT) {
                throw std::runtime_error(
                    "CGM clear text statement exceeds maximum token limit: " +
                    std::to_string(security::MAX_TOKENS_PER_STATEMENT)
                );
            }

            stmt.tokens.push_back(next);
        }

        std::ostringstream payload;
        bool firstArg = true;
        for (const auto& arg : stmt.tokens) {
            if (!firstArg) {
                payload << ' ';
            }
            if (arg.type == TokenType::String) {
                payload << "'" << arg.lexeme << "'";
            } else {
                payload << arg.lexeme;
            }
            firstArg = false;
        }
        stmt.payload = payload.str();
        statements_.push_back(std::move(stmt));
    }
}

void DefaultClearTextReader::skipWhitespaceAndComments() {
    while (true) {
        int ch = stream_.peek();
        if (ch == EOF) {
            reachedEOF_ = true;
            return;
        }
        if (ch == '\n') {
            stream_.get();
            currentLine_++;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            stream_.get();
            continue;
        }
        // ISO 8632-4 uses "!" for comment to end of line
        if (ch == '!') {
            while (ch != '\n' && ch != EOF) {
                ch = stream_.get();
            }
            continue;
        }
        // Provide fallback for C-style comments
        if (ch == '/') {
            stream_.get();
            int next = stream_.peek();
            if (next == '/') {
                while (next != '\n' && next != EOF) {
                    stream_.get();
                    next = stream_.peek();
                }
                continue;
            }
            if (next == '*') {
                stream_.get(); // consume '*'
                int prev = 0;
                int cur = stream_.get();
                while (cur != EOF) {
                    if (cur == '\n') {
                        currentLine_++;
                    }
                    if (prev == '*' && cur == '/') {
                        break;
                    }
                    prev = cur;
                    cur = stream_.get();
                }
                continue;
            }
            // Not a comment; push back
            stream_.putback('/');
            return;
        }
        return;
    }
}

DefaultClearTextReader::Token DefaultClearTextReader::nextToken() {
    skipWhitespaceAndComments();
    if (reachedEOF_) {
        return {TokenType::EndOfFile, "", currentLine_};
    }

    int ch = stream_.peek();
    if (ch == EOF) {
        reachedEOF_ = true;
        return {TokenType::EndOfFile, "", currentLine_};
    }

    if (std::isspace(static_cast<unsigned char>(ch))) {
        skipWhitespaceAndComments();
        ch = stream_.peek();
        if (ch == EOF) {
            reachedEOF_ = true;
            return {TokenType::EndOfFile, "", currentLine_};
        }
    }
    return lexToken();
}

DefaultClearTextReader::Token DefaultClearTextReader::lexToken() {
    if (reachedEOF_) {
        return {TokenType::EndOfFile, "", currentLine_};
    }

    int ch = stream_.peek();
    if (ch == EOF) {
        reachedEOF_ = true;
        return {TokenType::EndOfFile, "", currentLine_};
    }

    int startLine = currentLine_;

    if (isIdentifierStart(static_cast<char>(ch))) {
        std::string identifier;
        while (true) {
            int c = stream_.peek();
            if (c == EOF || !isIdentifierChar(static_cast<char>(c))) {
                break;
            }
            identifier.push_back(static_cast<char>(stream_.get()));
        }
        return {TokenType::Identifier, identifier, startLine};
    }

    if (isNumberStart(static_cast<char>(ch))) {
        std::string number;
        bool hasExponent = false;
        bool hasDot = false;
        while (true) {
            int c = stream_.peek();
            if (c == EOF) {
                break;
            }
            char cc = static_cast<char>(c);
            if (std::isdigit(static_cast<unsigned char>(cc))) {
                number.push_back(static_cast<char>(stream_.get()));
            } else if ((cc == '+' || cc == '-') && !number.empty() && (number.back() == 'e' || number.back() == 'E')) {
                number.push_back(static_cast<char>(stream_.get()));
            } else if ((cc == 'e' || cc == 'E') && !hasExponent) {
                hasExponent = true;
                number.push_back(static_cast<char>(stream_.get()));
            } else if (cc == '.' && !hasDot) {
                hasDot = true;
                number.push_back(static_cast<char>(stream_.get()));
            } else {
                break;
            }
        }
        return {TokenType::Number, number, startLine};
    }

    if (ch == '\'' || ch == '"') {
        char quote = static_cast<char>(stream_.get());
        std::string value;
        while (true) {
            int c = stream_.get();
            if (c == EOF) {
                reachedEOF_ = true;
                return {TokenType::Invalid, value, startLine};
            }
            if (c == '\n') {
                currentLine_++;
            }
            if (c == quote) {
                int next = stream_.peek();
                if (next == quote) {
                    stream_.get();
                    value.push_back(static_cast<char>(quote));
                    continue;
                }
                break;
            }
            value.push_back(static_cast<char>(c));
        }
        return {TokenType::String, value, startLine};
    }

    stream_.get();
    if (ch == '\n') {
        currentLine_++;
    }
    std::string symbol(1, static_cast<char>(ch));
    return {TokenType::Symbol, symbol, startLine};
}

} // namespace opencgm
