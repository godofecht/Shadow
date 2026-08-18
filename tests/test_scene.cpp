// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Scene sprite management (no rendering - sprites use null renderer)
#include "../Engine/EntityAndScene/Scene.h"
#include "test_main.h"

// SimpleSprite with a null Renderer* is safe as long as we never render
// or load textures (the ctor only stores the pointer).
static std::shared_ptr<SimpleSprite> makeSprite(const std::string& id)
{
    return std::make_shared<SimpleSprite>(static_cast<Renderer*>(nullptr), id);
}

REGISTER_TEST(test_Scene_default)
{
    Scene scene;
    ASSERT_TRUE(scene.isInitialized);
    ASSERT_EQ(0, scene.getAssetCount());
    ASSERT_TRUE(scene.getSpriteById("anything") == nullptr);
}

REGISTER_TEST(test_Scene_add_and_get_sprite)
{
    Scene scene;
    auto sprite = makeSprite("player");
    scene.addItem(sprite);

    auto found = scene.getSpriteById("player");
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(found.get(), sprite.get());
}

REGISTER_TEST(test_Scene_get_missing_sprite)
{
    Scene scene;
    scene.addItem(makeSprite("one"));
    ASSERT_TRUE(scene.getSpriteById("missing") == nullptr);
}

REGISTER_TEST(test_Scene_add_multiple)
{
    Scene scene;
    scene.addItem(makeSprite("a"));
    scene.addItem(makeSprite("b"));
    scene.addItem(makeSprite("c"));

    ASSERT_TRUE(scene.getSpriteById("a") != nullptr);
    ASSERT_TRUE(scene.getSpriteById("b") != nullptr);
    ASSERT_TRUE(scene.getSpriteById("c") != nullptr);
}

REGISTER_TEST(test_Scene_initialize_pending)
{
    Scene scene;
    auto sprite = makeSprite("ready");

    // Sprites are pending until initialized; getAssetCount reflects the
    // renderable list only.
    scene.addItem(sprite);
    ASSERT_EQ(0, scene.getAssetCount());

    sprite->isInitialized = true;
    scene.initializePendingSprites();
    ASSERT_EQ(1, scene.getAssetCount());
    ASSERT_TRUE(scene.getSpriteById("ready") != nullptr);
}

REGISTER_TEST(test_Scene_duplicate_ids_last_wins)
{
    Scene scene;
    auto first = makeSprite("dup");
    auto second = makeSprite("dup");
    scene.addItem(first);
    scene.addItem(second);

    // getSpriteById returns the first match found
    auto found = scene.getSpriteById("dup");
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(found.get(), first.get());
}
