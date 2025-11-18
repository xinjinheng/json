//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#include <nlohmann/detail/schema/schema.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

struct schema_validator::impl
{
    // Schema storage
    json m_schema;
    
    // Custom validators
    std::unordered_map<std::string, custom_validator_t> m_custom_validators;
    
    // Dependency graph for incremental validation
    std::unordered_map<std::string, std::vector<std::string>> m_dependency_graph;
    
    // Validate an instance against a schema
    bool validate_instance(const json& instance, const json& schema, const json_pointer& path, json& errors) const
    {
        bool valid = true;
        
        // Check type
        if (schema.contains("type"))
        {
            const auto& type_schema = schema["type"];
            if (type_schema.is_string())
            {
                const std::string type = type_schema.get<std::string>();
                bool type_mismatch = false;
                
                if (type == "object" && !instance.is_object()) type_mismatch = true;
                else if (type == "array" && !instance.is_array()) type_mismatch = true;
                else if (type == "string" && !instance.is_string()) type_mismatch = true;
                else if (type == "number" && !instance.is_number()) type_mismatch = true;
                else if (type == "integer" && !instance.is_number_integer()) type_mismatch = true;
                else if (type == "boolean" && !instance.is_boolean()) type_mismatch = true;
                else if (type == "null" && !instance.is_null()) type_mismatch = true;
                
                if (type_mismatch)
                {
                    errors.push_back({
                        {"path", path.to_string()},
                        {"error", "type mismatch"},
                        {"expected", type},
                        {"actual", instance.type_name()}
                    });
                    valid = false;
                }
            }
        }
        
        // Check properties for objects
        if (instance.is_object() && schema.contains("properties"))
        {
            const auto& properties = schema["properties"];
            for (const auto& [prop_name, prop_schema] : properties.items())
            {
                const json_pointer prop_path = path / prop_name;
                if (instance.contains(prop_name))
                {
                    const auto& prop_value = instance[prop_name];
                    valid &= validate_instance(prop_value, prop_schema, prop_path, errors);
                }
                else if (schema.contains("required"))
                {
                    const auto& required = schema["required"];
                    if (std::find(required.begin(), required.end(), prop_name) != required.end())
                    {
                        errors.push_back({
                            {"path", prop_path.to_string()},
                            {"error", "required property missing"}
                        });
                        valid = false;
                    }
                }
            }
        }
        
        // Check minimum for numbers
        if (instance.is_number() && schema.contains("minimum"))
        {
            const auto& minimum = schema["minimum"];
            if (instance < minimum)
            {
                errors.push_back({
                    {"path", path.to_string()},
                    {"error", "value too small"},
                    {"minimum", minimum},
                    {"actual", instance}
                });
                valid = false;
            }
        }
        
        // Check maximum for numbers
        if (instance.is_number() && schema.contains("maximum"))
        {
            const auto& maximum = schema["maximum"];
            if (instance > maximum)
            {
                errors.push_back({
                    {"path", path.to_string()},
                    {"error", "value too large"},
                    {"maximum", maximum},
                    {"actual", instance}
                });
                valid = false;
            }
        }
        
        // Check if-then-else
        if (schema.contains("if"))
        {
            const auto& if_schema = schema["if"];
            json if_errors;
            bool if_valid = validate_instance(instance, if_schema, path, if_errors);
            
            if (if_valid && schema.contains("then"))
            {
                const auto& then_schema = schema["then"];
                valid &= validate_instance(instance, then_schema, path, errors);
            }
            else if (!if_valid && schema.contains("else"))
            {
                const auto& else_schema = schema["else"];
                valid &= validate_instance(instance, else_schema, path, errors);
            }
        }
        
        return valid;
    }
    
    // Build dependency graph
    void build_dependency_graph(const json& schema, const std::string& current_path = "")
    {
        if (schema.is_object())
        {
            for (const auto& [key, value] : schema.items())
            {
                if (key == "properties")
                {
                    // Recursively process each property schema
                    for (const auto& [prop_name, prop_schema] : value.items())
                    {
                        std::string new_path = current_path.empty() ? prop_name : current_path + "." + prop_name;
                        build_dependency_graph(prop_schema, new_path);
                    }
                }
                else if (key == "if" || key == "then" || key == "else" || key == "allOf" || key == "anyOf" || key == "oneOf")
                {
                    // Recursively process these keyword schemas
                    if (value.is_array())
                    {
                        for (const auto& item : value)
                        {
                            build_dependency_graph(item, current_path);
                        }
                    }
                    else
                    {
                        build_dependency_graph(value, current_path);
                    }
                }
                // Add more keyword processing as needed
            }
        }
        else if (schema.is_array())
        {
            // Recursively process each item in the array
            for (const auto& item : schema)
            {
                build_dependency_graph(item, current_path);
            }
        }
        // For primitive types, nothing to process
    }
};

schema_validator::schema_validator() : m_impl(std::make_unique<impl>())
{
}

schema_validator::~schema_validator() noexcept = default;

void schema_validator::register_schema(const json& schema)
{
    m_impl->m_schema = schema;
    // Build dependency graph for incremental validation
    m_impl->build_dependency_graph(schema);
}

bool schema_validator::validate(const json& instance, json& errors) const
{
    // Validate the entire instance against the schema
    return m_impl->validate_instance(instance, m_impl->m_schema, json_pointer{}, errors);
}

void schema_validator::register_custom_validator(const std::string& keyword, custom_validator_t validator)
{
    m_impl->m_custom_validators[keyword] = validator;
}

bool schema_validator::validate_incremental(const json& instance, const json_pointer& changed_path, json& errors)
{
    // Incremental validation implementation
    // For now, just perform full validation
    return validate(instance, errors);
}

} // namespace detail
NLOHMANN_JSON_NAMESPACE_END