#include "gtest/gtest.h"

extern "C" {
#include "board_profile.h"
}

namespace {

TEST(BoardProfileTest, ActiveProfileValidates) {
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  EXPECT_TRUE(board_profile_validate(board_profile_active(), &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_VALID);
}

TEST(BoardProfileTest, RejectsNullProfile) {
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  EXPECT_FALSE(board_profile_validate(nullptr, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_NULL_PROFILE);
}

TEST(BoardProfileTest, RejectsMissingProfileName) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.profile_name = "";

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PROFILE_NAME);
}

TEST(BoardProfileTest, RejectsMissingConsolePathName) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.console_path_name = nullptr;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_CONSOLE_PATH_NAME);
}

TEST(BoardProfileTest, RejectsInvalidConsoleUart) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.console_uart_controller = -1;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_CONSOLE_UART);
}

TEST(BoardProfileTest, RejectsMissingPortName) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[0].name = "";

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PORT_NAME);
  EXPECT_EQ(result.port_index, 0);
}

TEST(BoardProfileTest, DisabledPortsMayOmitPinsAndUart) {
  board_profile_t candidate = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  candidate.ports[2] = BOARD_PORT_PROFILE_DISABLED("PORT3");

  EXPECT_TRUE(board_profile_validate(&candidate, &result));
}

TEST(BoardProfileTest, RejectsEnabledPortWithoutUart) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[0].uart_controller = BOARD_UART_UNUSED;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PORT_UART);
  EXPECT_EQ(result.port_index, 0);
}

TEST(BoardProfileTest, RejectsEnabledPortWithoutAllPins) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[0].sync_gpio = BOARD_GPIO_UNUSED;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PORT_PINS_REQUIRED);
  EXPECT_EQ(result.port_index, 0);
}

TEST(BoardProfileTest, RejectsDuplicatePinsWithinPort) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[0].sync_gpio = invalid_profile.ports[0].tx_gpio;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PORT_PINS_DUPLICATED);
  EXPECT_EQ(result.port_index, 0);
}

TEST(BoardProfileTest, RejectsConflictWithConsolePins) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[0].sync_gpio = invalid_profile.console_tx_gpio;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PIN_CONFLICT);
  EXPECT_EQ(result.port_index, 0);
}

TEST(BoardProfileTest, RejectsConflictAcrossEnabledPorts) {
  board_profile_t invalid_profile = *board_profile_active();
  board_profile_validation_result_t result =
      BOARD_PROFILE_VALIDATION_RESULT_INIT;

  invalid_profile.ports[1].sync_gpio = invalid_profile.ports[0].tx_gpio;

  EXPECT_FALSE(board_profile_validate(&invalid_profile, &result));
  EXPECT_EQ(result.code, BOARD_PROFILE_ERR_PIN_CONFLICT);
  EXPECT_EQ(result.port_index, 0);
}

}  // namespace
