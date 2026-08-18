// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for ThreadSafeUnorderedMap and AssetManager
#include "../Engine/Core/AssetManager.h"
#include "../Engine/EntityAndScene/Sprite.h"
#include "test_main.h"
#include <stdexcept>
#include <thread>
#include <atomic>

REGISTER_TEST(test_ThreadSafeMap_insert_get)
{
    ThreadSafeUnorderedMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);

    auto a = map.get("a");
    ASSERT_TRUE(a.has_value());
    ASSERT_EQ(1, a.value());

    auto c = map.get("c");
    ASSERT_FALSE(c.has_value());
}

REGISTER_TEST(test_ThreadSafeMap_update)
{
    ThreadSafeUnorderedMap<std::string, int> map;
    map.insert("k", 1);
    map.insert("k", 42);
    ASSERT_EQ(42, map.get("k").value());
}

REGISTER_TEST(test_ThreadSafeMap_contains_erase_size)
{
    ThreadSafeUnorderedMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);
    map.insert("c", 3);

    ASSERT_TRUE(map.contains("b"));
    ASSERT_EQ(3, static_cast<int>(map.size()));

    map.erase("b");
    ASSERT_FALSE(map.contains("b"));
    ASSERT_TRUE(map.contains("a"));
    ASSERT_EQ(2, static_cast<int>(map.size()));

    map.clear();
    ASSERT_EQ(0, static_cast<int>(map.size()));
    ASSERT_FALSE(map.contains("a"));
}

REGISTER_TEST(test_AssetManager_null_renderer_throws)
{
    bool threw = false;
    try
    {
        AssetManager am(nullptr);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

REGISTER_TEST(test_AssetManager_create_and_get)
{
    // Renderer default-constructs without touching SDL
    Renderer renderer;
    AssetManager am(&renderer);

    auto sprite = am.createAsset<SimpleSprite>("player");
    ASSERT_TRUE(sprite != nullptr);
    ASSERT_EQ("player", sprite->getId());

    auto fetched = am.getAsset<SimpleSprite>("player");
    ASSERT_TRUE(fetched != nullptr);
    ASSERT_EQ(fetched.get(), sprite.get());  // Same instance
}

REGISTER_TEST(test_AssetManager_get_missing)
{
    Renderer renderer;
    AssetManager am(&renderer);

    auto missing = am.getAsset<SimpleSprite>("nope");
    ASSERT_TRUE(missing == nullptr);
}

REGISTER_TEST(test_AssetManager_duplicate_create)
{
    Renderer renderer;
    AssetManager am(&renderer);

    auto a = am.createAsset<SimpleSprite>("dup");
    auto b = am.createAsset<SimpleSprite>("dup");
    ASSERT_EQ(a.get(), b.get());  // Returns existing asset
}

REGISTER_TEST(test_AssetManager_remove)
{
    Renderer renderer;
    AssetManager am(&renderer);

    am.createAsset<SimpleSprite>("temp");
    ASSERT_TRUE(am.getAsset<SimpleSprite>("temp") != nullptr);

    am.removeAsset("temp");
    ASSERT_TRUE(am.getAsset<SimpleSprite>("temp") == nullptr);
}

// The two concurrency tests below use std::thread. Emscripten's WASM test
// binaries run single-threaded in Node (no pthreads - enabling them is
// fragile in CI and needs every linked object, including third-party libs,
// compiled with atomics), and std::thread would throw at runtime there. They
// are guarded out for __EMSCRIPTEN__ and stay covered by the NATIVE sanitizer
// jobs, which run them with real threads.
#ifndef __EMSCRIPTEN__
REGISTER_TEST(test_ThreadSafeMap_concurrent_inserts)
{
    // 8 threads each insert 500 distinct keys concurrently.
    // No keys collide, so the final size must be exactly 4000.
    ThreadSafeUnorderedMap<std::string, int> map;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&map, t]() {
            for (int i = 0; i < kPerThread; ++i)
            {
                map.insert("key_" + std::to_string(t) + "_" + std::to_string(i), i);
            }
        });
    }
    for (auto& th : threads) th.join();

    ASSERT_EQ(kThreads * kPerThread, static_cast<int>(map.size()));

    // Spot-check values survived intact (no torn reads under contention)
    auto v = map.get("key_3_250");
    ASSERT_TRUE(v.has_value());
    ASSERT_EQ(250, v.value());
}

REGISTER_TEST(test_ThreadSafeMap_concurrent_mixed_ops)
{
    // Concurrent insert + get + erase on overlapping keys must never crash
    // and must leave a consistent (non-negative) size.
    ThreadSafeUnorderedMap<std::string, int> map;
    constexpr int kThreads = 6;
    constexpr int kIterations = 2000;
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&map, &stop, t]() {
            for (int i = 0; i < kIterations && !stop.load(); ++i)
            {
                std::string key = "k" + std::to_string(i % 64);
                switch (t % 3)
                {
                    case 0: map.insert(key, i); break;
                    case 1: (void)map.get(key); break;
                    case 2: map.erase(key); break;
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    ASSERT_TRUE(map.size() <= 64);  // Keys only ever span 0..63
}
#endif // !__EMSCRIPTEN__
