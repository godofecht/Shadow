// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Physics components
#include "../Engine/Core/Physics.h"
#include "test_main.h"

REGISTER_TEST(test_World_creation)
{
    World world;
    b2WorldId id = world.getId();
    ASSERT_TRUE(id.index1 >= 0);
}

REGISTER_TEST(test_World_default_gravity)
{
    World world;
    b2WorldDef def = world.getDef();
    ASSERT_NEAR(10.0f, def.gravity.y, 0.001f);  // Downward gravity
    ASSERT_NEAR(0.0f, def.gravity.x, 0.001f);
}

REGISTER_TEST(test_World_step)
{
    World world;
    world.simulateStep();  // No crash on empty world
    b2WorldId id = world.getId();
    ASSERT_TRUE(id.index1 >= 0);
}

REGISTER_TEST(test_RigidBody_creation)
{
    World world;
    RigidBody body(world);

    b2BodyId id = body.getId();
    ASSERT_TRUE(id.index1 >= 0);

    b2BodyDef def = body.getDef();
    ASSERT_TRUE(def.type == b2_dynamicBody);
}

REGISTER_TEST(test_PhysicsManager_creation)
{
    PhysicsManager manager;
    World& world = manager.getWorld();

    b2WorldId id = world.getId();
    ASSERT_TRUE(id.index1 >= 0);
}

REGISTER_TEST(test_Body_setters_getters)
{
    Body body;

    body.setUid("test_body");
    ASSERT_TRUE(std::string(body.getUid()) == "test_body");

    b2BodyId testId = {0, 1, 2};
    body.setId(testId);
    b2BodyId retrievedId = body.getId();
    ASSERT_TRUE(testId.index1 == retrievedId.index1);

    b2BodyDef def = b2DefaultBodyDef();
    def.type = b2_kinematicBody;
    body.setDef(def);
    ASSERT_TRUE(body.getDef().type == b2_kinematicBody);
}

REGISTER_TEST(test_PhysicsObject_dynamic)
{
    World world;
    PhysicsObject obj(world, 5.0f, 5.0f, 2.0f, 2.0f, true);

    b2BodyId id = obj.getId();
    ASSERT_TRUE(id.index1 >= 0);
    ASSERT_TRUE(obj.getDef().type == b2_dynamicBody);
}

REGISTER_TEST(test_PhysicsObject_static)
{
    World world;
    PhysicsObject obj(world, 0.0f, 0.0f, 4.0f, 4.0f, false);

    b2BodyId id = obj.getId();
    ASSERT_TRUE(id.index1 >= 0);
    ASSERT_TRUE(obj.getDef().type == b2_staticBody);
}

REGISTER_TEST(test_World_step_with_bodies)
{
    World world;
    RigidBody body(world);
    PhysicsObject box(world, 10.0f, 10.0f, 1.0f, 1.0f, true);

    world.simulateStep();  // Stepping with bodies must not crash
}

REGISTER_TEST(test_World_delete_body_by_id)
{
    World world;
    RigidBody* body = new RigidBody(world);
    body->setUid("to_delete");

    world.addBody(body);
    world.deleteBodyWithId("to_delete");

    // World still usable
    world.simulateStep();
    delete body;
}

REGISTER_TEST(test_World_add_body)
{
    World world;
    RigidBody body(world);
    ASSERT_TRUE(body.getId().index1 >= 0);
}
