#include "opencgm/text_cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/clear_text_reader.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {

using Token = opencgm::DefaultClearTextReader::Token;
using TokenType = opencgm::DefaultClearTextReader::TokenType;

std::string toUpperCopy(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return result;
}

bool isSymbol(const Token& token, const char* symbol) {
    return token.type == TokenType::Symbol && token.lexeme == symbol;
}

void skipCommaTokens(const std::vector<Token>& tokens, size_t& pos) {
    while (pos < tokens.size() && isSymbol(tokens[pos], ",")) {
        ++pos;
    }
}

std::optional<int> tokenToInt(const Token& token) {
    if (token.type == TokenType::Number || token.type == TokenType::Identifier) {
        try {
            int value = std::stoi(token.lexeme);
            return value;
        } catch (...) {
        }
    }
    return std::nullopt;
}

std::optional<double> tokenToDouble(const Token& token) {
    if (token.type == TokenType::Number || token.type == TokenType::Identifier) {
        try {
            double value = std::stod(token.lexeme);
            return value;
        } catch (...) {
        }
    }
    return std::nullopt;
}

std::string tokenToStringValue(const Token& token) {
    return token.lexeme;
}

std::string joinTokensAsString(const std::vector<Token>& tokens) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& token : tokens) {
        if (token.type == TokenType::Symbol) {
            continue;
        }
        if (!first) {
            oss << ' ';
        }
        oss << token.lexeme;
        first = false;
    }
    return oss.str();
}

bool parsePoint(const std::vector<Token>& tokens, size_t& pos, opencgm::CGMPoint& point) {
    if (pos >= tokens.size() || !isSymbol(tokens[pos], "(")) {
        return false;
    }
    ++pos;
    skipCommaTokens(tokens, pos);

    if (pos >= tokens.size()) {
        return false;
    }
    auto xOpt = tokenToDouble(tokens[pos]);
    if (!xOpt) {
        return false;
    }
    ++pos;
    skipCommaTokens(tokens, pos);

    if (pos >= tokens.size()) {
        return false;
    }
    auto yOpt = tokenToDouble(tokens[pos]);
    if (!yOpt) {
        return false;
    }
    ++pos;
    skipCommaTokens(tokens, pos);

    if (pos >= tokens.size() || !isSymbol(tokens[pos], ")")) {
        return false;
    }
    ++pos;
    point = opencgm::CGMPoint(*xOpt, *yOpt);
    return true;
}

} // namespace

