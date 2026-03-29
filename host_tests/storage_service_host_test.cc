#include <unistd.h>

#include <array>
#include <filesystem>

#include "gtest/gtest.h"

extern "C" {
#include "record_builder.h"
#include "storage_service.h"
}

namespace {

std::filesystem::path UniqueRoot(const char* test_name) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto unique =
      std::filesystem::path("embedded_ins_daq") / test_name /
      std::to_string(static_cast<unsigned long long>(::getpid()));
  return temp / unique;
}

TEST(StorageServiceTest, CreatesSessionFolderWithBinaryAndStatusFiles) {
  const auto root = UniqueRoot("creates_session_folder");
  storage_service_t service = {};
  session_info_t session = {};
  std::array<uint8_t, 4> binary_bytes = {0x10, 0x20, 0x30, 0x40};

  session.session_id = 42U;
  session.start_timestamp_us = 123456U;

  std::filesystem::remove_all(root);

  ASSERT_EQ(storage_service_init_for_host(&service, root.string().c_str()),
            ESP_OK);
  ASSERT_EQ(storage_service_mount(&service), ESP_OK);
  ASSERT_EQ(storage_service_open_session(&service, &session), ESP_OK);
  ASSERT_EQ(storage_service_write_binary_block(&service, binary_bytes.data(),
                                               binary_bytes.size()),
            ESP_OK);
  ASSERT_EQ(storage_service_write_status_message(&service, "recording"),
            ESP_OK);

  EXPECT_TRUE(std::filesystem::exists(root / "session_000042"));
  EXPECT_TRUE(std::filesystem::exists(root / "session_000042" / "session.bin"));
  EXPECT_TRUE(std::filesystem::exists(root / "session_000042" / "status.log"));

  std::filesystem::remove_all(root);
}

}  // namespace
