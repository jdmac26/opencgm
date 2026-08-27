#ifndef OPENCGM_PROFILE_VALIDATOR_H
#define OPENCGM_PROFILE_VALIDATOR_H

#include "cgm_file.h"
#include "command.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <set>
#include <regex>

namespace opencgm {

// ============================================================================
// Profile Metadata - Enhanced detection information
// ============================================================================

/**
 * @brief Metadata extracted from CGM profile declarations
 *
 * This structure captures profile information parsed from the METAFILE DESCRIPTION
 * element, supporting patterns like "ProfileId:WebCGM", "ProfileEd:2.1"
 */
struct ProfileMetadata {
    std::string profileId;          // e.g., "WebCGM", "ATA GRAPHICS.GREXCHANGE", "S1000D"
    std::string profileEdition;     // e.g., "2.1", "2.9", "6.0"
    std::string sourceApplication;  // e.g., "IsoDraw", "CGM Workshop"
    int cgmVersion = 0;             // CGM version from file header
    double detectionConfidence = 0.0; // 0.0 to 1.0 - how confident we are in detection
    bool hasApsStructures = false;  // Whether Application Structures are present
    bool hasNurbsElements = false;  // Whether NURBS elements are present
    bool hasParabolicArcs = false;  // Whether parabolic arc elements are present
    int pictureCount = 0;           // Number of pictures in the metafile

    /**
     * @brief Parse ProfileId and ProfileEd from METAFILE DESCRIPTION
     * @param description The raw metafile description string
     * @return Populated ProfileMetadata
     */
    static ProfileMetadata parseFromDescription(const std::string& description);

    /**
     * @brief Check if this metadata indicates a specific profile
     */
    bool isWebCGM() const;
    bool isS1000D() const;
    bool isATAGREXCHANGE() const;
    bool isCALS() const;

    /**
     * @brief Get a human-readable summary
     */
    std::string toString() const;
};

// ============================================================================
// Element Constraints - Profile-specific element allowances
// ============================================================================

/**
 * @brief Defines which CGM elements are allowed or prohibited per profile
 *
 * CGM profiles restrict certain elements. For example:
 * - WebCGM 2.1 prohibits PARABOLIC ARC (4,23) and NURBS (4,24/4,25)
 * - S1000D inherits WebCGM restrictions
 * - ATA GREXCHANGE 2.9+ allows NURBS and parabolic arcs
 */
struct ElementConstraints {
    using ElementId = std::pair<int, int>;  // (ClassCode, ElementId)

    std::set<ElementId> prohibitedElements;  // Elements not allowed in this profile
    std::set<ElementId> requiredElements;    // Elements that must be present
    std::set<ElementId> deprecatedElements;  // Elements that generate warnings

    /**
     * @brief Check if an element is allowed
     */
    bool isAllowed(int classCode, int elementId) const {
        return prohibitedElements.find({classCode, elementId}) == prohibitedElements.end();
    }

    /**
     * @brief Check if an element is deprecated
     */
    bool isDeprecated(int classCode, int elementId) const {
        return deprecatedElements.find({classCode, elementId}) != deprecatedElements.end();
    }

    /**
     * @brief Get predefined constraints for WebCGM 2.1
     * WebCGM prohibits PARABOLIC ARC and HYPERBOLIC ARC (plus segments).
     * NUBS/NURBS are permitted since WebCGM 2.0 (cubic order 4, max 4096
     * control points, clamped form — enforced as warnings).
     */
    static ElementConstraints getWebCGM21Constraints();

    /**
     * @brief Get predefined constraints for S1000D
     * Inherits WebCGM restrictions with additional S1000D-specific rules
     */
    static ElementConstraints getS1000DConstraints();

    /**
     * @brief Get predefined constraints for ATA GREXCHANGE 2.9+
     * Allows NURBS and parabolic arcs
     */
    static ElementConstraints getATAGREXCHANGEConstraints();

