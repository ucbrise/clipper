#include <gtest/gtest.h>

#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>

using namespace clipper_json;

/* Test JSON serialization utilities */
TEST(JsonUtilTests, TestCorrectJsonValues) {
  std::string expected_json =
      "{\"string_val\":\"test_string\",\"double_val\":0.3,\"int_val\":-100}";
  std::string string_val = "test_string";
  double double_val = 0.3;
  int int_val = -100;

  rapidjson::Document d;
  d.SetObject();
  add_string(string_val, "string_val", d);
  add_double(double_val, "double_val", d);
  add_int(int_val, "int_val", d);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json); 
}

TEST(JsonUtilTests, TestCorrectJsonArrays) {
  std::string expected_json =
      "{\"string_array\":[\"test1\",\"word2\",\"phrase3\"],"
       "\"double_array\":[1.4,2.23,3.243242,0.3223424],"
       "\"int_array\":[1,2,3,4]}";
  std::vector<std::string> string_array = {"test1", "word2", "phrase3"};
  std::vector<double> double_array = {1.4,2.23,3.243242,0.3223424};
  std::vector<int> int_array = {1, 2, 3, 4};

  rapidjson::Document d;
  d.SetObject();
  add_string_array(string_array, "string_array", d);
  add_double_array(double_array, "double_array", d);
  add_int_array(int_array, "int_array", d);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json); 
}

TEST(JsonUtilTests, TestCorrectJsonNestedObjects) {
  std::string expected_json =
      "{\"nested_object\":{\"double_array\":[1.4,2.23,3.243242,0.3223424],"
                          "\"twice_nested_object\":{\"double_val\":0.3}}}";
  double double_val = 0.3;
  std::vector<double> double_array = {1.4,2.23,3.243242,0.3223424};

  rapidjson::Document main_doc;
  main_doc.SetObject();
  rapidjson::Document nested_object;
  nested_object.SetObject();
  rapidjson::Document twice_nested_object;
  twice_nested_object.SetObject();

  add_double(double_val, "double_val", twice_nested_object);
  add_double_array(double_array, "double_array", nested_object);
  add_object(twice_nested_object, "twice_nested_object", nested_object);
  add_object(nested_object, "nested_object", main_doc);
  std::string output_json = to_json_string(main_doc);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestOverwritePastValues) {
  std::string expected_json =
      "{\"double_val\":0.3,\"double_array\":[1.4,2.23,3.243242,0.3223424]}";

  rapidjson::Document d;
  d.SetObject();

  double old_double_val = 0.5;
  double new_double_val = 0.3;
  std::vector<double> old_double_array = {1.5,2.23,3.243242,0.3223424};
  std::vector<double> new_double_array = {1.4,2.23,3.243242,0.3223424};
  add_double(old_double_val, "double_val", d);
  add_double(new_double_val, "double_val", d);
  add_double_array(old_double_array, "double_array", d);
  add_double_array(new_double_array, "double_array", d);
  std::string output_json = to_json_string(d);
  EXPECT_EQ(output_json, expected_json);
}

TEST(JsonUtilTests, TestAddEmptyValues) {
  std::string expected_json =
      "{\"empty_string\":\"\",\"empty_int_array\":[]}";

  rapidjson::Document d;
  d.SetObject();

  add_string("", "empty_string", d);
  add_int_array({}, "empty_int_array", d);
}

/* Test JSON deserialization utilities */
TEST(JsonUtilTests, TestDoubleIntFormatting) {
  // Numbers require a decimal point to be a float, and cannot have a
  //  decimal point as an integer.
  std::string source_json =
      "{\"number_with_decimal\": 0.3, \"number_without_decimal\": 5}";
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
  std::string source_json =
      "{\"double_array_with_int\": [1.2, 3.4, 5, 6.7],"
       "\"int_array_with_string\": [1, 2, 3, \"4\"],"
       "\"string_array_with_bool\": [\"test1\", \"phrase2\", true]}";

  rapidjson::Document d;
  parse_json(source_json, d);
  ASSERT_THROW(
        get_double_array(d, "double_array_with_int"),
        json_semantic_error);
  ASSERT_THROW(
        get_int_array(d, "int_array_with_string"),
        json_semantic_error);
  ASSERT_THROW(
        get_string_array(d, "string_array_with_bool"),
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
  ASSERT_THROW(add_int(3, "int_val", d), json_semantic_error);
}

TEST(JsonUtilTests, TestParseCandidateModels) {
  // Semantic checking to assert that model_version > 0?
  std::string correct_json =
      "{\"correct_candidate_models\": ["
          "{\"model_name\": \"sklearn_svm\", \"model_version\": 1},"
          "{\"model_name\": \"sklearn_svm\", \"model_version\": 2},"
          "{\"model_name\": \"network\", \"model_version\": 3}"
       "]}";
  std::string missing_name_json =
      "{\"missing_name\": [{\"model_version\": 3}]}";
  std::string missing_version_json =
      "{\"missing_version\": [{\"model_name\": \"m\"}]}";
  std::string wrong_name_type_json =
      "{\"wrong_name_type\": [{\"model_name\": 123, \"model_version\": 1}]}";
  std::string wrong_version_type_json =
      "{\"wrong_version_type\": ["
          "{\"model_name\": \"g\", \"model_version\": \"123\"}"
       "]}";
  std::vector<clipper::VersionedModelId> expected_models =
      {{"sklearn_svm", 1}, {"sklearn_svm", 2}, {"network", 3}};

  rapidjson::Document d;
  parse_json(correct_json, d);
  EXPECT_EQ(
        get_candidate_models(d, "correct_candidate_models"),
        expected_models);

  parse_json(missing_name_json, d);
  ASSERT_THROW(get_candidate_models(d, "missing_name"), json_semantic_error);

  parse_json(missing_version_json, d);
  ASSERT_THROW(get_candidate_models(d, "missing_version"), json_semantic_error);

  parse_json(wrong_name_type_json, d);
  ASSERT_THROW(get_candidate_models(d, "wrong_name_type"), json_semantic_error);

  parse_json(wrong_version_type_json, d);
  ASSERT_THROW(
        get_candidate_models(d, "wrong_version_type"),
        json_semantic_error);
}

TEST(JsonUtilTests, TestParseNestedObject) {
  std::string source_json =
      "{\"nested_object\":{\"double_array\":[1.4,2.23,3.243242,0.3223424],"
                          "\"twice_nested_object\":{\"double_val\":0.3}}}";
  std::vector<double> double_array = {1.4,2.23,3.243242,0.3223424};
  double double_val = 0.3;

  rapidjson::Document d;
  parse_json(source_json, d);

  rapidjson::Value& nested_object = get_object(d, "nested_object");
  EXPECT_EQ(get_double_array(nested_object, "double_array"), double_array);
  rapidjson::Value& twice_nested_object =
      get_object(nested_object, "twice_nested_object");
  EXPECT_EQ(get_double(twice_nested_object, "double_val"), double_val);
}
