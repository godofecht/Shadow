// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for AudioEngine group management.
// NOTE: These exercise the group/player registry only - no SDL_mixer device
// is opened, so no audio hardware is required (safe for CI).
#include "../Engine/Audio/AudioEngine.h"
#include "test_main.h"

REGISTER_TEST(test_AudioEngine_starts_empty)
{
    AudioEngine engine;
    ASSERT_EQ(0, static_cast<int>(engine.audioMediaGroups.size()));
}

REGISTER_TEST(test_AudioEngine_add_group)
{
    AudioEngine engine;
    engine.addAudioMediaGroup("sfx");
    ASSERT_EQ(1, static_cast<int>(engine.audioMediaGroups.size()));
}

REGISTER_TEST(test_AudioEngine_get_group_by_id)
{
    AudioEngine engine;
    engine.addAudioMediaGroup("sfx");
    engine.addAudioMediaGroup("music");

    AudioMediaGroup& g = engine.getAudioMediaGroupById("sfx");
    ASSERT_EQ("sfx", g.id);
}

REGISTER_TEST(test_AudioEngine_get_group_by_id_creates)
{
    AudioEngine engine;
    // Requesting a non-existent group auto-creates it
    AudioMediaGroup& g = engine.getAudioMediaGroupById("newgroup");
    ASSERT_EQ("newgroup", g.id);
    ASSERT_EQ(1, static_cast<int>(engine.audioMediaGroups.size()));
}

REGISTER_TEST(test_AudioEngine_get_group_by_index)
{
    AudioEngine engine;
    engine.addAudioMediaGroup("first");
    engine.addAudioMediaGroup("second");

    AudioMediaGroup& g0 = engine.getAudioMediaGroupByIndex(0);
    AudioMediaGroup& g1 = engine.getAudioMediaGroupByIndex(1);
    ASSERT_EQ("first", g0.id);
    ASSERT_EQ("second", g1.id);
}

REGISTER_TEST(test_AudioEngine_play_stop_empty_noop)
{
    AudioEngine engine;
    engine.addAudioMediaGroup("empty");

    // No players loaded -> these are no-ops (no crash)
    engine.playAudioInGroup("empty", "nonexistent");
    engine.stopAudioInGroup("empty", "nonexistent");
    engine.playAudioInGroup("doesnotexist", "x");
    engine.stopAudioInAllGroups();
}

REGISTER_TEST(test_AudioMediaGroup_player_registry)
{
    AudioMediaGroup group("sfx");
    ASSERT_TRUE(group.audioPlayers.empty());

    // addAudioPlayer with a bogus path still registers a player object
    // (loading fails gracefully, but the registry entry exists).
    group.addAudioPlayer("nonexistent-file.wav", "jump");
    ASSERT_EQ(1, static_cast<int>(group.audioPlayers.size()));
    ASSERT_EQ("jump", group.audioPlayers[0].getId());
}
