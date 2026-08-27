#ifndef OPENCGM_ESCAPE_COMMANDS_H
#define OPENCGM_ESCAPE_COMMANDS_H

#include "../command.h"
#include <string>

namespace opencgm {

// Element ID 0 (in DelimiterElement class): NO-OP
class NoOp : public Command {
public:
    explicit NoOp(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    // No parameters - this is a no-operation command
};

// Escape Element (Class 6, variable element ID)
class Escape : public Command {
public:
    explicit Escape(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    const std::string& dataRecord() const { return dataRecord_; }

private:
    int identifier_;
    std::string dataRecord_;
};

} // namespace opencgm

#endif // OPENCGM_ESCAPE_COMMANDS_H