    /**
     * @brief Get predefined constraints for CALS MIL-PRF-28003
     */
    static ElementConstraints getCALSConstraints();
};

// ============================================================================
// ICN Validation - S1000D Information Control Number
// ============================================================================

/**
 * @brief S1000D ICN (Information Control Number) validation
 *
 * ICN format: ICN-XXXXX-XXXXXXXX-XXX-XX
 * Components: ModelIdent-SDC-DI_Code-Variant
 */
struct IcnValidationResult {
    bool isValid = false;
    std::string modelIdent;
    std::string sdc;           // System Difference Code
    std::string diCode;        // Data Identification Code
    std::string variant;
    std::string errorMessage;

    static IcnValidationResult validate(const std::string& filename);
};

/**
 * @brief Severity levels for validation messages
 */
enum class ValidationSeverity {
    INFO,       // Informational message
    WARNING,    // Deviation from best practice but acceptable
    ERROR,      // Profile violation - non-conformant
    FATAL       // Critical error preventing further processing
};

/**
 * @brief Validation message with context
 */
struct ValidationMessage {
    ValidationSeverity severity;
    std::string message;
    std::string elementName;
    int elementId;
    ClassCode elementClass;
    int pictureIndex;
    int byteOffset;
    std::string rule;  // Reference to specification rule (e.g., "WebCGM-2.1 T.14.13")

    ValidationMessage(ValidationSeverity sev, const std::string& msg,
                     const std::string& elem = "", int eid = -1,
                     ClassCode eclass = ClassCode::DelimiterElement,
                     int picIdx = -1, int offset = -1, const std::string& r = "")
        : severity(sev), message(msg), elementName(elem), elementId(eid),
          elementClass(eclass), pictureIndex(picIdx), byteOffset(offset), rule(r) {}

    std::string toString() const;
    std::string getSeverityString() const;
};

/**
 * @brief CGM Profile types
 */
enum class ProfileType {
    UNKNOWN,
    WEBCGM_1_0,
    WEBCGM_2_0,
    WEBCGM_2_1,
    ISO_IEC_8632_COMPAT,
    ATA_GREXCHANGE_2_6,
    ATA_GREXCHANGE_2_7,
    ATA_GREXCHANGE_2_8,
    ATA_GREXCHANGE_2_9,
    S1000D_ISSUE_6,    ///< S1000D family (input detection bucket); v6 vs legacy is an output-side choice
    PIP_CGGC,          ///< POSC CGM*PIP — Petroleum Industry Profile (geoscience / seismic)
    CALS_MIL_PRF_28003 ///< CALS: MIL-D-28003A (CGM v1 binary) / MIL-PRF-28003B era sources
};

/**
 * @brief Abstract base class for profile validators
 */
class ProfileValidator {
public:
    virtual ~ProfileValidator() = default;

    /**
     * @brief Validate a CGM file against the profile
     * @param cgmFile The CGM file to validate
     * @return List of validation messages
     */
    virtual std::vector<ValidationMessage> validate(const CGMFile* cgmFile) = 0;

    /**
     * @brief Get the profile type
     */
    virtual ProfileType getProfileType() const = 0;

    /**
     * @brief Get the profile name
     */
    virtual std::string getProfileName() const = 0;

    /**
     * @brief Check if a specific element is allowed in this profile
     */
    virtual bool isElementAllowed(ClassCode elementClass, int elementId) const = 0;

    /**
     * @brief Check if the profile has specific constraints
     */
    virtual bool hasConstraint(const std::string& constraintName) const = 0;

protected:
    std::vector<ValidationMessage> messages_;
    int currentPictureIndex_ = 0;

    // Helper methods for validators
    void addError(const std::string& msg, const std::string& rule = "",
                  const std::string& element = "", int elementId = -1);
    void addWarning(const std::string& msg, const std::string& rule = "",
                    const std::string& element = "", int elementId = -1);
    void addInfo(const std::string& msg, const std::string& rule = "",
                 const std::string& element = "", int elementId = -1);
};

/**
 * @brief WebCGM 2.1 Profile Validator
 */
class WebCGM21Validator : public ProfileValidator {
public:
    WebCGM21Validator();

    void setAllowSegments(bool allow);

