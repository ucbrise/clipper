// THIS IS A GTEST PORT OF THE TESTS FROM https://github.com/tkislan/base64/
#include <gtest/gtest.h>
#include <iostream>

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <assert.h>
#include <base64.h>
#include <string>

#include <clipper/json_util.hpp>

#ifndef TESTS
#define TESTS 10000
#endif

using namespace clipper;

namespace {

class Base64Test : public ::testing::Test {
 public:
  Base64Test() { srand(time(NULL)); }
};

void GenerateRandomString(std::string *string, size_t size) {
  string->resize(size);

  for (size_t i = 0; i < size; ++i) {
    (*string)[i] = static_cast<char>(rand() % 256);
  }
}

void GenerateRandomAlphaNumString(std::string *string, size_t size) {
  static const char kAlphaNum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  string->resize(size);

  for (size_t i = 0; i < size; ++i) {
    (*string)[i] = kAlphaNum[rand() % (sizeof(kAlphaNum) - 1)];
  }
}

long GenerateRandomNumber(long min, long max) {
  return rand() % (max - min) + min;
}

void TestBase64(const std::string &input, bool strip_padding = false) {
  static std::string encoded;
  static std::string decoded;

  ASSERT_TRUE(Base64::Encode(input, &encoded));

  if (strip_padding) Base64::StripPadding(&encoded);
  ASSERT_TRUE(Base64::Decode(encoded, &decoded));

  ASSERT_EQ(input, decoded);
}

void TestCBase64(const std::string &input, bool strip_padding = false) {
  static std::string encoded;
  static std::string decoded;

  encoded.resize(Base64::EncodedLength(input));
  ASSERT_TRUE(
      Base64::Encode(input.c_str(), input.size(), &encoded[0], encoded.size()));

  if (strip_padding) Base64::StripPadding(&encoded);
  decoded.resize(Base64::DecodedLength(encoded));

  ASSERT_TRUE(Base64::Decode(encoded.c_str(), encoded.size(), &decoded[0],
                             decoded.size()));
  ASSERT_EQ(input, decoded);
}

TEST_F(Base64Test, EncodingDecodingCorrectForAlphanumericStringsWithPadding) {
  std::string input;
  for (size_t i = 0; i < TESTS; ++i) {
    GenerateRandomAlphaNumString(&input, GenerateRandomNumber(100, 200));
    TestBase64(input);
    TestCBase64(input);
  }
}

TEST_F(Base64Test, EncodingDecodingCorrectForRandomAsciiStringsWithPadding) {
  std::string input;
  for (size_t i = 0; i < TESTS; ++i) {
    GenerateRandomString(&input, GenerateRandomNumber(100, 200));
    TestBase64(input);
    TestCBase64(input);
  }
}

TEST_F(Base64Test, EncodingDecodingCorrectForAlphanumericStringsNoPadding) {
  std::string input;
  for (size_t i = 0; i < TESTS; ++i) {
    GenerateRandomAlphaNumString(&input, GenerateRandomNumber(100, 200));
    TestBase64(input, true);
    TestCBase64(input, true);
  }
}

TEST_F(Base64Test, EncodingDecodingCorrectForRandomAsciiStringsNoPadding) {
  std::string input;
  for (size_t i = 0; i < TESTS; ++i) {
    GenerateRandomString(&input, GenerateRandomNumber(100, 200));
    TestBase64(input, true);
    TestCBase64(input, true);
  }
}

}  // namespace