namespace opencgm {

TextCGMFile::TextCGMFile() {
    name_ = "new-clear-text";
}

TextCGMFile::TextCGMFile(const std::string& fileName) {
    readFile(fileName);
}

void TextCGMFile::readFile(const std::string& fileName) {
    std::ifstream stream(fileName);
    if (!stream) {
        throw std::runtime_error("Failed to open clear text CGM file: " + fileName);
    }
    readStream(stream, fileName);
}

void TextCGMFile::readStream(std::istream& stream, const std::string& logicalName) {
    readData(stream, logicalName);
}

void TextCGMFile::readData(std::istream& stream, const std::string& logicalName) {
    resetMetaDefinitions();
    if (!logicalName.empty()) {
        size_t pos = logicalName.find_last_of("/\\");
        name_ = (pos == std::string::npos) ? logicalName : logicalName.substr(pos + 1);
    } else {
        name_ = "stream";
    }
    commands_.clear();
    defaultsReplacementCommands_.clear();
    messages_.clear();

    DefaultCommandFactory factory;
    DefaultClearTextReader reader(stream, this, &factory);
    reader.readStatements();
    statements_ = reader.statements();

    for (const auto& stmt : statements_) {
        translateStatement(stmt, factory);
    }
}

void TextCGMFile::logTranslationError(const DefaultClearTextReader::Statement& stmt, const std::string& reason) {
    std::ostringstream oss;
    oss << "Line " << stmt.line << " (" << stmt.command << "): " << reason;
    addMessage(Message(Severity::Unsupported,
                       ClassCode::DelimiterElement,
                       -1,
                       oss.str(),
                       "CLEAR_TEXT"));
}

bool TextCGMFile::translateStatement(const DefaultClearTextReader::Statement& stmt, DefaultCommandFactory& factory) {
    const std::string commandUpper = toUpperCopy(stmt.command);

    auto createCommand = [&](int elementId, ClassCode classCode) -> CommandPtr {
        return factory.createCommand(elementId, static_cast<int>(classCode), this);
    };

    if (commandUpper == "BEGMF") {
        std::string metafileName = stmt.tokens.empty() ? "" : tokenToStringValue(stmt.tokens.front());
        auto command = createCommand(1, ClassCode::DelimiterElement);
        if (!command) {
            logTranslationError(stmt, "Failed to create BEGMF command");
            return false;
        }
        if (auto* beginMf = dynamic_cast<BeginMetafile*>(command.get())) {
            beginMf->setName(metafileName);
        }
        commands_.push_back(std::move(command));
        if (!metafileName.empty()) {
            name_ = metafileName;
        }
        return true;
    }

    if (commandUpper == "ENDMF") {
        auto command = createCommand(2, ClassCode::DelimiterElement);
        if (!command) {
            logTranslationError(stmt, "Failed to create ENDMF command");
            return false;
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "MFVERSION") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "MFVERSION requires a version number");
            return false;
        }
        auto versionOpt = tokenToInt(stmt.tokens.front());
        if (!versionOpt) {
            logTranslationError(stmt, "Invalid version value");
            return false;
        }
        auto command = createCommand(1, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create MFVERSION command");
            return false;
        }
        if (auto* metaVersion = dynamic_cast<MetafileVersion*>(command.get())) {
            metaVersion->setVersion(*versionOpt);
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "MFDESC") {
        std::string description = stmt.tokens.empty() ? "" : (stmt.tokens.front().type == TokenType::String
                                                                  ? stmt.tokens.front().lexeme
                                                                  : joinTokensAsString(stmt.tokens));
        auto command = createCommand(2, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create MFDESC command");
            return false;
        }
        if (auto* metaDesc = dynamic_cast<MetafileDescription*>(command.get())) {
            metaDesc->setDescription(description);
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "VDCTYPE") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "VDCTYPE requires a value (INTEGER|REAL)");
            return false;
        }
        std::string valueUpper = toUpperCopy(stmt.tokens.front().lexeme);
        VDCType vdcType = (valueUpper == "REAL") ? VDCType::Real : VDCType::Integer;
        if (valueUpper != "REAL" && valueUpper != "INTEGER") {
            logTranslationError(stmt, "Unsupported VDCTYPE value: " + stmt.tokens.front().lexeme);
            return false;
        }
        auto command = createCommand(3, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create VDCTYPE command");
            return false;
        }
        if (auto* vdcCmd = dynamic_cast<VDCTypeCommand*>(command.get())) {
            vdcCmd->setVdcType(vdcType);
        }
        setVdcType(vdcType);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "INTEGERPREC") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "INTEGERPREC requires a precision value");
            return false;
        }
        auto precisionOpt = tokenToInt(stmt.tokens.front());
        if (!precisionOpt) {
            logTranslationError(stmt, "Invalid INTEGERPREC value");
            return false;
        }
        auto command = createCommand(4, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create INTEGERPREC command");
            return false;
        }
        if (auto* intPrec = dynamic_cast<IntegerPrecision*>(command.get())) {
            intPrec->setPrecision(*precisionOpt);
        }
        setIntegerPrecision(*precisionOpt);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "REALPREC") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "REALPREC requires precision parameters");
            return false;
        }
        std::vector<Token> filtered;
        filtered.reserve(stmt.tokens.size());
        for (const auto& token : stmt.tokens) {
            if (isSymbol(token, ",")) {
                continue;
            }
            filtered.push_back(token);
        }
        if (filtered.empty()) {
            logTranslationError(stmt, "REALPREC parameters missing");
            return false;
        }
        Precision precision = Precision::Floating_32;
        if (filtered.front().type == TokenType::Identifier &&
            toUpperCopy(filtered.front().lexeme) == "FIXED") {
            if (filtered.size() < 2) {
                logTranslationError(stmt, "REALPREC FIXED requires field width");
                return false;
            }
            auto widthOpt = tokenToInt(filtered[1]);
            if (!widthOpt) {
                logTranslationError(stmt, "Invalid REALPREC FIXED width");
                return false;
            }
            precision = (*widthOpt == 64) ? Precision::Fixed_64 : Precision::Fixed_32;
        } else {
            auto widthOpt = tokenToInt(filtered.front());
            if (!widthOpt) {
                logTranslationError(stmt, "Invalid REALPREC width");
                return false;
            }
            precision = (*widthOpt == 64) ? Precision::Floating_64 : Precision::Floating_32;
        }
        auto command = createCommand(5, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create REALPREC command");
            return false;
        }
        if (auto* realPrec = dynamic_cast<RealPrecision*>(command.get())) {
            realPrec->setPrecision(precision);
        }
        setRealPrecision(precision);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "INDEXPREC") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "INDEXPREC requires a precision value");
            return false;
        }
        auto precisionOpt = tokenToInt(stmt.tokens.front());
        if (!precisionOpt) {
            logTranslationError(stmt, "Invalid INDEXPREC value");
            return false;
        }
        auto command = createCommand(6, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create INDEXPREC command");
            return false;
        }
        if (auto* indexPrec = dynamic_cast<IndexPrecision*>(command.get())) {
            indexPrec->setPrecision(*precisionOpt);
        }
        setIndexPrecision(*precisionOpt);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "COLOURPREC") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "COLOURPREC requires a value");
            return false;
        }
        auto precisionOpt = tokenToInt(stmt.tokens.front());
        if (!precisionOpt) {
            logTranslationError(stmt, "Invalid COLOURPREC value");
            return false;
        }
        auto command = createCommand(7, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create COLOURPREC command");
            return false;
        }
        if (auto* colourPrec = dynamic_cast<ColourPrecision*>(command.get())) {
            colourPrec->setPrecision(*precisionOpt);
        }
        setColourPrecision(*precisionOpt);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "COLOURINDEXPREC") {
        if (stmt.tokens.empty()) {
            logTranslationError(stmt, "COLOURINDEXPREC requires a value");
            return false;
        }
        auto precisionOpt = tokenToInt(stmt.tokens.front());
        if (!precisionOpt) {
            logTranslationError(stmt, "Invalid COLOURINDEXPREC value");
            return false;
        }
        auto command = createCommand(8, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create COLOURINDEXPREC command");
            return false;
        }
        if (auto* colourIndexPrec = dynamic_cast<ColourIndexPrecision*>(command.get())) {
            colourIndexPrec->setPrecision(*precisionOpt);
        }
        setColourIndexPrecision(*precisionOpt);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "MAXVDCEXT") {
        size_t pos = 0;
        CGMPoint first, second;
        if (!parsePoint(stmt.tokens, pos, first)) {
            logTranslationError(stmt, "MAXVDCEXT missing first corner");
            return false;
        }
        skipCommaTokens(stmt.tokens, pos);
        if (!parsePoint(stmt.tokens, pos, second)) {
            logTranslationError(stmt, "MAXVDCEXT missing second corner");
            return false;
        }
        auto command = createCommand(17, ClassCode::MetafileDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create MAXVDCEXT command");
            return false;
        }
        if (auto* extent = dynamic_cast<MaximumVDCExtent*>(command.get())) {
            extent->setExtent(first, second);
        }
        setVdcExtent(first, second);
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "BEGPIC") {
        std::string pictureName = stmt.tokens.empty() ? "" : tokenToStringValue(stmt.tokens.front());
        auto command = createCommand(3, ClassCode::DelimiterElement);
        if (!command) {
            logTranslationError(stmt, "Failed to create BEGPIC command");
            return false;
        }
        if (auto* beginPic = dynamic_cast<BeginPicture*>(command.get())) {
            beginPic->setName(pictureName);
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "BEGPICBODY") {
        auto command = createCommand(4, ClassCode::DelimiterElement);
        if (!command) {
            logTranslationError(stmt, "Failed to create BEGPICBODY command");
            return false;
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "ENDPIC") {
        auto command = createCommand(5, ClassCode::DelimiterElement);
        if (!command) {
            logTranslationError(stmt, "Failed to create ENDPIC command");
            return false;
        }
        commands_.push_back(std::move(command));
        return true;
    }

    if (commandUpper == "VDCEXT") {
        size_t pos = 0;
        CGMPoint first, second;
        if (!parsePoint(stmt.tokens, pos, first)) {
            logTranslationError(stmt, "VDCEXT missing first corner");
            return false;
        }
        skipCommaTokens(stmt.tokens, pos);
        if (!parsePoint(stmt.tokens, pos, second)) {
            logTranslationError(stmt, "VDCEXT missing second corner");
            return false;
        }
        auto command = createCommand(6, ClassCode::PictureDescriptorElements);
        if (!command) {
            logTranslationError(stmt, "Failed to create VDCEXT command");
            return false;
        }
        if (auto* vdcExtent = dynamic_cast<VDCExtent*>(command.get())) {
            vdcExtent->setExtent(first, second);
        }
        setVdcExtent(first, second);
        commands_.push_back(std::move(command));
        return true;
    }

    logTranslationError(stmt, "Unsupported command");
    return false;
}

} // namespace opencgm
