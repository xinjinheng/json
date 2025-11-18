#include <iostream>
#include "single_include/nlohmann/json.hpp"
#include "single_include/nlohmann/json_schema.hpp"
#include "single_include/nlohmann/json_sync.hpp"
#include "single_include/nlohmann/json_compression.hpp"

using namespace nlohmann;

int main() {
    // Test JSON Schema validation
    std::cout << "1. Testing JSON Schema Validation...\n";
    
    try {
        // Create a schema
        json schema = {
            {"type", "object"},
            {"properties", {
                {"name", {"type", "string"}},
                {"age", {"type", "integer"}},
                {"email", {"type", "string"}}
            }},
            {"required", {"name", "age"}}
        };
        
        // Create a validator
        json_schema_validator validator;
        validator.register_schema(schema);
        
        // Create a valid JSON instance
        json instance = {
            {"name", "John Doe"},
            {"age", 30},
            {"email", "john@example.com"}
        };
        
        json errors;
        if (validator.validate(instance, errors)) {
            std::cout << "  Valid JSON instance!\n";
        } else {
            std::cout << "  Invalid JSON instance! Errors:\n" << errors.dump(2) << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "  Error: " << e.what() << std::endl;
    }
    
    // Test JSON Incremental Sync
    std::cout << "\n2. Testing JSON Incremental Sync...\n";
    
    try {
        json old_doc = {
            {"name", "John"},
            {"age", 30},
            {"city", "New York"}
        };
        
        json new_doc = {
            {"name", "John Doe"},
            {"age", 31},
            {"city", "New York"},
            {"email", "john@example.com"}
        };
        
        json_sync sync;
        json_patch_extended patch = sync.diff(old_doc, new_doc);
        
        std::cout << "  Generated patch:\n" << patch.dump(2) << std::endl;
        
        // Apply patch
        json patched_doc = old_doc;
        sync.apply_patch(patched_doc, patch);
        
        std::cout << "  Patched document:\n" << patched_doc.dump(2) << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "  Error: " << e.what() << std::endl;
    }
    
    // Test JSON Compression
    std::cout << "\n3. Testing JSON Compression...\n";
    
    try {
        json large_json = {
            {"name", "John Doe"},
            {"email", "john@example.com"},
            {"age", 30},
            {"city", "New York"}
        };
        
        compression_options options;
        options.level = compression_level_t::best;
        
        std::string compressed = compress_json(large_json, options);
        std::cout << "  Compressed size: " << compressed.size() << " bytes\n";
        
        json decompressed = decompress_json(compressed);
        std::cout << "  Decompressed data:\n" << decompressed.dump(2) << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "  Error: " << e.what() << std::endl;
    }
    
    return 0;
}