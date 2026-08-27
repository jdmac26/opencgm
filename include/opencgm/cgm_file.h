#ifndef OPENCGM_FILE_H
#define OPENCGM_FILE_H

#include "opencgm/command.h"
#include "opencgm/enums.h"
#include "opencgm/cgm_color.h"
#include "opencgm/cgm_point.h"
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <utility>

namespace opencgm {

/**
 * @brief Message severity and information
 */
struct Message {
    Severity severity;
    ClassCode elementClass;
    int elementId;
    std::string message;
    std::string commandName;

    Message(Severity sev, ClassCode ec, int eid, const std::string& msg, const std::string& cmd)
        : severity(sev), elementClass(ec), elementId(eid), message(msg), commandName(cmd) {}
};

/**
 * @brief Information about a picture within a CGM file
 */
struct PictureRange {
    size_t startCommandIndex;  ///< Index of the BEGIN PICTURE command
    size_t endCommandIndex;    ///< Index of the END PICTURE command
    std::string name;          ///< Picture name (from BEGIN PICTURE, may be empty)
};

/**
 * @brief Base class for CGM files
 */
class CGMFile {
public:
    virtual ~CGMFile() = default;

    /**
     * @brief Get the file name
     */
    const std::string& name() const { return name_; }

    /**
     * @brief Get the source file name/path associated with this CGM.
     * Defaults to logical name and may be overridden by subclasses.
     */
    virtual const std::string& fileName() const { return name_; }

    /**
     * @brief Get all commands in this file
     */
    const std::vector<CommandPtr>& commands() const { return commands_; }
    std::vector<CommandPtr>& commands() { return commands_; }

    const std::vector<CommandPtr>& defaultsReplacementCommands() const { return defaultsReplacementCommands_; }
    void setDefaultsReplacementCommands(std::vector<CommandPtr> commands) { defaultsReplacementCommands_ = std::move(commands); }

    /**
     * @brief Get all messages generated during processing
     */
    const std::vector<Message>& messages() const { return messages_; }
    void addMessage(const Message& message);

    /**
     * @brief Get information about all pictures in this CGM file
     * @return Vector of PictureRange structures describing each picture's command range
     */
    std::vector<PictureRange> getPictureRanges() const;

    // Configuration getters
    int colourIndexPrecision() const { return colourIndexPrecision_; }
    int colourPrecision() const { return colourPrecision_; }
    int indexPrecision() const { return indexPrecision_; }
    int integerPrecision() const { return integerPrecision_; }
    int namePrecision() const { return namePrecision_; }
    int vdcIntegerPrecision() const { return vdcIntegerPrecision_; }
    Precision vdcRealPrecision() const { return vdcRealPrecision_; }
    Precision realPrecision() const { return realPrecision_; }
    bool realPrecisionProcessed() const { return realPrecisionProcessed_; }
    SpecificationMode edgeWidthSpecificationMode() const { return edgeWidthSpecificationMode_; }
    SpecificationMode lineWidthSpecificationMode() const { return lineWidthSpecificationMode_; }
    SpecificationMode markerSizeSpecificationMode() const { return markerSizeSpecificationMode_; }
    SpecificationMode interiorStyleSpecificationMode() const { return interiorStyleSpecificationMode_; }
    VDCType vdcType() const { return vdcType_; }
    ColorModel colorModel() const { return colorModel_; }
    SpecificationMode scalingMode() const { return scalingMode_; }
    double metricScaleFactor() const { return metricScaleFactor_; }
    ColorSelectionMode colorSelectionMode() const { return colorSelectionMode_; }

    // Configuration setters (underscore prefix style - legacy)
    void set_colourIndexPrecision(int value) { colourIndexPrecision_ = value; }
    void set_colourPrecision(int value) { colourPrecision_ = value; }
    void set_indexPrecision(int value) { indexPrecision_ = value; }
    void set_integerPrecision(int value) { integerPrecision_ = value; }
    void set_namePrecision(int value) { namePrecision_ = value; }
    void set_vdcIntegerPrecision(int value) { vdcIntegerPrecision_ = value; }
    void set_vdcRealPrecision(Precision value) { vdcRealPrecision_ = value; }
    void set_realPrecision(Precision value) { realPrecision_ = value; }
    void set_realPrecisionProcessed(bool value) { realPrecisionProcessed_ = value; }
    void set_edgeWidthSpecificationMode(SpecificationMode value) { edgeWidthSpecificationMode_ = value; }
    void set_lineWidthSpecificationMode(SpecificationMode value) { lineWidthSpecificationMode_ = value; }
    void set_markerSizeSpecificationMode(SpecificationMode value) { markerSizeSpecificationMode_ = value; }
    void set_interiorStyleSpecificationMode(SpecificationMode value) { interiorStyleSpecificationMode_ = value; }
    void set_vdcType(VDCType value) { vdcType_ = value; }