    std::vector<ValidationMessage> validate(const CGMFile* cgmFile) override;
    ProfileType getProfileType() const override { return ProfileType::WEBCGM_2_1; }
    std::string getProfileName() const override { return "WebCGM 2.1"; }
    bool isElementAllowed(ClassCode elementClass, int elementId) const override;
    bool hasConstraint(const std::string& constraintName) const override;

private:
    // Validation methods
    void validateMetafileStructure(const CGMFile* cgmFile);
    void validatePictureCount(const CGMFile* cgmFile);
    void validateTextConstraints(const CGMFile* cgmFile);
    void validateTextSequencing(const CGMFile* cgmFile);
    void validateStructuredDataRecords(const CGMFile* cgmFile);
    void validateNonGraphicalTextConstraints(const CGMFile* cgmFile);
    void validateApsStructure(const CGMFile* cgmFile);
    void validateProhibitedElements(const CGMFile* cgmFile);
    void validateColorTable(const CGMFile* cgmFile);
    void validateProtectionRegions(const CGMFile* cgmFile);
    void validateFigureComplexity(const CGMFile* cgmFile);
    void validateEncodingType(const CGMFile* cgmFile);
    void validateVersionSupport(const CGMFile* cgmFile);
    void validateTileCompression(const CGMFile* cgmFile);
    void validateNurbsKnotVectors(const CGMFile* cgmFile);
    void validateConicArcFallback(const CGMFile* cgmFile);
    void validateSymbolReferences(const CGMFile* cgmFile);
    void validateGeneralizedDrawingPrimitives(const CGMFile* cgmFile);
    void validateExternalElements(const CGMFile* cgmFile);
    void trackPictureIndex(const CGMFile* cgmFile);
    void checkTextCharacterRepertoire(const std::string& value,
                                      const std::string& elementName,
                                      int elementId);
    void reportCharacterEncodingFindings(const CGMFile* cgmFile);
    void validateXcfBindings(const CGMFile* cgmFile,
                             const std::unordered_map<std::string, std::string>& apsTypeById,
                             const std::map<std::string, int>& typeCounts);

    // Phase 1 Quick Win Validations
    void validateProfileDeclaration(const CGMFile* cgmFile);
    void validateCoordinateNormalization(const CGMFile* cgmFile);
    void validateTextBaselineAndUpVector(const CGMFile* cgmFile);
    void validateLineAndEdgeMetrics(const CGMFile* cgmFile);

    // Phase 2 High Impact Validations
    void validateFragmentIDLongForm(const CGMFile* cgmFile);
    void validateDegeneracyRules(const CGMFile* cgmFile);
    void validateElementSetRestrictions(const CGMFile* cgmFile);
    // Phase 3 Completeness Validations (100% Compliance)
    void validateRasterAspectPreservation(const CGMFile* cgmFile);
    void validateEnhancedURIValidation(const CGMFile* cgmFile);
    void validateEnhancedColorAnalysis(const CGMFile* cgmFile);

    // Prohibited elements in WebCGM 2.1
    std::map<ClassCode, std::vector<int>> prohibitedElements_;

    // Constraint flags
    bool allowSegments_ = false;
    bool allowMultiplePictures_ = false;
    int maxProtectionRegions_ = 1;
    int maxFigurePrimitives_ = 1024;
    std::vector<std::string> symbolLibraries_;
    std::vector<std::string> symbolLibraryResolvedPaths_;
    std::vector<bool> symbolLibraryLocalResolved_;
    std::vector<bool> symbolLibraryRemote_;
};

/**
 * @brief ATA GREXCHANGE Profile Validator
 */
class ATAGREXCHANGEValidator : public ProfileValidator {
public:
    enum class Version {
        V2_6,
        V2_7,
        V2_8,
        V2_9
    };

    explicit ATAGREXCHANGEValidator(Version version);

    std::vector<ValidationMessage> validate(const CGMFile* cgmFile) override;
    ProfileType getProfileType() const override;
    std::string getProfileName() const override;
    bool isElementAllowed(ClassCode elementClass, int elementId) const override;
    bool hasConstraint(const std::string& constraintName) const override;

private:
    Version version_;

