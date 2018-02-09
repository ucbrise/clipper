#include <gtest/gtest.h>

#include <base64.h>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/redis.hpp>
#include <clipper/selection_policies.hpp>

using namespace clipper;
using namespace clipper::json;
using namespace clipper::redis;

/* Test JSON serialization utilities
 * Note:
 *   RapidJSON output strings do not contain any whitespace or newlines
 *   Expected JSON strings spanning multiple lines are formatted using
 *    multiple string literals to avoid introducing newlines */
TEST(JsonUtilTests, TestCorrectJsonValues) {
  std::string expected_json =
      R"({"string_val":"test_string","double_val":0.3,"int_val":-100})";
  std::string string_val = "test_string";
  double double_val = 0.3;
  int int_val = -100;

  rapidjson::Document d;
  d.SetObject();
  add_string(d, "string_val", string_val);
  add_double(d, "double_val", double_val);
  add_int(d, "int_val", int_val);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestSetStringArrayCorrect) {
  const std::vector<std::string> str_array = {"a", "b"};
  rapidjson::Document d;
  set_string_array(d, str_array);
  std::string expected_json = R"(["a","b"])";
  EXPECT_EQ(to_json_string(d), expected_json);
}

TEST(JsonUtilTests, TestToStringArrayCorrect) {
  rapidjson::Document d;
  rapidjson::Document::AllocatorType& a = d.GetAllocator();
  d.SetArray();
  d.PushBack("a", a);
  d.PushBack("b", a);
  auto result = to_string_array(d);
  std::vector<std::string> expected_result = {"a", "b"};
  EXPECT_EQ(expected_result, result);
}

TEST(JsonUtilTests, TestCorrectJsonArrays) {
  std::string expected_json =
      R"({"string_array":["test1","word2","phrase3"],)"
      R"("double_array":[1.4,2.23,3.243242,0.3223424],)"
      R"("int_array":[1,2,3,4]})";
  std::vector<std::string> string_array = {"test1", "word2", "phrase3"};
  std::vector<double> double_array = {1.4, 2.23, 3.243242, 0.3223424};
  std::vector<int> int_array = {1, 2, 3, 4};

  rapidjson::Document d;
  d.SetObject();
  add_string_array(d, "string_array", string_array);
  add_double_array(d, "double_array", double_array);
  add_int_array(d, "int_array", int_array);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestCorrectJsonNestedObjects) {
  std::string expected_json =
      R"({"nested_object":{"double_array":[1.4,2.23,3.243242,0.3223424],)"
      R"("twice_nested_object":{"double_val":0.3}}})";
  double double_val = 0.3;
  std::vector<double> double_array = {1.4, 2.23, 3.243242, 0.3223424};

  rapidjson::Document main_doc;
  main_doc.SetObject();
  rapidjson::Document nested_object;
  nested_object.SetObject();
  rapidjson::Document twice_nested_object;
  twice_nested_object.SetObject();

  add_double(twice_nested_object, "double_val", double_val);
  add_double_array(nested_object, "double_array", double_array);
  add_object(nested_object, "twice_nested_object", twice_nested_object);
  add_object(main_doc, "nested_object", nested_object);
  std::string output_json = to_json_string(main_doc);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestOverwritePastValues) {
  std::string expected_json =
      R"({"double_val":0.3,"double_array":[1.4,2.23,3.243242,0.3223424]})";

  rapidjson::Document d;
  d.SetObject();

  double old_double_val = 0.5;
  double new_double_val = 0.3;
  std::vector<double> old_double_array = {1.5, 2.23, 3.243242, 0.3223424};
  std::vector<double> new_double_array = {1.4, 2.23, 3.243242, 0.3223424};
  add_double(d, "double_val", old_double_val);
  add_double(d, "double_val", new_double_val);
  add_double_array(d, "double_array", old_double_array);
  add_double_array(d, "double_array", new_double_array);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestAddEmptyValues) {
  std::string expected_json =
      R"({"empty_string":"","empty_int_array":[]})";

  rapidjson::Document d;
  d.SetObject();

  std::vector<int> empty_int_array = {};
  add_string(d, "empty_string", "");
  add_int_array(d, "empty_int_array", empty_int_array);
}

/* Test JSON deserialization utilities */
TEST(JsonUtilTests, TestDoubleIntFormatting) {
  // Numbers require a decimal point to be a float, and cannot have a
  //  decimal point as an integer.
  std::string source_json = R"(
      {"number_with_decimal": 0.3, "number_without_decimal": 5}
  )";
  rapidjson::Document d;
  parse_json(source_json, d);
  EXPECT_EQ(get_double(d, "number_with_decimal"), 0.3);
  ASSERT_THROW(get_int(d, "number_with_decimal"), json_semantic_error);
  ASSERT_THROW(get_double(d, "number_without_decimal"), json_semantic_error);
  EXPECT_EQ(get_int(d, "number_without_decimal"), 5);
}

