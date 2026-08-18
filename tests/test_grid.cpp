// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Grid and GridEntity
#include "../Engine/EntityAndScene/Grid.h"
#include "test_main.h"

REGISTER_TEST(test_Grid_creation)
{
    Grid grid(10, 8, 20);
    ASSERT_EQ(10, grid.getWidth());
    ASSERT_EQ(8, grid.getHeight());
    ASSERT_EQ(20, grid.getTileSize());
}

REGISTER_TEST(test_Grid_default_cells)
{
    Grid grid(4, 4);
    GridCell c = grid.cell(1, 1);
    ASSERT_EQ(0, c.value);
    ASSERT_FALSE(c.isSolid);
    ASSERT_TRUE(c.isVisible);
}

REGISTER_TEST(test_Grid_set_get_value)
{
    Grid grid(5, 5);
    grid.setValue(2, 3, 7);
    ASSERT_EQ(7, grid.getValue(2, 3));

    grid.cell(0, 0).value = 42;
    ASSERT_EQ(42, grid.getValue(0, 0));
}

REGISTER_TEST(test_Grid_bounds)
{
    Grid grid(5, 5);
    ASSERT_TRUE(grid.isInBounds(0, 0));
    ASSERT_TRUE(grid.isInBounds(4, 4));
    ASSERT_FALSE(grid.isInBounds(5, 4));
    ASSERT_FALSE(grid.isInBounds(4, 5));
    ASSERT_FALSE(grid.isInBounds(-1, 0));
    ASSERT_FALSE(grid.isInBounds(0, -1));
}

REGISTER_TEST(test_Grid_cell_rect)
{
    Grid grid(10, 10, 20);
    Rect<float> r = grid.getCellRect(2, 3);
    ASSERT_EQ(40.0f, r.x);
    ASSERT_EQ(60.0f, r.y);
    ASSERT_EQ(20.0f, r.width);
    ASSERT_EQ(20.0f, r.height);
}

REGISTER_TEST(test_Grid_screen_to_grid)
{
    Grid grid(10, 10, 20);
    int gx, gy;
    grid.screenToGrid(45, 65, gx, gy);
    ASSERT_EQ(2, gx);
    ASSERT_EQ(3, gy);

    grid.screenToGrid(0, 0, gx, gy);
    ASSERT_EQ(0, gx);
    ASSERT_EQ(0, gy);
}

REGISTER_TEST(test_Grid_grid_to_screen)
{
    Grid grid(10, 10, 20);
    int sx, sy;
    grid.gridToScreen(2, 3, sx, sy);
    ASSERT_EQ(40, sx);
    ASSERT_EQ(60, sy);
}

REGISTER_TEST(test_Grid_fill_and_clear)
{
    Grid grid(3, 3);
    SDL_Color c = {10, 20, 30, 255};
    grid.fill(c);
    ASSERT_EQ(10, grid.cell(1, 1).color.r);
    ASSERT_EQ(20, grid.cell(2, 2).color.g);

    grid.clear();
    ASSERT_EQ(0, grid.cell(1, 1).value);
    ASSERT_FALSE(grid.cell(1, 1).isSolid);
}

REGISTER_TEST(test_Grid_set_cell_color)
{
    Grid grid(4, 4);
    SDL_Color c = {5, 15, 25, 255};
    grid.setCellColor(1, 2, c);
    ASSERT_EQ(5, grid.cell(1, 2).color.r);
    ASSERT_EQ(15, grid.cell(1, 2).color.g);
    ASSERT_EQ(25, grid.cell(1, 2).color.b);

    // Out of bounds is a no-op (no crash)
    grid.setCellColor(99, 99, c);
}

REGISTER_TEST(test_Grid_set_border_color)
{
    Grid grid(3, 3);
    SDL_Color c = {9, 8, 7, 255};
    grid.setBorderColor(c);
    ASSERT_EQ(9, grid.cell(0, 0).borderColor.r);
    ASSERT_EQ(8, grid.cell(2, 2).borderColor.g);
    ASSERT_EQ(7, grid.cell(1, 1).borderColor.b);
}

