// -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
//
// Copyright (C) 2013-2021 Niels Lohmann <http://nlohmann.me>
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef NLOHMANN_DETAIL_PARSE_STATE_HPP
#define NLOHMANN_DETAIL_PARSE_STATE_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include "nlohmann/detail/input/lexer.hpp"
#include "nlohmann/detail/value_t.hpp"

// Forward declarations
template<typename BasicJsonType>
class parser;

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

// Parser state enum
enum class parser_state
{
    expect_value,
    expect_object_key,
    expect_object_value,
    expect_array_value,
    expect_colon,
    expect_comma_or_object_end,
    expect_comma_or_array_end,
    parse_error
};
/*!
@brief class to hold the state of the parser during parsing

This class is used to save the current state of the parser when a memory limit
is exceeded, allowing the parsing to be resumed later.
*/
template<typename BasicJsonType>
struct parse_state
{
    /*!
    @brief default constructor
    */
    parse_state() = default;

    /*!
    @brief constructor to create a parse state from a parser
    
    @param p the parser whose state should be saved
    */
    template<typename InputType>
    parse_state(const parser<BasicJsonType, InputType>& p)
        : current_token(p.last_token),
          state(p.state),
          depth(p.depth),
          is_errored(p.is_errored),
          error_message(p.error_message),
          error_pos(p.error_pos),
          current_key(p.current_key),
          input_position(p.m_lexer.input_position()),
          parsed_value(nullptr)
    {
        // Save the remaining input data
        std::stringstream ss;
        ss << p.m_lexer.remaining_input();
        remaining_input = ss.str();
    }

    /*!
    @brief destructor
    */
    ~parse_state() = default;

    /*!
    @brief copy constructor
    */
    parse_state(const parse_state&) = default;

    /*!
    @brief move constructor
    */
    parse_state(parse_state&&) noexcept = default;

    /*!
    @brief copy assignment operator
    */
    parse_state& operator=(const parse_state&) = default;

    /*!
    @brief move assignment operator
    */
    parse_state& operator=(parse_state&&) noexcept = default;

    // Current token type
    token_type current_token = token_type::uninitialized;

    // Current parser state
    parser_state state = parser_state::expect_value;

    // Current nesting depth
    std::size_t depth = 0;

    // Whether an error occurred
    bool is_errored = false;

    // Error message (if any)
    std::string error_message;

    // Error position (if any)
    std::size_t error_pos = 0;

    // Current key (for object parsing)
    std::string current_key;

    // Position in the input stream
    std::size_t input_position = 0;

    // Remaining input data to parse
    std::string remaining_input;

    // Partially parsed JSON value (if any)
    std::unique_ptr<BasicJsonType> parsed_value;

    /*!
    @brief check if the parse state is valid for resuming
    
    @return true if the state is valid for resuming, false otherwise
    */
    bool is_valid() const
    {
        return !remaining_input.empty() && !is_errored && state != parser_state::parse_error;
    }

    /*!
    @brief serialize the parse state to a JSON value
    
    @return a JSON value representing the parse state
    */
    BasicJsonType to_json() const
    {
        BasicJsonType j;
        j["current_token"] = static_cast<int>(current_token);
        j["state"] = static_cast<int>(state);
        j["depth"] = depth;
        j["is_errored"] = is_errored;
        j["error_message"] = error_message;
        j["error_pos"] = error_pos;
        j["current_key"] = current_key;
        j["input_position"] = input_position;
        j["remaining_input"] = remaining_input;
        if (parsed_value)
        {
            j["parsed_value"] = *parsed_value;
        }
        return j;
    }

    /*!
    @brief deserialize the parse state from a JSON value
    
    @param j the JSON value representing the parse state
    @return true if deserialization was successful, false otherwise
    */
    bool from_json(const BasicJsonType& j)
    {
        try
        {
            current_token = static_cast<token_type>(j.at("current_token").get<int>());
            state = static_cast<parser_state>(j.at("state").get<int>());
            depth = j.at("depth").get<std::size_t>();
            is_errored = j.at("is_errored").get<bool>();
            error_message = j.at("error_message").get<std::string>();
            error_pos = j.at("error_pos").get<std::size_t>();
            current_key = j.at("current_key").get<std::string>();
            input_position = j.at("input_position").get<std::size_t>();
            remaining_input = j.at("remaining_input").get<std::string>();
            if (j.contains("parsed_value"))
            {
                parsed_value = std::make_unique<BasicJsonType>(j.at("parsed_value"));
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
};

} // namespace detail
NLOHMANN_JSON_NAMESPACE_END

#endif // NLOHMANN_DETAIL_PARSE_STATE_HPP
