// Error messages that tell users what to type must name options that exist,
// in a form the command-line tools accept.

#include "cgm_test_builder.h"
#include "../src/tools/cli_args.h"

#include <string>
#include <vector>

namespace
{
    using opencgm_test::CgmBuilder;
    using B = CgmBuilder;

    std::vector<std::string> expand(std::vector<std::string> args)
    {
        std::vector<char *> argv;
        for (auto &a : args)
            argv.push_back(a.data());
        return expandOptionAssignments(static_cast<int>(argv.size()), argv.data());
    }
}

TEST(CliArgs, OptionAssignmentsSplitIntoNameAndValue)
{
    EXPECT_EQ(expand({"tool", "--profile=compat", "in.cgm", "out.svg"}),
              (std::vector<std::string>{"tool", "--profile", "compat", "in.cgm", "out.svg"}));
    EXPECT_EQ(expand({"tool", "--font-map=C:/maps/a=b.json"}),
              (std::vector<std::string>{"tool", "--font-map", "C:/maps/a=b.json"}))
        << "only the first '=' separates the value";
}

TEST(CliArgs, EverythingElseIsLeftAlone)
{
    const std::vector<std::string> args{"tool", "--profile", "compat", "-p=x", "a=b.cgm", "--=x", "--verbose"};
    EXPECT_EQ(expand(args), args);
}

TEST(ErrorMessages, SegmentRejectionNamesAWorkingRemedy)
{
    // WebCGM forbids segments. The message used to suggest --allow-segments
    // and --profile=iso8632, neither of which any tool accepts.
    CgmBuilder b;
    b.element(0, 6, B::i16(1));  // BEGIN SEGMENT
    b.element(4, 1, B::cat({B::i16(100), B::i16(100), B::i16(900), B::i16(900)}));
    b.element(0, 7, {});         // END SEGMENT
    const auto result = b.run("webcgm");

    ASSERT_NE(result.status, OPENCGM_OK);
    EXPECT_NE(result.error.find("--profile compat"), std::string::npos) << result.error;
    EXPECT_EQ(result.error.find("--allow-segments"), std::string::npos) << result.error;
    EXPECT_EQ(result.error.find("iso8632"), std::string::npos) << result.error;
}

TEST(ErrorMessages, CompatProfileConvertsWhatTheMessageSentThereFor)
{
    CgmBuilder b;
    b.element(0, 6, B::i16(1));
    b.element(4, 1, B::cat({B::i16(100), B::i16(100), B::i16(900), B::i16(900)}));
    b.element(0, 7, {});
    EXPECT_NE(b.convert("compat").find("<polyline"), std::string::npos);
}
