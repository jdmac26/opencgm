#ifndef OPENCGM_TEXT_CGM_FILE_H
#define OPENCGM_TEXT_CGM_FILE_H

#include "opencgm/cgm_file.h"
#include "opencgm/clear_text_reader.h"
#include "opencgm/command_factory.h"
#include <string>
#include <vector>

namespace opencgm {

/**
 * @brief CGM container specialised for clear text input (ISO/IEC 8632-4)
 */
class TextCGMFile : public CGMFile {
public:
    TextCGMFile();
    explicit TextCGMFile(const std::string& fileName);

    /**
     * @brief Load a clear text CGM from disk.
     */
    void readFile(const std::string& fileName);

    /**
     * @brief Load a clear text CGM from an input stream.
     */
    void readStream(std::istream& stream, const std::string& logicalName = "stream");

    const std::vector<DefaultClearTextReader::Statement>& statements() const {
        return statements_;
    }

private:
    void readData(std::istream& stream, const std::string& logicalName);
    bool translateStatement(const DefaultClearTextReader::Statement& stmt, DefaultCommandFactory& factory);
    void logTranslationError(const DefaultClearTextReader::Statement& stmt, const std::string& reason);

    std::vector<DefaultClearTextReader::Statement> statements_;
};

} // namespace opencgm

#endif // OPENCGM_TEXT_CGM_FILE_H