TEST(JsonUtilTests, TestArrayTypeMismatch) {
  // JSON arrays may contain items of different types so the array
  //  parsing methods should throw an exception if this is the case.
  std::string source_json = R"(
  {
    "double_array_with_int": [1.2, 3.4, 5, 6.7],
    "int_array_with_string": [1, 2, 3, "4"],
    "string_array_with_bool": ["test1", "phrase2", true]
  }
  )";

  rapidjson::Document d;
  parse_json(source_json, d);
  ASSERT_THROW(get_double_array(d, "double_array_with_int"),
               json_semantic_error);
  ASSERT_THROW(get_int_array(d, "int_array_with_string"), json_semantic_error);
  ASSERT_THROW(get_string_array(d, "string_array_with_bool"),
               json_semantic_error);
}

TEST(JsonUtilTests, TestEmptyDocumentToString) {
  rapidjson::Document d;
  EXPECT_EQ("null", to_json_string(d));
  d.SetObject();
  EXPECT_EQ("{}", to_json_string(d));
  d.SetArray();
  EXPECT_EQ("[]", to_json_string(d));
  d.SetDouble(0.3);
  EXPECT_EQ("0.3", to_json_string(d));
  d.SetBool(true);
  EXPECT_EQ("true", to_json_string(d));
}

TEST(JsonUtilTests, TestAddToNonObject) {
  rapidjson::Document d;
  d.SetArray();
  ASSERT_THROW(add_int(d, "int_val", 3), json_semantic_error);
}

TEST(JsonUtilTests, TestParseCandidateModels) {
  std::string correct_json = R"(
  {
    "correct_candidate_models": [
      {"model_name": "sklearn_svm", "model_version": "92248e3"},
      {"model_name": "sklearn_svm", "model_version": "1.2.4"},
      {"model_name": "network", "model_version": "3--0"}
    ]
  }
  )";
  std::string missing_name_json = R"(
      {"missing_name": [{"model_version": "3"}]}
  )";
  std::string missing_version_json = R"(
      {"missing_version": [{"model_name": "m"}]}
  )";
  std::string wrong_name_type_json = R"(
      {"wrong_name_type": [{"model_name": 123, "model_version": "1"}]}
  )";
  std::string wrong_version_type_json = R"(
  {
    "wrong_version_type": [
      {"model_name": "g", "model_version": 123}
    ]
  }
  )";
  std::vector<clipper::VersionedModelId> expected_models = {
      {"sklearn_svm", "92248e3"},
      {"sklearn_svm", "1.2.4"},
      {"network", "3--0"}};

  rapidjson::Document d;
  parse_json(correct_json, d);
  EXPECT_EQ(get_candidate_models(d, "correct_candidate_models"),
            expected_models);

  parse_json(missing_name_json, d);
  ASSERT_THROW(get_candidate_models(d, "missing_name"), json_semantic_error);

  parse_json(missing_version_json, d);
  ASSERT_THROW(get_candidate_models(d, "missing_version"), json_semantic_error);

  parse_json(wrong_name_type_json, d);
  ASSERT_THROW(get_candidate_models(d, "wrong_name_type"), json_semantic_error);

  parse_json(wrong_version_type_json, d);
  ASSERT_THROW(get_candidate_models(d, "wrong_version_type"),
               json_semantic_error);
}

TEST(JsonUtilTests, TestParseNestedObject) {
  std::string source_json = R"(
  {
    "nested_object": {
        "double_array": [1.4,2.23,3.243242,0.3223424],
        "twice_nested_object": {"double_val":0.3}
    }
  }
  )";
  std::vector<double> double_array = {1.4, 2.23, 3.243242, 0.3223424};
  double double_val = 0.3;

  rapidjson::Document d;
  parse_json(source_json, d);

  rapidjson::Value& nested_object = get_object(d, "nested_object");
  auto parsed_array_content = get_double_array(nested_object, "double_array");
  UniquePoolPtr<double>& parsed_array_data = parsed_array_content.first;
  size_t parsed_array_size = parsed_array_content.second;
  ASSERT_EQ(parsed_array_size, double_array.size());
  for (size_t i = 0; i < parsed_array_size; ++i) {
    EXPECT_EQ(parsed_array_data.get()[i], double_array[i]);
  }
  rapidjson::Value& twice_nested_object =
      get_object(nested_object, "twice_nested_object");
  EXPECT_EQ(get_double(twice_nested_object, "double_val"), double_val);
}

