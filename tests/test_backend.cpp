// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Backend tests: repo integrity checks (example sources exist).
// All functional engine tests live in the main suite (sdl_app_tests).
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#ifndef SHADOW_SOURCE_DIR
#define SHADOW_SOURCE_DIR "."
#endif

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> testResults;

void recordTest(const std::string& name, bool passed, const std::string& message = "") {
    testResults.push_back({name, passed, message});
    std::cout << (passed ? "  [PASS] " : "  [FAIL] ") << name;
    if (!message.empty()) std::cout << " - " << message;
    std::cout << std::endl;
}

// ============================================
// Example File Validation Tests
// ============================================
void testExampleFiles() {
    std::cout << "\n=== Example File Tests ===" << std::endl;

    std::vector<std::string> requiredExamples = {
        "2DShapes", "2DViewportToWorld", "2DBloom", "CPUDrawing",
        "Mesh2dAlphaMode", "Arc2DMeshes", "ManualMesh2D", "Mesh2dRepeatedTexture",
        "Mesh2DVertexColors", "Mesh2D", "MoveSprite", "MultiWindowText",
        "PixelGridSnapping", "RotationToCursor", "Generic2DRotation",
        "SpriteAnimation", "SpriteFlipping", "SpriteScale", "SpriteSheet",
        "SpriteSlice", "SpriteTile", "Sprite", "Text2D", "TilemapChunk", "Transparency2D",
        // Classic games
        "Snake", "Minesweeper", "TicTacToe", "Roguelike"
    };

    int missing = 0;
    for (const auto& example : requiredExamples) {
        std::string path = std::string(SHADOW_SOURCE_DIR) + "/Examples/" + example + "/main.cpp";
        std::ifstream file(path);
        bool ok = file.good();
        recordTest("Example exists: " + example, ok);
        if (!ok) missing++;
    }
    if (missing == 0) {
        recordTest("All required examples present", true);
    }
}

// ============================================
// Main Test Runner
// ============================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Umbra Engine Backend Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    testExampleFiles();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0, failed = 0;
    for (const auto& result : testResults) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "Total: " << testResults.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "========================================" << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& result : testResults) {
            if (!result.passed) {
                std::cout << "  - " << result.name;
                if (!result.message.empty()) std::cout << ": " << result.message;
                std::cout << std::endl;
            }
        }
    }

    return failed > 0 ? 1 : 0;
}
