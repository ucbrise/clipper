#include <gtest/gtest.h>
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
      {"model_name": "sklearn_svm", "model_version": 1},
      {"model_name": "sklearn_svm", "model_version": 2},
      {"model_name": "network", "model_version": 3}
    ]
  }
  )";
  std::string missing_name_json = R"(
      {"missing_name": [{"model_version": 3}]}
  )";
  std::string missing_version_json = R"(
      {"missing_version": [{"model_name": "m"}]}
  )";
  std::string wrong_name_type_json = R"(
      {"wrong_name_type": [{"model_name": 123, "model_version": 1}]}
  )";
  std::string wrong_version_type_json = R"(
  {
    "wrong_version_type": [
      {"model_name": "g", "model_version": "123"}
    ]
  }
  )";
  std::vector<clipper::VersionedModelId> expected_models = {
      {"sklearn_svm", 1}, {"sklearn_svm", 2}, {"network", 3}};

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
  EXPECT_EQ(get_double_array(nested_object, "double_array"), double_array);
  rapidjson::Value& twice_nested_object =
      get_object(nested_object, "twice_nested_object");
  EXPECT_EQ(get_double(twice_nested_object, "double_val"), double_val);
}

class SetJsonDocTest : public ::testing::Test {
 public:
  SetJsonDocTest() : redis_(std::make_shared<redox::Redox>()) {
    Config& conf = get_config();
    redis_->connect(conf.get_redis_address(), conf.get_redis_port());

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});
  }

  virtual ~SetJsonDocTest() { redis_->disconnect(); }

  std::shared_ptr<redox::Redox> redis_;
};

TEST_F(SetJsonDocTest, TestSetJsonDocFromRedisAppMetadata) {
  // Application data in redis storage format
  std::string input_type = "doubles";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  std::string selection_policy = DefaultOutputSelectionPolicy::get_name();
  std::vector<std::string> candidate_model_names =
      std::vector<std::string>{"m", "k"};

  add_application(*redis_, "myappname", candidate_model_names,
                  parse_input_type(input_type), selection_policy,
                  default_output, latency_slo_micros);
  std::unordered_map<std::string, std::string> app_metadata =
      get_application(*redis_, "myappname");

  rapidjson::Document d;
  set_json_doc_from_redis_app_metadata(d, app_metadata);

  EXPECT_EQ(get_string(d, "input_type"), input_type);
  EXPECT_EQ(get_string(d, "default_output"), default_output);
  EXPECT_EQ(get_int(d, "latency_slo_micros"), latency_slo_micros);
  EXPECT_EQ(get_string_array(d, "candidate_model_names"),
            candidate_model_names);
}