    // ATA-specific validation
    void validateEdgeVisibility(const CGMFile* cgmFile);
    void validateGeneralizedDrawingPrimitives(const CGMFile* cgmFile);
    void validateExternalReferences(const CGMFile* cgmFile);
    void validateColorClass(const CGMFile* cgmFile);
    void validateProfileDeclarationATA(const CGMFile* cgmFile);
    void validateColorClassDeclaration(const CGMFile* cgmFile);
};

/**
 * @brief S1000D Profile Validator
 */
class S1000DValidator : public ProfileValidator {
public:
    S1000DValidator();

    std::vector<ValidationMessage> validate(const CGMFile* cgmFile) override;
    ProfileType getProfileType() const override { return ProfileType::S1000D_ISSUE_6; }
    std::string getProfileName() const override { return "S1000D Issue 6"; }
    bool isElementAllowed(ClassCode elementClass, int elementId) const override;
    bool hasConstraint(const std::string& constraintName) const override;

private:
    // S1000D-specific validation
    void validateMetadata(const CGMFile* cgmFile);
    void validateCSDBNaming(const CGMFile* cgmFile);
    void validateTextHeights(const CGMFile* cgmFile);
    void validateAPSBinding(const CGMFile* cgmFile);
    void validateMonochromePolicy(const CGMFile* cgmFile);
    void validateAPSConstraints(const CGMFile* cgmFile);
    void validateMetafileDescriptionFormat(const CGMFile* cgmFile);
};

/**
 * @brief Profile detector - automatically detects CGM profile from metafile
 */
class ProfileDetector {
public:
    struct DetectionResult
    {
        ProfileType profile;
        std::string reason;
        bool confident;
        ProfileMetadata metadata;  // Enhanced metadata from detection
    };

    /**
     * @brief Detect the profile type from a CGM file
     * @param cgmFile The CGM file to analyze
     * @return Detection result containing profile and reasoning
     */
    static DetectionResult detectProfile(const CGMFile* cgmFile);

    /**
     * @brief Enhanced detection with full metadata extraction
     * @param cgmFile The CGM file to analyze
     * @return Detection result with comprehensive metadata
     */
    static DetectionResult detectProfileWithMetadata(const CGMFile* cgmFile);

    /**
     * @brief Get profile version string from metafile description
     * @param description Metafile description string
     * @return Profile version (e.g., "ATA GREXCHANGE 2.9")
     */
    static std::string extractProfileVersion(const std::string& description);

    /**
     * @brief Create appropriate validator for detected profile
     * @param profileType The profile type
     * @return Validator instance
     */
    static std::unique_ptr<ProfileValidator> createValidator(ProfileType profileType);

    /**
     * @brief Get element constraints for a specific profile
     * @param profileType The profile type
     * @return Element constraints for the profile
     */
    static ElementConstraints getElementConstraints(ProfileType profileType);

    /**
     * @brief Check if a CGM file contains NURBS elements
     * @param cgmFile The CGM file to analyze
     * @return True if NURBS elements are present
     */
    static bool hasNurbsElements(const CGMFile* cgmFile);

    /**
     * @brief Check if a CGM file contains Application Structures
     * @param cgmFile The CGM file to analyze
     * @return True if APS elements are present
     */
    static bool hasApplicationStructures(const CGMFile* cgmFile);
};

/**
 * @brief Validation report generator
 */
class ValidationReport {
public:
    ValidationReport(const std::vector<ValidationMessage>& messages);

    /**
     * @brief Generate human-readable report
     */
    std::string generateTextReport() const;

    /**
     * @brief Generate JSON report
     */
    std::string generateJSONReport() const;

    /**
     * @brief Get count by severity
     */
    int getErrorCount() const;
    int getWarningCount() const;
    int getInfoCount() const;

    /**
     * @brief Check if validation passed (no errors or fatal)
     */
    bool passed() const;

private:
    std::vector<ValidationMessage> messages_;
};

} // namespace opencgm

#endif // OPENCGM_PROFILE_VALIDATOR_H