REGISTER_TEST(test_Grid_to_tilemap_data)
{
    Grid grid(3, 2);
    grid.setValue(0, 0, 1);
    grid.setValue(1, 1, 5);

    auto data = grid.toTileMapData();
    ASSERT_EQ(2, static_cast<int>(data.size()));
    ASSERT_EQ(3, static_cast<int>(data[0].size()));
    ASSERT_EQ(1.0f, data[0][0]);
    ASSERT_EQ(5.0f, data[1][1]);
    ASSERT_EQ(0.0f, data[0][1]);
}

REGISTER_TEST(test_GridEntity_creation)
{
    Grid grid(10, 10);
    GridEntity e(&grid, 3, 4);
    ASSERT_EQ(3, e.getX());
    ASSERT_EQ(4, e.getY());
    ASSERT_TRUE(e.isActive());
    ASSERT_EQ(&grid, e.getGrid());
}

REGISTER_TEST(test_GridEntity_move)
{
    Grid grid(10, 10);
    GridEntity e(&grid, 3, 4);
    e.move(1, 2);
    ASSERT_EQ(4, e.getX());
    ASSERT_EQ(6, e.getY());
}

REGISTER_TEST(test_GridEntity_set_position)
{
    Grid grid(10, 10);
    GridEntity e(&grid, 3, 4);
    e.setPosition(7, 8);
    ASSERT_EQ(7, e.getX());
    ASSERT_EQ(8, e.getY());

    // Out of bounds set is ignored
    e.setPosition(99, 99);
    ASSERT_EQ(7, e.getX());
    ASSERT_EQ(8, e.getY());
}

REGISTER_TEST(test_GridEntity_try_move)
{
    Grid grid(10, 10);
    grid.cell(4, 4).isSolid = true;  // Block (4,4)

    GridEntity e(&grid, 3, 4);
    // Moving right from (3,4) into solid (4,4) is blocked
    ASSERT_FALSE(e.tryMove(1, 0));
    ASSERT_EQ(3, e.getX());
    ASSERT_EQ(4, e.getY());

    // Moving down to (3,5) is free
    ASSERT_TRUE(e.tryMove(0, 1));
    ASSERT_EQ(3, e.getX());
    ASSERT_EQ(5, e.getY());

    // Out of bounds is blocked
    ASSERT_FALSE(e.tryMove(50, 0));
    ASSERT_EQ(3, e.getX());
    ASSERT_EQ(5, e.getY());
}

REGISTER_TEST(test_GridEntity_can_move_to)
{
    Grid grid(10, 10);
    grid.cell(5, 5).isSolid = true;

    GridEntity e(&grid, 4, 5);
    ASSERT_FALSE(e.canMoveTo(5, 5));   // solid
    ASSERT_TRUE(e.canMoveTo(3, 5));    // free
    ASSERT_FALSE(e.canMoveTo(-1, 5));  // out of bounds
    ASSERT_FALSE(e.canMoveTo(5, 99));
}

REGISTER_TEST(test_GridEntity_visual_properties)
{
    Grid grid(10, 10);
    GridEntity e(&grid, 1, 1);

    SDL_Color c = {255, 0, 0, 255};
    e.setColor(c);
    ASSERT_EQ(255, e.getColor().r);
    ASSERT_EQ(0, e.getColor().g);

    e.setSymbol('@');
    ASSERT_EQ('@', e.getSymbol());
}

REGISTER_TEST(test_GridEntity_active_state)
{
    Grid grid(10, 10);
    GridEntity e(&grid, 1, 1);
    ASSERT_TRUE(e.isActive());
    e.setActive(false);
    ASSERT_FALSE(e.isActive());
}
