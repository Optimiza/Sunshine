/**
 * @file tests/unit/test_config.cpp
 * @brief Tests for Sunshine configuration persistence helpers.
 */

// test includes
#include "../tests_common.h"

// standard includes
#include <filesystem>
#include <string>
#include <utility>

// local includes
#include <src/config.h>
#include <src/file_handler.h>

using namespace std::literals;

using NvencPresetNameParam = std::pair<int, std::string_view>;

/**
 * @brief Parameterized coverage for FFmpeg's symbolic NVENC preset names.
 */
struct NvencPresetNameTest: testing::TestWithParam<NvencPresetNameParam> {};

TEST_P(NvencPresetNameTest, UsesStableSymbolicName) {
  const auto &[quality_preset, expected] = GetParam();

  EXPECT_EQ(expected, config::nv::ffmpeg_preset_from_quality(quality_preset));
}

INSTANTIATE_TEST_SUITE_P(
  NvencQualityPresets,
  NvencPresetNameTest,
  testing::Values(
    NvencPresetNameParam {1, "p1"sv},
    NvencPresetNameParam {2, "p2"sv},
    NvencPresetNameParam {3, "p3"sv},
    NvencPresetNameParam {4, "p4"sv},
    NvencPresetNameParam {5, "p5"sv},
    NvencPresetNameParam {6, "p6"sv},
    NvencPresetNameParam {7, "p7"sv}
  )
);

namespace {

  /**
   * @brief Fixture that redirects configuration persistence to a temporary file.
   */
  class ConfigPersistenceTest: public testing::Test {
  protected:
    /**
     * @brief Preserve the active config path and prepare an empty temporary path.
     */
    void SetUp() override {
      original_config_file_ = config::sunshine.config_file;
      original_gamepad_driver_ = config::input.gamepad_driver;
      config_file_ = std::filesystem::temp_directory_path() / "sunshine_test_config_persistence.conf";  // NOSONAR(cpp:S5443): safe for tests
      std::filesystem::remove(config_file_);
      config::sunshine.config_file = config_file_.string();
      config::input.gamepad_driver.clear();
    }

    /**
     * @brief Remove the temporary file and restore the active config path.
     */
    void TearDown() override {
      config::sunshine.config_file = std::move(original_config_file_);
      config::input.gamepad_driver = std::move(original_gamepad_driver_);
      std::filesystem::remove(config_file_);
    }

    /**
     * @brief Get the temporary configuration file used by the test.
     *
     * @return Temporary configuration file path.
     */
    const std::filesystem::path &config_file() const {
      return config_file_;
    }

  private:
    std::filesystem::path config_file_;  ///< Temporary configuration file used by the test.
    std::string original_config_file_;  ///< Active configuration path restored after each test.
    std::string original_gamepad_driver_;  ///< Gamepad driver preference restored after each test.
  };

}  // namespace

TEST_F(ConfigPersistenceTest, AppendsMissingOptionWithoutReplacingExistingText) {
  ASSERT_EQ(file_handler::write_file(config_file().string().c_str(), "# retained comment"), 0);

  EXPECT_TRUE(config::persist_config_option_if_missing("gamepad_driver", "all"));
  EXPECT_EQ(
    file_handler::read_file(config_file().string().c_str()),
    "# retained comment\ngamepad_driver = all\n"
  );

  EXPECT_FALSE(config::persist_config_option_if_missing("gamepad_driver", "virtualhid"));
  EXPECT_EQ(
    file_handler::read_file(config_file().string().c_str()),
    "# retained comment\ngamepad_driver = all\n"
  );
}

TEST_F(ConfigPersistenceTest, DoesNotReplaceAnExistingOption) {
  ASSERT_EQ(file_handler::write_file(config_file().string().c_str(), "gamepad_driver = vigembus\n"), 0);

  EXPECT_FALSE(config::persist_config_option_if_missing("gamepad_driver", "all"));
  EXPECT_EQ(file_handler::read_file(config_file().string().c_str()), "gamepad_driver = vigembus\n");
}

