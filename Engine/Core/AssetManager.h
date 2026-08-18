// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <SDL2/SDL.h>
#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <optional>

class Scene;
class SimpleSprite;
class Object;
class Renderer;

template <typename Key, typename Value>
class ThreadSafeUnorderedMap 
{
public:
    // Inserts or updates a key-value pair in the map
    void insert (const Key& key, const Value& value) 
    {
        std::scoped_lock lock (mutex_);
        map_[key] = value;
    }

    // Erases an element by key if it exists
    void erase (const Key& key) 
    {
        std::scoped_lock lock (mutex_);
        map_.erase (key);
    }

    // Retrieves the value associated with a key if it exists
    std::optional<Value> get (const Key& key) const 
    {
        std::scoped_lock lock (mutex_);
        auto it = map_.find (key);
        if (it != map_.end()) 
        {
            return it->second;
        }
        return std::nullopt;
    }

    // Checks if a key exists in the map
    bool contains (const Key& key) const 
    {
        std::scoped_lock lock (mutex_);
        return map_.find (key) != map_.end();
    }

    // Retrieves the current size of the map
    size_t size() const 
    {
        std::scoped_lock lock (mutex_);
        return map_.size();
    }

    // Clears all elements from the map
    void clear() 
    {
        std::scoped_lock lock (mutex_);
        map_.clear();
    }

private:
    mutable std::mutex mutex_;                  // Mutex for thread safety
    std::unordered_map<Key, Value> map_;        // Underlying map
};

class AssetManager 
{
public:
    // Constructor that requires an Renderer
    explicit AssetManager (Renderer* renderer)
        : renderer (renderer)
    {
        if (!renderer) 
        {
            throw std::runtime_error ("Invalid Renderer passed to AssetManager");
        }
    }

    AssetManager (const AssetManager&) = delete;
    AssetManager& operator= (const AssetManager&) = delete;

    // Template function to create an asset, optionally with a path
    template <typename T>
    std::shared_ptr<T> createAsset (const std::string& assetName) 
    {
        auto existingAsset = assets.get(assetName);
        if (existingAsset) {
            std::cerr << "Asset '" << assetName << "' already exists. Returning existing asset." << '\n';
            return std::dynamic_pointer_cast<T>(existingAsset.value());
        }

        std::shared_ptr<T> asset = std::make_shared<T>(renderer, assetName);
        assets.insert (assetName, asset);
        return asset;
    }

    // Function to retrieve an asset by name with type casting
    template <typename T>
    std::shared_ptr<T> getAsset (const std::string& assetName) const 
    {
        auto asset = assets.get (assetName);
        if (asset) 
        {
            return std::dynamic_pointer_cast<T>(asset.value());
        }
        std::cerr << "Asset '" << assetName << "' not found." << '\n';
        return nullptr;
    }

    // Optionally, a function to remove an asset if needed
    void removeAsset (const std::string& assetName) 
    {
        assets.erase (assetName);
    }

private:
    Renderer* renderer; // Renderer provided by the caller
    ThreadSafeUnorderedMap<std::string, std::shared_ptr<Object>> assets; // Thread-safe assets map
};

#endif
