#ifndef OPENCGM_SEGMENT_CONTROL_COMMANDS_H
#define OPENCGM_SEGMENT_CONTROL_COMMANDS_H

#include "../command.h"
#include "../cgm_point.h"
#include <vector>
#include <string>

namespace opencgm {

// Element ID 1: COPY SEGMENT
class CopySegment : public Command {
public:
    explicit CopySegment(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int id() const { return id_; }
    double xScale() const { return xScale_; }
    double xRotation() const { return xRotation_; }
    double yRotation() const { return yRotation_; }
    double yScale() const { return yScale_; }
    double xTranslation() const { return xTranslation_; }
    double yTranslation() const { return yTranslation_; }
    bool flag() const { return flag_; }

private:
    int id_;
    double xScale_;
    double xRotation_;
    double yRotation_;
    double yScale_;
    double xTranslation_;
    double yTranslation_;
    bool flag_;
};

// Element ID 2: INHERITANCE FILTER
class InheritanceFilter : public Command {
public:
    explicit InheritanceFilter(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<int>& values() const { return values_; }
    int setting() const { return setting_; }

private:
    std::vector<int> values_;
    int setting_;
};

// Element ID 3: CLIP INHERITANCE
class ClipInheritanceCommand : public Command {
public:
    explicit ClipInheritanceCommand(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    ClipInheritance value() const { return value_; }

private:
    ClipInheritance value_;
};

// Element ID 4: SEGMENT TRANSFORMATION
class SegmentTransformation : public Command {
public:
    explicit SegmentTransformation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    double scaleX() const { return scaleX_; }
    double rotationX() const { return rotationX_; }
    double rotationY() const { return rotationY_; }
    double scaleY() const { return scaleY_; }
    double translationX() const { return translationX_; }
    double translationY() const { return translationY_; }

private:
    int identifier_;
    double scaleX_;
    double rotationX_;
    double rotationY_;
    double scaleY_;
    double translationX_;
    double translationY_;
};

// Element ID 5: SEGMENT HIGHLIGHTING
class SegmentHighlighting : public Command {
public:
    explicit SegmentHighlighting(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    int value() const { return value_; }

private:
    int identifier_;
    int value_; // 0=NORMAL, 1=HIGHL
};

// Element ID 6: SEGMENT DISPLAY PRIORITY
class SegmentDisplayPriority : public Command {
public:
    explicit SegmentDisplayPriority(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int name() const { return name_; }
    int priority() const { return priority_; }

private:
    int name_;
    int priority_;
};

// Element ID 7: SEGMENT PICK PRIORITY
class SegmentPickPriority : public Command {
public:
    explicit SegmentPickPriority(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    int priority() const { return priority_; }

private:
    int identifier_;
    int priority_;
};

} // namespace opencgm

#endif // OPENCGM_SEGMENT_CONTROL_COMMANDS_H
