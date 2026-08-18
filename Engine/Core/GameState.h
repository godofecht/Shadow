// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

// Game state representation for LLM navigation
// Provides text-based state that LLMs can understand and act upon

struct GameState {
    bool gameRunning = false;
    bool gameOver = false;
    bool gameWon = false;
    int score = 0;
    int level = 1;
    std::string message;
    std::unordered_map<std::string, int> stats;
    
    // Grid-based state (for tile games)
    int gridWidth = 0;
    int gridHeight = 0;
    std::vector<std::vector<int>> grid;
    
    // Entity positions (name -> {x, y})
    std::unordered_map<std::string, std::pair<int, int>> entities;
    
    // Available actions
    std::vector<std::string> availableActions;
    
    // Convert to LLM-readable string
    std::string toString() const {
        std::ostringstream oss;
        oss << "=== GAME STATE ===\n";
        oss << "Status: " << (gameOver ? "GAME OVER" : (gameWon ? "WON" : "PLAYING")) << "\n";
        oss << "Score: " << score << "\n";
        oss << "Level: " << level << "\n";
        if (!message.empty()) oss << "Message: " << message << "\n";
        
        // Stats
        for (const auto& [key, value] : stats) {
            oss << key << ": " << value << "\n";
        }
        
        // Grid visualization (compact)
        if (gridWidth > 0 && gridHeight > 0 && gridWidth <= 50 && gridHeight <= 50) {
            oss << "\n=== GRID ===\n";
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    const std::size_t sy = static_cast<std::size_t>(y);
                    const std::size_t sx = static_cast<std::size_t>(x);
                    int val = (y < (int)grid.size() && x < (int)grid[sy].size()) ? grid[sy][sx] : 0;
                    oss << (val == 0 ? '.' : (val == 1 ? '#' : (val == 2 ? 'O' : (val == 3 ? 'E' : (val == 4 ? '$' : '*')))));
                }
                oss << "\n";
            }
        }
        
        // Entity positions
        if (!entities.empty()) {
            oss << "\n=== ENTITIES ===\n";
            for (const auto& [name, pos] : entities) {
                oss << name << ": (" << pos.first << ", " << pos.second << ")\n";
            }
        }
        
        // Available actions
        if (!availableActions.empty()) {
            oss << "\n=== AVAILABLE ACTIONS ===\n";
            for (const auto& action : availableActions) {
                oss << "  - " << action << "\n";
            }
        }
        
        oss << "==================";
        return oss.str();
    }
    
    // Convert to JSON-like format
    std::string toJSON() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"gameRunning\": " << (gameRunning ? "true" : "false") << ",\n";
        oss << "  \"gameOver\": " << (gameOver ? "true" : "false") << ",\n";
        oss << "  \"score\": " << score << ",\n";
        oss << "  \"level\": " << level << ",\n";
        oss << R"(  "message": ")" << message << R"(",)" << "\n";
        oss << "  \"gridWidth\": " << gridWidth << ",\n";
        oss << "  \"gridHeight\": " << gridHeight << ",\n";
        oss << "  \"availableActions\": [";
        for (size_t i = 0; i < availableActions.size(); i++) {
            oss << "\"" << availableActions[i] << "\"";
            if (i < availableActions.size() - 1) oss << ", ";
        }
        oss << "]\n";
        oss << "}";
        return oss.str();
    }
};

// Action result
struct ActionResult {
    bool success = false;
    std::string message;
    int scoreChange = 0;
    bool gameOver = false;
    bool gameWon = false;
};

// Base interface for LLM-playable games
class LLMPlayable {
public:
    virtual ~LLMPlayable() = default;
    
    // Get current game state
    virtual GameState getState() const = 0;
    
    // Execute an action by name
    virtual ActionResult executeAction(const std::string& action) = 0;
    
    // Get list of available actions
    virtual std::vector<std::string> getAvailableActions() const = 0;
    
    // Reset the game
    virtual void reset() = 0;
    
    // Check if game is over
    virtual bool isGameOver() const = 0;
    
    // Check if game is won
    virtual bool isGameWon() const = 0;
};
