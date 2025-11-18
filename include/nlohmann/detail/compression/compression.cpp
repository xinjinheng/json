//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#include <nlohmann/detail/compression/compression.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
#include <cstring>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

// Trie structure for string prefix compression
class trie_node
{
public:
    std::unordered_map<char, std::unique_ptr<trie_node>> children;
    bool is_end_of_word = false;
    std::size_t count = 0;
};

struct json_compressor::impl
{
    compression_options m_options;
    trie_node m_trie_root;
    std::unordered_map<std::string, std::uint32_t> m_key_dict;
    std::uint32_t m_next_dict_id = 1;
    
    // Compress JSON recursively
    void compress_value(const json& j, std::vector<std::uint8_t>& output) const
    {
        // Simple compression: write type followed by value
        // This is a basic implementation, more advanced techniques can be added
        
        switch (j.type())
        {
            case json::value_t::null:
                output.push_back(0x00); // Null type
                break;
                
            case json::value_t::boolean:
                output.push_back(0x01); // Boolean type
                output.push_back(j.get<bool>() ? 0x01 : 0x00);
                break;
                
            case json::value_t::number_integer:
                output.push_back(0x02); // Integer type
                {
                    const int64_t val = j.get<int64_t>();
                    // Simple 64-bit little-endian encoding
                    for (int i = 0; i < 8; ++i)
                    {
                        output.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
                    }
                }
                break;
                
            case json::value_t::number_unsigned:
                output.push_back(0x03); // Unsigned integer type
                {
                    const uint64_t val = j.get<uint64_t>();
                    // Simple 64-bit little-endian encoding
                    for (int i = 0; i < 8; ++i)
                    {
                        output.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
                    }
                }
                break;
                
            case json::value_t::number_float:
                output.push_back(0x04); // Float type
                {
                    const double val = j.get<double>();
                    // Simple 64-bit little-endian encoding of double
                    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                    for (int i = 0; i < 8; ++i)
                    {
                        output.push_back(bytes[i]);
                    }
                }
                break;
                
            case json::value_t::string:
                output.push_back(0x05); // String type
                {
                    const std::string& str = j.get<std::string>();
                    // Write length followed by string content
                    const uint32_t length = static_cast<uint32_t>(str.size());
                    for (int i = 0; i < 4; ++i)
                    {
                        output.push_back(static_cast<uint8_t>((length >> (i * 8)) & 0xFF));
                    }
                    output.insert(output.end(), str.begin(), str.end());
                }
                break;
                
            case json::value_t::array:
                output.push_back(0x06); // Array type
                {
                    const uint32_t size = static_cast<uint32_t>(j.size());
                    // Write array size
                    for (int i = 0; i < 4; ++i)
                    {
                        output.push_back(static_cast<uint8_t>((size >> (i * 8)) & 0xFF));
                    }
                    // Write each element
                    for (const auto& element : j)
                    {
                        compress_value(element, output);
                    }
                }
                break;
                
            case json::value_t::object:
                output.push_back(0x07); // Object type
                {
                    const uint32_t size = static_cast<uint32_t>(j.size());
                    // Write object size
                    for (int i = 0; i < 4; ++i)
                    {
                        output.push_back(static_cast<uint8_t>((size >> (i * 8)) & 0xFF));
                    }
                    // Write each key-value pair
                    for (const auto& [key, value] : j.items())
                    {
                        // Write key
                        const uint32_t key_length = static_cast<uint32_t>(key.size());
                        for (int i = 0; i < 4; ++i)
                        {
                            output.push_back(static_cast<uint8_t>((key_length >> (i * 8)) & 0xFF));
                        }
                        output.insert(output.end(), key.begin(), key.end());
                        // Write value
                        compress_value(value, output);
                    }
                }
                break;
                
            default:
                output.push_back(0x00); // Default to null for unknown types
                break;
        }
    }
    
    // Build trie for prefix compression
    void build_trie(const json& j)
    {
        // Build trie from JSON string values
        if (j.is_string())
        {
            const std::string& str = j.get<std::string>();
            trie_node* current = &m_trie_root;
            
            for (char c : str)
            {
                auto it = current->children.find(c);
                if (it == current->children.end())
                {
                    current->children[c] = std::make_unique<trie_node>();
                }
                current = current->children[c].get();
            }
            
            current->is_end_of_word = true;
            current->count++;
        }
        else if (j.is_array())
        {
            for (const auto& element : j)
            {
                build_trie(element);
            }
        }
        else if (j.is_object())
        {
            for (const auto& [key, value] : j.items())
            {
                build_trie(key); // Build trie for keys as well
                build_trie(value);
            }
        }
    }
    
    // Build key dictionary
    void build_key_dict(const json& j)
    {
        // Build dictionary of frequent keys
        if (j.is_object())
        {
            for (const auto& [key, value] : j.items())
            {
                if (m_key_dict.find(key) == m_key_dict.end())
                {
                    m_key_dict[key] = m_next_dict_id++;
                    // Limit dictionary size
                    if (m_key_dict.size() >= m_options.dict_size_limit)
                    {
                        return;
                    }
                }
                build_key_dict(value);
            }
        }
        else if (j.is_array())
        {
            for (const auto& element : j)
            {
                build_key_dict(element);
            }
        }
    }
};

json_compressor::json_compressor(const compression_options& options) : m_impl(std::make_unique<impl>())
{
    m_impl->m_options = options;
}

