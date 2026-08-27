#ifndef OPENCGM_EXTERNAL_COMMANDS_H
#define OPENCGM_EXTERNAL_COMMANDS_H

#include "../command.h"
#include <string>

namespace opencgm {

// Element ID 1: MESSAGE
class MessageCommand : public Command {
public:
    explicit MessageCommand(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int action() const { return action_; } // 0=NoAction, 1=Action
    const std::string& message() const { return message_; }

private:
    int action_;
    std::string message_;
};

// Element ID 2: APPLICATION DATA
class ApplicationData : public Command {
public:
    explicit ApplicationData(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    const std::string& data() const { return data_; }

private:
    int identifier_;
    std::string data_;
};

} // namespace opencgm

#endif // OPENCGM_EXTERNAL_COMMANDS_H
