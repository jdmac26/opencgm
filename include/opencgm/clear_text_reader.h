#ifndef OPENCGM_CLEAR_TEXT_READER_H
#define OPENCGM_CLEAR_TEXT_READER_H

#include "opencgm/interfaces.h"
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace opencgm {

class CGMFile;

/**
 * @brief Minimal clear text CGM reader (ISO/IEC 8632-4 scaffold)
 *
 * This reader performs lexical analysis of clear text CGM streams and
 * produces a sequence of statements. Command-to-object translation is
 * introduced incrementally; initially unsupported statements are recorded
 * for downstream processing.
 */
class DefaultClearTextReader {
public:
    enum class TokenType {
        Identifier,
        Number,
        String,
        Symbol,
        EndOfFile,
        Invalid
    };

    struct Token {
        TokenType type;
        std::string lexeme;
        int line;
    };

    struct Statement {
        std::string command;
        std::vector<Token> tokens;
        std::string payload;
        int line;
    };

    DefaultClearTextReader(std::istream& stream, CGMFile* cgm, ICommandFactory* factory);

    /**
     * @brief Parse the clear text stream and collect statements.
     *
     * Commands that cannot yet be mapped to CGM objects are recorded
     * verbatim for later handling. Future milestones will translate
     * specific commands into concrete Command instances.
     */
    void readStatements();

    const std::vector<Statement>& statements() const { return statements_; }

private:
    Token lexToken();
    Token nextToken();
    void skipWhitespaceAndComments();
    std::istream& stream_;
    CGMFile* cgm_;
    ICommandFactory* factory_;
    std::vector<Statement> statements_;
    int currentLine_;
    bool reachedEOF_;
};

} // namespace opencgm

#endif // OPENCGM_CLEAR_TEXT_READER_H