    // Configuration setters (camelCase style - used by command implementations)
    void setColourIndexPrecision(int value) { colourIndexPrecision_ = value; }
    void setColourPrecision(int value) { colourPrecision_ = value; }
    void setIndexPrecision(int value) { indexPrecision_ = value; }
    void setIntegerPrecision(int value) { integerPrecision_ = value; }
    void setNamePrecision(int value) { namePrecision_ = value; }
    void setVdcIntegerPrecision(int value) { vdcIntegerPrecision_ = value; }
    void setVdcRealPrecision(Precision value) { vdcRealPrecision_ = value; }
    void setRealPrecision(Precision value) { realPrecision_ = value; }
    void setRealPrecisionProcessed(bool value) { realPrecisionProcessed_ = value; }
    void setEdgeWidthSpecificationMode(SpecificationMode value) { edgeWidthSpecificationMode_ = value; }
    void setLineWidthSpecificationMode(SpecificationMode value) { lineWidthSpecificationMode_ = value; }
    void setMarkerSizeSpecificationMode(SpecificationMode value) { markerSizeSpecificationMode_ = value; }
    void setInteriorStyleSpecificationMode(SpecificationMode value) { interiorStyleSpecificationMode_ = value; }
    void setVdcType(VDCType value) { vdcType_ = value; }
    void setColorModel(ColorModel value) { colorModel_ = value; }
    void setScalingMode(SpecificationMode value) { scalingMode_ = value; }
    void setMetricScaleFactor(double value) { metricScaleFactor_ = value; }
    void setColorSelectionMode(ColorSelectionMode value) { colorSelectionMode_ = value; }
    void setVdcExtent(const CGMPoint& firstCorner, const CGMPoint& secondCorner) {
        vdcExtentFirstCorner_ = firstCorner;
        vdcExtentSecondCorner_ = secondCorner;
    }
    void setCharacterCoding(int value) { characterCodingAnnouncer_ = value; }
    int characterCoding() const { return characterCodingAnnouncer_; }
    void setCharacterSetList(const std::vector<std::pair<int, std::string>>& sets) { characterSets_ = sets; }
    const std::vector<std::pair<int, std::string>>& characterSetList() const { return characterSets_; }
    const std::vector<double>& colourCalibrationData() const { return colourCalibrationData_; }
    void setColourCalibrationData(std::vector<double> data) { colourCalibrationData_ = std::move(data); }
    void setCharacterSetIndex(int index) { characterSetIndex_ = index; }
    int characterSetIndex() const { return characterSetIndex_; }
    void setAlternateCharacterSetIndex(int index) { alternateCharacterSetIndex_ = index; }
    int alternateCharacterSetIndex() const { return alternateCharacterSetIndex_; }

    void reportUnsupportedCharset(const std::string& designation) const;
    const std::set<std::string>& unsupportedCharsets() const { return unsupportedCharsets_; }
    void reportIllegalControl(uint32_t codePoint) const;
    const std::set<uint32_t>& illegalControls() const { return illegalControls_; }

    /**
     * @brief Reset all meta definitions to defaults
     */
    void resetMetaDefinitions();

protected:
    CGMFile();

    std::string name_;
    std::vector<CommandPtr> commands_;
    std::vector<Message> messages_;
    std::vector<CommandPtr> defaultsReplacementCommands_;

    // Configuration settings
    int colourIndexPrecision_;
    int colourPrecision_;
    int indexPrecision_;
    int integerPrecision_;
    int namePrecision_;
    int vdcIntegerPrecision_;
    Precision vdcRealPrecision_;
    Precision realPrecision_;
    bool realPrecisionProcessed_;
    SpecificationMode edgeWidthSpecificationMode_;
    SpecificationMode lineWidthSpecificationMode_;
    SpecificationMode markerSizeSpecificationMode_;
    SpecificationMode interiorStyleSpecificationMode_;
    VDCType vdcType_;
    ColorModel colorModel_;
    SpecificationMode scalingMode_;
    double metricScaleFactor_;
    ColorSelectionMode colorSelectionMode_;
    CGMPoint vdcExtentFirstCorner_;
    CGMPoint vdcExtentSecondCorner_;
    int characterCodingAnnouncer_;
    int characterSetIndex_;
    int alternateCharacterSetIndex_;
    std::vector<std::pair<int, std::string>> characterSets_;
    mutable std::set<std::string> unsupportedCharsets_;
    mutable std::set<uint32_t> illegalControls_;
    std::vector<double> colourCalibrationData_;
};

/**
 * @brief Binary CGM file implementation
 */
class BinaryCGMFile : public CGMFile {
public:
    BinaryCGMFile();
    explicit BinaryCGMFile(const std::string& fileName);
    BinaryCGMFile(std::istream& stream, const std::string& name = "stream");

    /**
     * @brief Write the CGM commands to a file
     * @warning NOT IMPLEMENTED - This method is incomplete and will produce no output
     *
     * The binary CGM writing functionality is not fully implemented.
     * While the writer infrastructure exists, the command serialization
     * pipeline requires completion and testing. Calling this method will
     * generate a warning to stderr and produce no output file.
     *
     * @param fileName Path to output CGM file (currently unused)
     */
    void writeFile(const std::string& fileName);

    /**
     * @brief Write the CGM commands to a stream
     * @warning NOT IMPLEMENTED - This method is incomplete and will produce no output
     *
     * The binary CGM writing functionality is not fully implemented.
     * While the writer infrastructure exists, the command serialization
     * pipeline requires completion and testing. Calling this method will
     * generate a warning to stderr and produce no output.
     *
     * @param stream Output stream (currently unused)
     */
    void writeFile(std::ostream& stream);

    /**
     * @brief Get the file name
     */
    const std::string& fileName() const override { return fileName_; }

private:
    void readData(std::istream& stream);
    void readData(const std::string& fileName);

    std::string fileName_;
};

} // namespace opencgm

#endif // OPENCGM_FILE_H
