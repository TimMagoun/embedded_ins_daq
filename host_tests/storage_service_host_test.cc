#include <unistd.h>

#include <array>
#include <filesystem>
#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "record_builder.h"
#include "runtime_config.h"
#include "storage_service.h"
}

namespace {

runtime_config_t SampleConfig(void) {
  runtime_config_t config = runtime_config_default();
  return config;
}

std::filesystem::path UniqueRoot(const char* test_name) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto unique =
      std::filesystem::path("embedded_ins_daq") / test_name /
      std::to_string(static_cast<unsigned long long>(::getpid()));
  return temp / unique;
}

TEST(StorageServiceTest, CreatesSessionFolderWithBinaryStatusAndConfigFiles) {
  const auto root = UniqueRoot("creates_session_folder");
  storage_service_t service = {};
  session_info_t session = {};
  runtime_config_t config = SampleConfig();
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
  ASSERT_EQ(storage_service_copy_config_snapshot(&service, &config), ESP_OK);

  EXPECT_TRUE(std::filesystem::exists(root / "session_000042"));
  EXPECT_TRUE(std::filesystem::exists(root / "session_000042" / "session.bin"));
  EXPECT_TRUE(std::filesystem::exists(root / "session_000042" / "status.log"));
  EXPECT_TRUE(std::filesystem::exists(root / "session_000042" / "config.txt"));

  std::filesystem::remove_all(root);
}

TEST(StorageServiceTest, SurfacesWriteFailureAsNormalizedFault) {
  const auto root = UniqueRoot("surfaces_write_failure");
  storage_service_t service = {};
  session_info_t session = {};
  fault_event_t fault = {};
  const std::array<uint8_t, 3> binary_bytes = {0xaa, 0xbb, 0xcc};

  session.session_id = 7U;
  session.start_timestamp_us = 777U;

  std::filesystem::remove_all(root);
  ASSERT_EQ(storage_service_init_for_host(&service, root.string().c_str()),
            ESP_OK);
  ASSERT_EQ(storage_service_mount(&service), ESP_OK);
  ASSERT_EQ(storage_service_open_session(&service, &session), ESP_OK);
  ASSERT_EQ(storage_service_force_next_write_failure_for_host(&service),
            ESP_OK);

  EXPECT_EQ(storage_service_write_binary_block(&service, binary_bytes.data(),
                                               binary_bytes.size()),
            ESP_FAIL);
  ASSERT_TRUE(storage_service_take_pending_fault(&service, &fault));
  EXPECT_EQ(fault.code, FAULT_CODE_STORAGE_IO);
  EXPECT_EQ(fault.severity, FAULT_SEVERITY_FATAL);

  std::filesystem::remove_all(root);
}

TEST(StorageServiceTest, WritesSyncEdgeModeIntoConfigSnapshot) {
  const auto root = UniqueRoot("writes_sync_edge_mode");
  storage_service_t service = {};
  session_info_t session = {};
  runtime_config_t config = SampleConfig();

  session.session_id = 99U;
  session.start_timestamp_us = 999U;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = SYNC_EDGE_CHANGE;

  std::filesystem::remove_all(root);

  ASSERT_EQ(storage_service_init_for_host(&service, root.string().c_str()),
            ESP_OK);
  ASSERT_EQ(storage_service_mount(&service), ESP_OK);
  ASSERT_EQ(storage_service_open_session(&service, &session), ESP_OK);
  ASSERT_EQ(storage_service_copy_config_snapshot(&service, &config), ESP_OK);

  const std::string snapshot =
      std::filesystem::path(root / "session_000099" / "config.txt").string();
  const std::string contents = [&snapshot]() {
    FILE* file = fopen(snapshot.c_str(), "rb");
    EXPECT_NE(file, nullptr);
    if (file == nullptr) {
      return std::string();
    }
    std::string data;
    char buffer[256] = {0};
    size_t bytes_read = 0U;
    while ((bytes_read = fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
      data.append(buffer, bytes_read);
    }
    fclose(file);
    return data;
  }();

  EXPECT_NE(contents.find("port1.sync_edge_mode=2"), std::string::npos);

  std::filesystem::remove_all(root);
}

}  // namespace
