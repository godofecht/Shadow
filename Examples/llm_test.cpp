// LLM Test Client - Demonstrates LLM-playable interface
// Run this to see game state and test actions programmatically
#include "Engine/Core/Game2D.h"
#include <iostream>
#include <string>
#include <sstream>

// Test Snake game with LLM interface (no graphics needed)
class ConsoleSnake {
    std::vector<std::pair<int, int>> snakeBody;
    std::pair<int, int> food;
    std::pair<int, int> head;
    int direction = 1;  // 0=up, 1=right, 2=down, 3=left
    int score = 0;
    bool gameOver = false;
    
public:
    ConsoleSnake() {
        reset();
    }
    
    void reset() {
        head = {10, 10};
        snakeBody = {{9, 10}, {8, 10}};
        direction = 1;
        score = 0;
        gameOver = false;
        spawnFood();
    }
    
    void spawnFood() {
        food = {rand() % 20, rand() % 20};
    }
    
    GameState getState() const {
        GameState state;
        state.gameRunning = !gameOver;
        state.gameOver = gameOver;
        state.score = score;
        state.gridWidth = 20;
        state.gridHeight = 20;
        state.entities["head"] = head;
        state.entities["food"] = food;
        state.message = "Score: " + std::to_string(score);
        state.availableActions = {"up", "down", "left", "right", "restart"};
        return state;
    }
    
    ActionResult executeAction(const std::string& action) {
        if (gameOver && action != "restart") {
            return {false, "Game over! Use 'restart'"};
        }
        
        if (action == "restart") {
            reset();
            return {true, "Game restarted"};
        }
        
        int newDir = -1;
        if (action == "up") newDir = 0;
        else if (action == "down") newDir = 2;
        else if (action == "left") newDir = 3;
        else if (action == "right") newDir = 1;
        else return {false, "Unknown action: " + action};
        
        // Prevent 180-degree turns
        if ((newDir == 0 && direction == 2) ||
            (newDir == 2 && direction == 0) ||
            (newDir == 1 && direction == 3) ||
            (newDir == 3 && direction == 1)) {
            return {false, "Cannot reverse direction!"};
        }
        
        direction = newDir;
        
        // Move
        if (direction == 0) head.second--;
        else if (direction == 1) head.first++;
        else if (direction == 2) head.second++;
        else if (direction == 3) head.first--;
        
        // Wrap
        if (head.first < 0) head.first = 19;
        if (head.first >= 20) head.first = 0;
        if (head.second < 0) head.second = 19;
        if (head.second >= 20) head.second = 0;
        
        // Check food
        if (head == food) {
            score += 10;
            spawnFood();
            return {true, "Ate food! Score: " + std::to_string(score)};
        }
        
        return {true, "Moved"};
    }
    
    std::vector<std::string> getAvailableActions() const {
        if (gameOver) return {"restart"};
        return {"up", "down", "left", "right"};
    }
    
    bool isGameOver() const { return gameOver; }
};

// Demo: Show how LLM can interact with the game
void demoLLMInterface() {
    std::cout << "=== LLM Game Interface Demo ===\n\n";
    
    ConsoleSnake game;
    
    // Get initial state
    std::cout << "Initial game state:\n";
    std::cout << game.getState().toString() << "\n\n";
    
    // Show available actions
    std::cout << "Available actions:\n";
    for (const auto& action : game.getAvailableActions()) {
        std::cout << "  - " << action << "\n";
    }
    std::cout << "\n";
    
    // Execute some actions
    std::vector<std::string> testActions = {"right", "right", "down", "left", "up"};
    for (const auto& action : testActions) {
        auto result = game.executeAction(action);
        std::cout << "Action '" << action << "': " << result.message << "\n";
        
        // Show updated state
        auto state = game.getState();
        std::cout << "Head position: (" << state.entities["head"].first 
                  << ", " << state.entities["head"].second << ")\n\n";
    }
    
    // Show JSON output
    std::cout << "JSON state:\n";
    std::cout << game.getState().toJSON() << "\n\n";
    
    // Simulate LLM conversation
    std::cout << "=== Simulated LLM Conversation ===\n\n";
    std::cout << "System: Here's the current game state:\n";
    std::cout << game.getState().toString() << "\n\n";
    std::cout << "System: What action would you like to take?\n";
    std::cout << "LLM: I'll move right to get closer to the food.\n";
    auto result = game.executeAction("right");
    std::cout << "System: Action result - " << result.message << "\n";
    std::cout << "System: New state:\n" << game.getState().toString() << "\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--demo") {
        demoLLMInterface();
        return 0;
    }
    
    std::cout << "LLM Test Client\n";
    std::cout << "================\n\n";
    std::cout << "This demonstrates how an LLM can play games via state-based interface.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  ./llm_test          - Show this help\n";
    std::cout << "  ./llm_test --demo   - Run interactive demo\n\n";
    std::cout << "The engine provides:\n";
    std::cout << "  1. getState() - Returns game state as text/JSON\n";
    std::cout << "  2. executeAction(name) - Execute named action\n";
    std::cout << "  3. getAvailableActions() - List valid actions\n\n";
    std::cout << "Example LLM prompt:\n";
    std::cout << "  'Here is the game state: ' + game.getState().toString()\n";
    std::cout << "  'Available actions: ' + join(game.getAvailableActions())\n";
    std::cout << "  'What action do you take?'\n\n";
    std::cout << "For graphical games, run: snake_example, minesweeper_example, etc.\n";
    
    return 0;
}
