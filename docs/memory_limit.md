# Memory Limit and Parsing Interruption

The nlohmann/json library now supports memory usage monitoring during JSON parsing, allowing you to set memory thresholds and handle cases where parsing would exceed available memory resources.

## Features

- **Memory Threshold Setting**: Set absolute memory limits (in bytes) or relative limits (as a percentage of total system memory)
- **Memory Usage Monitoring**: Real-time monitoring of memory usage during parsing
- **Parsing Interruption**: Automatic interruption of parsing when memory limits are exceeded
- **Parse State Saving**: Save the current parsing state when an interruption occurs
- **Parsing Recovery**: Resume parsing from a saved state when memory becomes available

## Usage

### Setting Memory Thresholds

You can set memory thresholds either as an absolute value (in bytes) or as a relative value (as a percentage of total system memory).

```cpp
#include <nlohmann/json.hpp>

using nlohmann::json;

// Set absolute memory threshold (100MB)
json::set_memory_threshold(100 * 1024 * 1024);

// Set relative memory threshold (50% of system memory)
json::set_memory_threshold(0.5);

// Set both thresholds (whichever is reached first will trigger interruption)
json::set_memory_threshold(50 * 1024 * 1024);  // 50MB absolute
json::set_memory_threshold(0.3);               // 30% relative
```

### Getting Current Thresholds

```cpp
// Get absolute memory threshold
std::size_t absolute_threshold = json::get_memory_threshold();

// Get relative memory threshold
double relative_threshold = json::get_relative_memory_threshold();
```

### Checking Memory Usage

```cpp
// Check if current memory usage exceeds thresholds
bool exceeded = json::is_memory_exceeded();

// Get current memory usage in bytes
std::size_t current_usage = json::get_current_memory_usage();

// Get total system memory in bytes
std::size_t total_memory = json::get_total_system_memory();
```

### Handling Memory Limit Exceptions

When parsing exceeds the memory threshold, a `json::detail::memory_limit_exception` is thrown. This exception contains information about the current memory usage, threshold, and optionally the parse state.

```cpp
try
{
    json j = json::parse(large_json_string);
}
catch (const json::detail::memory_limit_exception& ex)
{
    std::cerr << "Memory limit exceeded!" << std::endl;
    std::cerr << "Current usage: " << ex.current_memory << " bytes" << std::endl;
    std::cerr << "Threshold: " << ex.memory_threshold << " bytes" << std::endl;
    
    // Check if parse state is available for recovery
    auto parse_state = ex.get_parse_state<json>();
    if (parse_state)
    {
        // Save the parse state for later recovery
        save_parse_state_to_disk(*parse_state);
    }
}
```

### Recovering from Interrupted Parsing

Once memory becomes available, you can resume parsing from a saved parse state:

```cpp
// Load saved parse state from disk
json::detail::parse_state<json> saved_state = load_parse_state_from_disk();

if (saved_state.is_valid())
{
    try
    {
        // Resume parsing from the saved state
        json j = json::from_json_with_state(saved_state);
        
        // Use the fully parsed JSON
        process_json(j);
    }
    catch (const json::detail::parse_error& ex)
    {
        std::cerr << "Failed to resume parsing: " << ex.what() << std::endl;
    }
}
```

You can also resume parsing directly from the exception object:

```cpp
try
{
    json j = json::parse(large_json_string);
}
catch (const json::detail::memory_limit_exception& ex)
{
    // Free up some memory
    free_memory();
    
    try
    {
        // Resume parsing from the exception's parse state
        json j = json::from_json_with_state(ex);
        process_json(j);
    }
    catch (...)
    {
        // Handle recovery failure
    }
}
```

### Resetting Thresholds

To disable memory monitoring, set both thresholds to zero:

```cpp
// Disable memory monitoring
json::set_memory_threshold(0);
json::set_memory_threshold(0.0);
```

## Implementation Details

### Memory Monitoring

The library uses platform-specific APIs to monitor memory usage:
- **Windows**: Uses `GetProcessMemoryInfo` from `psapi.h`
- **Linux/Unix**: Uses `getrusage` from `sys/resource.h`

### Parse State

The `parse_state` structure contains:
- Current token being parsed
- Parser state machine state
- Nesting depth
- Current object key (if applicable)
- Input stream position
- Remaining input data
- Partially parsed JSON value

### Performance Considerations

Memory checks are performed at key parsing nodes (object start, array start, value parsing, etc.), which adds minimal overhead to the parsing process. The exact performance impact depends on the JSON structure and the frequency of memory checks.

## Limitations

- The memory monitoring is process-wide, not per-parser instance
- The parse state serialization does not include the complete input stream, only the remaining unparsed portion
- The current implementation does not support resuming parsing from arbitrary points in all cases
- Memory usage reporting may vary slightly between platforms

## Best Practices

1. **Set Realistic Thresholds**: Set thresholds that leave enough memory for other system operations
2. **Handle Exceptions Gracefully**: Always catch `memory_limit_exception` when parsing large JSON data
3. **Save Parse State**: Save the parse state when an exception occurs to allow recovery
4. **Free Memory Before Recovery**: Ensure sufficient memory is available before attempting to resume parsing
5. **Test Thoroughly**: Test your application with various JSON sizes and memory configurations

## Example

```cpp
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using nlohmann::json;

void save_parse_state(const json::detail::parse_state<json>& state, const std::string& filename)
{
    std::ofstream file(filename);
    if (file.is_open())
    {
        file << state.to_json().dump(2);
        file.close();
    }
}

json::detail::parse_state<json> load_parse_state(const std::string& filename)
{
    json::detail::parse_state<json> state;
    std::ifstream file(filename);
    if (file.is_open())
    {
        json j;
        file >> j;
        state.from_json(j);
        file.close();
    }
    return state;
}

int main()
{
    // Set memory threshold to 100MB
    json::set_memory_threshold(100 * 1024 * 1024);
    
    try
    {
        // Try to parse a very large JSON file
        std::ifstream file("large_file.json");
        json j;
        file >> j;
        
        // Process the JSON
        std::cout << "Successfully parsed JSON with size: " << j.size() << std::endl;
    }
    catch (const json::detail::memory_limit_exception& ex)
    {
        std::cerr << "Memory limit exceeded during parsing!" << std::endl;
        std::cerr << "Current memory: " << ex.current_memory << " bytes" << std::endl;
        std::cerr << "Threshold: " << ex.memory_threshold << " bytes" << std::endl;
        
        // Save parse state for later recovery
        auto state = ex.get_parse_state<json>();
        if (state)
        {
            save_parse_state(*state, "parse_state.json");
            std::cout << "Parse state saved to parse_state.json" << std::endl;
        }
        
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```
