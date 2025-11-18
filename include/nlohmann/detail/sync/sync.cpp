//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#include <nlohmann/detail/sync/sync.hpp>
#include <nlohmann/json.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

json_patch_extended json_patch_extended::create_move(const json_pointer& from, const json_pointer& to)
{
    json_patch_extended patch = json::array();
    patch.push_back({
        {"op", "move"},
        {"from", from.to_string()},
        {"path", to.to_string()}
    });
    return patch;
}

struct version_vector::impl
{
    // Node ID to version mapping
    std::unordered_map<std::string, uint64_t> m_versions;
};

version_vector::version_vector() : m_impl(std::make_unique<impl>())
{
}

version_vector::~version_vector() noexcept = default;

void version_vector::increment(const std::string& node_id)
{
    m_impl->m_versions[node_id]++;
}

void version_vector::merge(const version_vector& other)
{
    for (const auto& [node_id, version] : other.m_impl->m_versions)
    {
        auto it = m_impl->m_versions.find(node_id);
        if (it == m_impl->m_versions.end() || it->second < version)
        {
            m_impl->m_versions[node_id] = version;
        }
    }
}

bool version_vector::is_before(const version_vector& other) const
{
    bool found_strictly_less = false;
    
    // Check all nodes in this version vector
    for (const auto& [node_id, version] : m_impl->m_versions)
    {
        auto it = other.m_impl->m_versions.find(node_id);
        if (it == other.m_impl->m_versions.end() || version > it->second)
        {
            return false;
        }
        if (version < it->second)
        {
            found_strictly_less = true;
        }
    }
    
    // Check if other has nodes not in this
    for (const auto& [node_id, version] : other.m_impl->m_versions)
    {
        if (m_impl->m_versions.find(node_id) == m_impl->m_versions.end())
        {
            found_strictly_less = true;
        }
    }
    
    return found_strictly_less;
}

json version_vector::to_json() const
{
    return m_impl->m_versions;
}

version_vector version_vector::from_json(const json& j)
{
    version_vector vv;
    vv.m_impl->m_versions = j.get<std::unordered_map<std::string, uint64_t>>();
    return vv;
}

struct json_sync::impl
{
    conflict_strategy_t m_default_strategy = conflict_strategy_t::keep_local;
    
    // Generate diff recursively
    void generate_diff(const json& old_val, const json& new_val, const json_pointer& path, json_patch_extended& patch) const
    {
        if (old_val.type() != new_val.type())
        {
            // Different types: replace old with new
            patch.push_back({
                {"op", "replace"},
                {"path", path.to_string()},
                {"value", new_val}
            });
            return;
        }
        
        if (old_val.is_object())
        {
            // Compare objects
            // Find keys in old but not in new (remove)
            for (const auto& [key, old_item] : old_val.items())
            {
                if (!new_val.contains(key))
                {
                    patch.push_back({
                        {"op", "remove"},
                        {"path", (path / key).to_string()}
                    });
                }
            }
            
            // Find keys in new but not in old (add), or changed (replace)
            for (const auto& [key, new_item] : new_val.items())
            {
                if (!old_val.contains(key))
                {
                    // Add new key
                    patch.push_back({
                        {"op", "add"},
                        {"path", (path / key).to_string()},
                        {"value", new_item}
                    });
                }
                else
                {
                    // Same key, check value
                    const json& old_item = old_val[key];
                    if (old_item != new_item)
                    {
                        // Recursively generate diff for value
                        generate_diff(old_item, new_item, path / key, patch);
                    }
                }
            }
        }
        else if (old_val.is_array())
        {
            // Arrays: for simplicity, replace if any difference
            // More advanced: find common prefix/suffix and patch changes
            if (old_val != new_val)
            {
                patch.push_back({
                    {"op", "replace"},
                    {"path", path.to_string()},
                    {"value", new_val}
                });
            }
        }
        else
        {
            // Primitive types: replace if different
            if (old_val != new_val)
            {
                patch.push_back({
                    {"op", "replace"},
                    {"path", path.to_string()},
                    {"value", new_val}
                });
            }
        }
    }
};

json_sync::json_sync() : m_impl(std::make_unique<impl>())
{
}

json_sync::~json_sync() noexcept = default;

json_patch_extended json_sync::diff(const json& old_doc, const json& new_doc) const
{
    json_patch_extended patch = json::array();
    m_impl->generate_diff(old_doc, new_doc, json_pointer{}, patch);
    return patch;
}

json json_sync::apply_patch(const json& doc, const json_patch_extended& patch) const
{
    json result = doc;
    
    for (const auto& operation : patch)
    {
        const std::string op = operation["op"].get<std::string>();
        const json_pointer path(operation["path"].get<std::string>());
        
        if (op == "add")
        {
            result[path] = operation["value"];
        }
        else if (op == "remove")
        {
            result.erase(path);
        }
        else if (op == "replace")
        {
            result[path] = operation["value"];
        }
        else if (op == "move")
        {
            const json_pointer from(operation["from"].get<std::string>());
            const json value = result[from];
            result.erase(from);
            result[path] = value;
        }
        else if (op == "copy")
        {
            const json_pointer from(operation["from"].get<std::string>());
            result[path] = result[from];
        }
        // Add other patch operations as needed
    }
    
    return result;
}

json json_sync::resolve_conflicts(const json& local, const json& remote, const version_vector& local_version, const version_vector& remote_version, conflict_strategy_t strategy, conflict_resolver_t resolver) const
{
    // Simple conflict resolution based on version vectors
    if (local_version.is_before(remote_version))
    {
        return remote;
    }
    else if (remote_version.is_before(local_version))
    {
        return local;
    }
    
    // Concurrent changes, use strategy
    switch (strategy)
    {
        case conflict_strategy_t::keep_local:
            return local;
        case conflict_strategy_t::keep_remote:
            return remote;
        case conflict_strategy_t::merge:
        {
            // Simple merge for objects
            if (local.is_object() && remote.is_object())
            {
                json merged = local;
                for (const auto& [key, remote_val] : remote.items())
                {
                    if (local.contains(key))
                    {
                        // Both have the key, recursively merge
                        merged[key] = resolve_conflicts(local[key], remote_val, local_version, remote_version, conflict_strategy_t::merge, resolver);
                    }
                    else
                    {
                        // Only remote has the key, add it
                        merged[key] = remote_val;
                    }
                }
                return merged;
            }
            // For other types, keep local
            return local;
        }
        case conflict_strategy_t::custom:
            if (resolver)
            {
                return resolver(local, remote, json_pointer{});
            }
            // Fallback to keep_local if no resolver
            return local;
        default:
            return local;
    }
}

void json_sync::set_default_strategy(conflict_strategy_t strategy)
{
    m_impl->m_default_strategy = strategy;
}

} // namespace detail
NLOHMANN_JSON_NAMESPACE_END