#include <gtest/gtest.h>
#include <boost/algorithm/string.hpp>

#include <sstream>

#include <clipper/logging.hpp>

using namespace clipper;

namespace {

class LoggingTest : public ::testing::Test {
 public:
  LoggingTest() : logger_(oss_) {}

  std::ostringstream oss_;
  Logger logger_;
};

TEST_F(LoggingTest, LogsContainSpecifiedTagAndMessage) {
  std::string tag1 = "TESTTAG";
  std::string msg1 = "TEST MESSAGE";

  logger_.log_error(tag1, msg1);

  std::string log_output1 = oss_.str();
  oss_.str("");

  std::string time_level1 = log_output1.substr(0, LOGGING_FORMAT_LENGTH);
  std::string log_msg1 = log_output1.substr(LOGGING_FORMAT_LENGTH);

  // Check that the specified tag is included in the resulting log output
  ASSERT_NE(log_msg1.find(tag1), std::string::npos);
  // Check that the specified message is included in the resulting log output
  ASSERT_NE(log_msg1.find(msg1), std::string::npos);
}

TEST_F(LoggingTest, LogContentPrecedingMessageHasConsistentLength) {
  std::string tag1 = "REDIS";
  std::string tag2 = "RPCSERVICE";
  std::string tag3 = "EXCESSIVELYLONGTAG";

  std::string msg = "This is a test message";

  logger_.log_info(tag1, msg);
  std::string log1 = oss_.str();
  oss_.str("");
  logger_.log_info(tag2, msg);
  std::string log2 = oss_.str();
  oss_.str("");
  logger_.log_info(tag3, msg);
  std::string log3 = oss_.str();
  oss_.str("");

  // Check that the predefined message is present
  // in each of the three log outputs at the
  // same index
  ASSERT_EQ(log1.find(msg), log2.find(msg));
  ASSERT_EQ(log2.find(msg), log3.find(msg));
}

TEST_F(LoggingTest, LogTagsExceedingMaxLengthTruncateCorrectly) {
  std::string tag = std::string(MAX_TAG_LENGTH + 1, 'a');
  std::string msg = "TESTMESSAGE";

  logger_.log_info(tag, msg);
  std::string log = oss_.str();

  std::string truncated_tag = tag.substr(0, MAX_TAG_LENGTH - 3) + "...";
  boost::to_upper(truncated_tag);

  // Check that a truncated tag of length "MAX_TAG_LENGTH" is present
  // within the log output
  ASSERT_NE(log.find(truncated_tag), std::string::npos);
}

TEST_F(LoggingTest, MultipleMessagesLogCorrectly) {
  std::string tag = "TESTTAG";
  std::string msg1 = "This is text";
  std::string msg2 = "This is different text";

  logger_.log_info(tag, msg1, msg2);
  std::string log = oss_.str();

  size_t pos_msg1 = log.find(msg1);
  size_t pos_msg2 = log.find(msg2);

  // Check that the second predefined message is positioned
  // after the first predefined message in the log output
  ASSERT_GE(pos_msg2, pos_msg1);
  std::string delimiting_newline = log.substr(pos_msg1 + msg1.length(), 1);
  // Check that a newline character is used to delimit the predefined
  // messages within the log output
  ASSERT_EQ(delimiting_newline, "\n");
  std::string padding =
      std::string(LOGGING_FORMAT_LENGTH + MAX_TAG_LENGTH, ' ');
  // Check that a required amount of padding exists between the predefined
  // messages
  // This ensures that the messages on separate lines are properly aligned
  ASSERT_NE(log.find(padding), std::string::npos);
}

TEST_F(LoggingTest, FormattedLogsFormatCorrectly) {
  std::string tag = "TESTTAG";
  std::string formatted_message = "The average of 10 and {} is {}";
  logger_.log_info_formatted(tag, formatted_message.data(), 5, 7.5);
  std::string expected_result = "The average of 10 and 5 is 7.5";
  std::string log = oss_.str();
  // Check that the correctly formatted message exists in the log output
  ASSERT_NE(log.find(expected_result), std::string::npos);
}

TEST_F(LoggingTest, LogsOfDifferentLevelsDifferByLevelTag) {
  std::string tag = "TESTTAG";
  std::string msg = "TEST MESSAGE";
  logger_.log_info(tag, msg);
  std::string info_log = oss_.str();
  oss_.str("");
  logger_.log_error(tag, msg);
  std::string error_log = oss_.str();
  oss_.str("");

  // The info level tag length is the length of the tag "INFO" + 2 characters
  // for the surrounding brackets. The level tag in its entirety is [INFO]
  size_t info_level_tag_length = LOGGING_LEVEL_FORMAT_INFO_LENGTH + 2;
  // The info level tag length is the length of the tag "ERROR" + 2 characters
  // for the surrounding brackets. The level tag in its entirety is [ERROR]
  size_t error_level_tag_length = LOGGING_LEVEL_FORMAT_ERROR_LENGTH + 2;

  size_t level_index = info_log.substr(1).find("[") + 1;
  // Obtains the text used to indicate that a log is at the "info" level
  std::string info_level_tag =
      info_log.substr(level_index, info_level_tag_length);
  // Obtains the text used to indicate that a log is at the "error" level
  std::string error_level_tag =
      error_log.substr(level_index, error_level_tag_length);

  // We expect the "info" and "error" level tags to differ
  ASSERT_NE(info_level_tag, error_level_tag);

  size_t info_message_index =
      level_index + info_log.substr(level_index + 1).find("[") + 1;
  size_t error_message_index =
      level_index + error_log.substr(level_index + 1).find("[") + 1;

  // However, we expect that the message content (everything after the level tag
  // and subsequent padding)
  // of the "info" log is equivalent to that of the "error" log
  ASSERT_EQ(info_log.substr(info_message_index),
            error_log.substr(error_message_index));
}

}  // namespace