TEST(JsonUtilTests, TestAddStringPreservesJsonIntegrity) {
  rapidjson::Document d;
  d.SetObject();
  // If these strings are not escaped, they will invalidate
  // the JSON format
  json::add_string(d, "}}}}", "[[[[[");
  std::string json_content = json::to_json_string(d);
  // Attempt to parse the output string. We should see that
  // it is still a valid JSON object
  ASSERT_NO_THROW(json::parse_json(json_content, d));
}

TEST(JsonUtilTests, TestBase64DecodingYieldsOriginalString) {
  std::string raw_string = "base64 decoded";
  std::string encoded_string;
  Base64 encoder_decoder;
  encoder_decoder.Encode(raw_string, &encoded_string);
  rapidjson::Document d;
  d.SetObject();
  std::string key_name = "test_key";
  json::add_string(d, key_name.data(), encoded_string);
  auto decoded_byte_data =
      json::get_base64_encoded_byte_array(d, key_name.data());
  UniquePoolPtr<uint8_t>& decoded_bytes = decoded_byte_data.first;
  size_t decoded_bytes_size = decoded_byte_data.second;
  char* decoded_content = reinterpret_cast<char*>(decoded_bytes.get());
  std::string decoded_string(decoded_content,
                             decoded_content + decoded_bytes_size);
  ASSERT_EQ(raw_string, decoded_string);
}

class RedisToJsonTest : public ::testing::Test {
 public:
  RedisToJsonTest() : redis_(std::make_shared<redox::Redox>()) {
    Config& conf = get_config();
    redis_->connect(conf.get_redis_address(), conf.get_redis_port());

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});
  }

  virtual ~RedisToJsonTest() { redis_->disconnect(); }

  std::shared_ptr<redox::Redox> redis_;
};

TEST_F(RedisToJsonTest, TestRedisAppMetadataToJson) {
  // Application data in redis storage format
  std::string input_type = "doubles";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  std::string selection_policy = DefaultOutputSelectionPolicy::get_name();

  ASSERT_TRUE(add_application(*redis_, "myappname",
                              parse_input_type(input_type), selection_policy,
                              default_output, latency_slo_micros));
  std::unordered_map<std::string, std::string> app_metadata =
      get_application(*redis_, "myappname");

  rapidjson::Document d;
  redis_app_metadata_to_json(d, app_metadata);

  ASSERT_EQ(get_string(d, "input_type"), input_type);
  ASSERT_EQ(get_string(d, "default_output"), default_output);
  ASSERT_EQ(get_int(d, "latency_slo_micros"), latency_slo_micros);
}

TEST_F(RedisToJsonTest, TestRedisModelMetadataToJson) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model = VersionedModelId("m", "1");
  std::string input_type = "doubles";
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, parse_input_type(input_type), labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));

  std::unordered_map<std::string, std::string> model_metadata =
      get_model(*redis_, model);

  rapidjson::Document d;
  redis_model_metadata_to_json(d, model_metadata);

  ASSERT_EQ(get_string(d, "input_type"), input_type);
  ASSERT_EQ(get_string(d, "model_name"), model.get_name());
  ASSERT_EQ(get_string(d, "model_version"), model.get_id());
  ASSERT_EQ(get_string(d, "input_type"), input_type);
  ASSERT_EQ(get_string_array(d, "labels"), labels);
  ASSERT_EQ(get_string(d, "container_name"), container_name);
  ASSERT_EQ(get_string(d, "model_data_path"), model_path);
}

TEST_F(RedisToJsonTest, TestRedisContainerMetadataToJson) {
  VersionedModelId model = VersionedModelId("m", "1");
  int replica_id = 4;
  int zmq_connection_id = 12;
  std::string input_type = "doubles";
  ASSERT_TRUE(add_container(*redis_, model, replica_id, zmq_connection_id,
                            parse_input_type(input_type)));
  std::unordered_map<std::string, std::string> container_metadata =
      get_container(*redis_, model, replica_id);

  rapidjson::Document d;
  redis_container_metadata_to_json(d, container_metadata);

  ASSERT_EQ(get_string(d, "model_name"), model.get_name());
  ASSERT_EQ(get_string(d, "model_version"), model.get_id());
  ASSERT_EQ(get_string(d, "input_type"), input_type);
  ASSERT_EQ(get_int(d, "model_replica_id"), replica_id);
  ASSERT_EQ(get_string(d, "model_id"), gen_versioned_model_key(model));
}
