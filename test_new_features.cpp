#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_schema.hpp>
#include <nlohmann/json_sync.hpp>
#include <nlohmann/json_compression.hpp>

using namespace nlohmann;

int main()
{
    std::cout << "Testing new JSON features...\n";
    
    // Test 1: JSON Schema validator
    std::cout << "1. Testing JSON Schema validator...\n";
    json_schema_validator validator;
    json schema = {
        {"type", "object"},
        {"properties", {
            {"name", {"type", "string"}},
            {"age", {"type", "integer"}, {"minimum", 0}, {"maximum", 150}}
        }},
        {"required", {"name"}}
    };
    try {
        validator.register_schema(schema);
        json instance = {{"name", "John"}, {"age", 30}};
        json errors;
        if (validator.validate(instance, errors)) {
            std::cout << "   Instance is valid!\n";
        } else {
            std::cout << "   Instance is invalid: " << errors.dump() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "   Error: " << e.what() << std::endl;
    }
    
    // Test 2: JSON Sync
    std::cout << "\n2. Testing JSON Sync...\n";
    json old_doc = {{"name", "John"}, {"age", 30}};
    json new_doc = {{"name", "John"}, {"age", 31}};
    json_sync sync;
    json_patch_extended patch = sync.diff(old_doc, new_doc);
    std::cout << "   Generated patch: " << patch.dump() << std::endl;
    
    // Test 3: JSON Compression
    std::cout << "\n3. Testing JSON Compression...\n";
    json large_json = {{"name", "John Doe"}, {"email", "john@example.com"}, {"age", 30}, {"city", "New York"}};
    compression_options options;
    options.level = compression_level_t::best;
    try {
        std::vector<std::uint8_t> compressed = compress_json(large_json, options);
        json decompressed = decompress_json(compressed);
        std::cout << "   Compression successful! Original size: " << large_json.dump().size() << " bytes, compressed size: " << compressed.size() << " bytes\n";
        std::cout << "   Decompressed JSON: " << decompressed.dump() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "   Error: " << e.what() << std::endl;
    }
    
    std::cout << "\nAll tests completed!\n";
    return 0;
}