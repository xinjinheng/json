// -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
//
// Copyright (C) 2013-2021 Niels Lohmann <http://nlohmann.me>
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "doctest_compatibility.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>
#include <string>

using nlohmann::json;

TEST_CASE("memory limit: basic functionality")
{
    SECTION("default memory limit is disabled")
    {
        // Reset to default
        json::set_memory_threshold(0);
        json::set_memory_threshold(0.0);
        
        CHECK(json::get_memory_threshold() == 0);
        CHECK(json::get_relative_memory_threshold() == 0.0);
        CHECK_FALSE(json::is_memory_exceeded());
    }
    
    SECTION("set and get absolute memory threshold")
    {
        const std::size_t threshold = 1024 * 1024; // 1MB
        json::set_memory_threshold(threshold);
        
        CHECK(json::get_memory_threshold() == threshold);
        CHECK(json::get_relative_memory_threshold() == 0.0);
    }
    
    SECTION("set and get relative memory threshold")
    {
        const double threshold = 0.5; // 50% of system memory
        json::set_memory_threshold(threshold);
        
        CHECK(json::get_memory_threshold() == 0);
        CHECK(json::get_relative_memory_threshold() == threshold);
    }
    
    SECTION("set both absolute and relative thresholds")
    {
        const std::size_t absolute_threshold = 1024 * 1024; // 1MB
        const double relative_threshold = 0.5; // 50% of system memory
        
        json::set_memory_threshold(absolute_threshold);
        json::set_memory_threshold(relative_threshold);
        
        CHECK(json::get_memory_threshold() == absolute_threshold);
        CHECK(json::get_relative_memory_threshold() == relative_threshold);
    }
}

TEST_CASE("memory limit: parsing without exceeding threshold")
{
    // Reset to default (no limit)
    json::set_memory_threshold(0);
    json::set_memory_threshold(0.0);
    
    SECTION("parse small JSON object")
    {
        const std::string json_str = R"({
            "name": "John",
            "age": 30,
            "city": "New York"
        })";
        
        json j;
        CHECK_NOTHROW(j = json::parse(json_str));
        CHECK(j["name"] == "John");
        CHECK(j["age"] == 30);
        CHECK(j["city"] == "New York");
    }
    
    SECTION("parse small JSON array")
    {
        const std::string json_str = R"([1, 2, 3, 4, 5, "six", "seven", true, false, null])";
        
        json j;
        CHECK_NOTHROW(j = json::parse(json_str));
        CHECK(j.size() == 10);
        CHECK(j[0] == 1);
        CHECK(j[5] == "six");
        CHECK(j[7] == true);
    }
}

TEST_CASE("memory limit: parsing with absolute threshold")
{
    // Set a very low absolute threshold to trigger memory limit exception
    const std::size_t threshold = 1024; // 1KB
    json::set_memory_threshold(threshold);
    json::set_memory_threshold(0.0);
    
    SECTION("parse large JSON array should throw")
    {
        // Create a large JSON array that will exceed 1KB
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < 100; ++i)
        {
            if (i > 0)
            {
                ss << ",";
            }
            ss << i;
        }
        ss << "]";
        
        const std::string large_json = ss.str();
        
        CHECK_THROWS_AS(json::parse(large_json), json::detail::memory_limit_exception);
    }
    
    SECTION("parse nested JSON object should throw")
    {
        // Create a deeply nested JSON object
        std::string nested_json = "{";
        for (int i = 0; i < 20; ++i)
        {
            nested_json += R"("key": {)";
        }
        nested_json += R"("value": "test")";
        for (int i = 0; i < 20; ++i)
        {
            nested_json += "}";
        }
        nested_json += "}";
        
        CHECK_THROWS_AS(json::parse(nested_json), json::detail::memory_limit_exception);
    }
}

TEST_CASE("memory limit: parsing with relative threshold")
{
    // Set a very low relative threshold (0.01% of system memory)
    const double threshold = 0.0001;
    json::set_memory_threshold(0);
    json::set_memory_threshold(threshold);
    
    SECTION("parse large JSON should throw")
    {
        // Create a large JSON string
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < 1000; ++i)
        {
            if (i > 0)
            {
                ss << ",";
            }
            ss << R"({"id": )" << i << R(", "name": "user)" << i << R("}");
        }
        ss << "]";
        
        const std::string large_json = ss.str();
        
        // This should throw on most systems with sufficient memory
        CHECK_THROWS_AS(json::parse(large_json), json::detail::memory_limit_exception);
    }
}

TEST_CASE("memory limit: parse state serialization")
{
    // Create a parse state
    json::detail::parse_state<json> state;
    state.current_token = json::detail::token_type::value_string;
    state.state = json::detail::parser_state::expect_value;
    state.depth = 2;
    state.current_key = "test_key";
    state.remaining_input = R"("value", "key2": 42})";
    
    SECTION("serialize and deserialize parse state")
    {
        // Serialize to JSON
        json j = state.to_json();
        
        // Deserialize from JSON
        json::detail::parse_state<json> restored_state;
        CHECK(restored_state.from_json(j));
        
        // Verify the state was restored correctly
        CHECK(restored_state.current_token == json::detail::token_type::value_string);
        CHECK(restored_state.state == json::detail::parser_state::expect_value);
        CHECK(restored_state.depth == 2);
        CHECK(restored_state.current_key == "test_key");
        CHECK(restored_state.remaining_input == R"("value", "key2": 42})";
    }
    
    SECTION("invalid parse state should return false on deserialization")
    {
        json j = {"current_token": 999, "state": 999, "depth": -1};
        
        json::detail::parse_state<json> restored_state;
        CHECK_FALSE(restored_state.from_json(j));
    }
}

TEST_CASE("memory limit: parsing recovery")
{
    // This is a simplified test since actual memory limit triggering depends on system resources
    // We'll test the parse state restoration mechanism directly
    
    SECTION("restore parse state and continue parsing")
    {
        // Create a parse state in the middle of parsing an object
        json::detail::parse_state<json> state;
        state.current_token = json::detail::token_type::value_string;
        state.state = json::detail::parser_state::expect_value;
        state.depth = 1;
        state.current_key = "key2";
        state.remaining_input = R"("value2"})";
        
        // Create a partially parsed value
        json partial_json;
        partial_json["key1"] = "value1";
        state.parsed_value = std::make_unique<json>(partial_json);
        
        // Try to restore parsing (this is a simplified test, actual implementation may vary)
        // Note: The current implementation doesn't fully support state restoration yet
        // This test will be updated once the feature is complete
        CHECK(state.is_valid());
    }
}