TEST_F(ConfigPersistenceTest, SelectsAllDriversOnlyForLicensedUsersWithoutAPreference) {
  EXPECT_FALSE(config::select_all_gamepad_drivers_if_licensed(false));
  EXPECT_TRUE(config::input.gamepad_driver.empty());
  EXPECT_TRUE(file_handler::read_file(config_file().string().c_str()).empty());

  EXPECT_TRUE(config::select_all_gamepad_drivers_if_licensed(true));
  EXPECT_EQ(config::input.gamepad_driver, config::GAMEPAD_DRIVER_ALL);
  EXPECT_EQ(file_handler::read_file(config_file().string().c_str()), "gamepad_driver = all\n");

  EXPECT_FALSE(config::select_all_gamepad_drivers_if_licensed(true));
  EXPECT_EQ(file_handler::read_file(config_file().string().c_str()), "gamepad_driver = all\n");
}

namespace {

  /**
   * @brief Fixture that restores every configuration struct a parse can change, and points the apps file at one that exists.
   */
  class ConfigGamepadKeyTest: public testing::Test {
  protected:
    /**
     * @brief Leave the configured gamepad at its default and keep the parse away from the user's apps file.
     */
    void SetUp() override {
      config::stream.file_apps = SUNSHINE_SOURCE_DIR "/tests/unit/test_config.cpp";
    }

    /**
     * @brief Restore the configuration the parse changed.
     */
    void TearDown() override {
      config::video = original_video_;
      config::audio = original_audio_;
      config::stream = original_stream_;
      config::nvhttp = original_nvhttp_;
      config::input = original_input_;
      config::sunshine = original_sunshine_;
      config::modified_config_settings = original_modified_config_settings_;
    }

  private:
    config::video_t original_video_ {config::video};  ///< Video configuration restored after each test.
    config::audio_t original_audio_ {config::audio};  ///< Audio configuration restored after each test.
    config::stream_t original_stream_ {config::stream};  ///< Stream configuration restored after each test.
    config::nvhttp_t original_nvhttp_ {config::nvhttp};  ///< HTTP configuration restored after each test.
    config::input_t original_input_ {config::input};  ///< Input configuration restored after each test.
    config::sunshine_t original_sunshine_ {config::sunshine};  ///< Core configuration restored after each test.
    decltype(config::modified_config_settings) original_modified_config_settings_ {config::modified_config_settings};  ///< Modified settings restored after each test.
  };

}  // namespace

TEST_F(ConfigGamepadKeyTest, DefaultsToAuto) {
  config::apply_config_for_test("");

  EXPECT_EQ(config::input.gamepad, "auto");
}

TEST_F(ConfigGamepadKeyTest, AcceptsDualSense) {
  config::apply_config_for_test("gamepad = ds5\n");

  EXPECT_EQ(config::input.gamepad, "ds5");
}

TEST_F(ConfigGamepadKeyTest, AcceptsSwitchPro) {
  config::apply_config_for_test("gamepad = switch\n");

  EXPECT_EQ(config::input.gamepad, "switch");
}

TEST_F(ConfigGamepadKeyTest, AcceptsAuto) {
  config::apply_config_for_test("gamepad = auto\n");

  EXPECT_EQ(config::input.gamepad, "auto");
}

TEST_F(ConfigGamepadKeyTest, IgnoresUnknownValue) {
  config::apply_config_for_test("gamepad = bogus\n");

  EXPECT_EQ(config::input.gamepad, "auto");
}

namespace {

  /**
   * @brief Runs the gamepad key fixture's setup and teardown around a parse, outside a test.
   */
  class ConfigGamepadKeyFixtureProbe: public ConfigGamepadKeyTest {
  public:
    /**
     * @brief Parse @p file_content between the fixture's setup and teardown.
     *
     * @param file_content Configuration text to apply.
     */
    void parse(std::string_view file_content) {
      SetUp();
      config::apply_config_for_test(file_content);
      TearDown();
    }

    /**
     * @brief Unused: the probe only drives the fixture.
     */
    void TestBody() override {}
  };

}  // namespace

TEST(ConfigGamepadKeyFixtureTest, RestoresWhatAParseChanges) {
  const auto original = config::sunshine.csrf_allowed_origins;
  config::sunshine.csrf_allowed_origins = {"https://example.test"};

  ConfigGamepadKeyFixtureProbe {}.parse("gamepad = ds5\n");

  EXPECT_EQ(config::sunshine.csrf_allowed_origins, std::vector<std::string> {"https://example.test"});
  config::sunshine.csrf_allowed_origins = original;
}