json_compressor::~json_compressor() noexcept = default;

std::vector<std::uint8_t> json_compressor::compress(const json& j) const
{
    std::vector<std::uint8_t> output;
    m_impl->compress_value(j, output);
    return output;
}

void json_compressor::start_compression()
{
    // Reset internal state for streaming compression
    m_impl->m_trie_root = trie_node();
    m_impl->m_key_dict.clear();
    m_impl->m_next_dict_id = 1;
}

void json_compressor::compress_chunk(const json& j)
{
    // Build trie and dictionary for the chunk
    m_impl->build_trie(j);
    m_impl->build_key_dict(j);
}

std::vector<std::uint8_t> json_compressor::finish_compression()
{
    // For basic implementation, we don't use the trie or dictionary
    // In a more advanced implementation, we would write the dictionary here
    return {};
}

struct json_decompressor::impl
{
    std::unordered_map<std::uint32_t, std::string> m_key_dict;
    std::uint32_t m_next_dict_id = 1;
    
    // Decompress JSON recursively
    json decompress_value(const std::vector<std::uint8_t>& data, size_t& pos) const
    {
        if (pos >= data.size())
        {
            return json(nullptr);
        }
        
        const uint8_t type = data[pos++];
        
        switch (type)
        {
            case 0x00: // Null type
                return json(nullptr);
                
            case 0x01: // Boolean type
                if (pos >= data.size())
                {
                    return json(nullptr);
                }
                return json(data[pos++] != 0);
                
            case 0x02: // Integer type
                if (pos + 7 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    int64_t val = 0;
                    for (int i = 0; i < 8; ++i)
                    {
                        val |= static_cast<int64_t>(data[pos++]) << (i * 8);
                    }
                    return json(val);
                }
                
            case 0x03: // Unsigned integer type
                if (pos + 7 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    uint64_t val = 0;
                    for (int i = 0; i < 8; ++i)
                    {
                        val |= static_cast<uint64_t>(data[pos++]) << (i * 8);
                    }
                    return json(val);
                }
                
            case 0x04: // Float type
                if (pos + 7 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    double val;
                    uint8_t* bytes = reinterpret_cast<uint8_t*>(&val);
                    for (int i = 0; i < 8; ++i)
                    {
                        bytes[i] = data[pos++];
                    }
                    return json(val);
                }
                
            case 0x05: // String type
                if (pos + 3 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    uint32_t length = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        length |= static_cast<uint32_t>(data[pos++]) << (i * 8);
                    }
                    
                    if (pos + length > data.size())
                    {
                        return json(nullptr);
                    }
                    
                    std::string str(data.begin() + pos, data.begin() + pos + length);
                    pos += length;
                    return json(str);
                }
                
            case 0x06: // Array type
                if (pos + 3 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    uint32_t size = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        size |= static_cast<uint32_t>(data[pos++]) << (i * 8);
                    }
                    
                    json::array_t result;
                    for (uint32_t i = 0; i < size; ++i)
                    {
                        result.push_back(decompress_value(data, pos));
                    }
                    return json(result);
                }
                
            case 0x07: // Object type
                if (pos + 3 >= data.size())
                {
                    return json(nullptr);
                }
                {
                    uint32_t size = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        size |= static_cast<uint32_t>(data[pos++]) << (i * 8);
                    }
                    
                    json::object_t result;
                    for (uint32_t i = 0; i < size; ++i)
                    {
                        // Read key
                        if (pos + 3 >= data.size())
                        {
                            return json(nullptr);
                        }
                        
                        uint32_t key_length = 0;
                        for (int j = 0; j < 4; ++j)
                        {
                            key_length |= static_cast<uint32_t>(data[pos++]) << (j * 8);
                        }
                        
                        if (pos + key_length > data.size())
                        {
                            return json(nullptr);
                        }
                        
                        std::string key(data.begin() + pos, data.begin() + pos + key_length);
                        pos += key_length;
                        
                        // Read value
                        json value = decompress_value(data, pos);
                        result[key] = std::move(value);
                    }
                    return json(result);
                }
                
            default: // Unknown type, return null
                return json(nullptr);
        }
    }
};

json_decompressor::json_decompressor() : m_impl(std::make_unique<impl>())
{
}

json_decompressor::~json_decompressor() noexcept = default;

json json_decompressor::decompress(const std::vector<std::uint8_t>& data) const
{
    size_t pos = 0;
    return m_impl->decompress_value(data, pos);
}

void json_decompressor::start_decompression()
{
    // Reset internal state for streaming decompression
    m_impl->m_key_dict.clear();
    m_impl->m_next_dict_id = 1;
}

void json_decompressor::decompress_chunk(const std::vector<std::uint8_t>& data)
{
    // For basic implementation, we don't do anything here
    // In a more advanced implementation, we would process dictionary updates
}

json json_decompressor::finish_decompression()
{
    // For basic implementation, return null
    // In a more advanced implementation, we would return any remaining data
    return json(nullptr);
}

std::vector<std::uint8_t> compress_json(const json& j, const compression_options& options)
{
    json_compressor compressor(options);
    return compressor.compress(j);
}

json decompress_json(const std::vector<std::uint8_t>& data)
{
    json_decompressor decompressor;
    return decompressor.decompress(data);
}

} // namespace detail
NLOHMANN_JSON_NAMESPACE_END