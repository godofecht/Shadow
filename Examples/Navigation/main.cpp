// A* Pathfinding Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <vector>
#include <queue>
#include <cmath>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

struct Node {
    int x, y;
    float g, h;
    Node* parent;
    bool operator>(const Node& other) const { return (g + h) > (other.g + other.h); }
};

class NavGame : public Game
{
    static const int GRID_SIZE = 20;
    static const int GRID_PIXELS = 500;
    static const int OFFSET_X = 50;
    static const int OFFSET_Y = 50;
    int grid[GRID_SIZE][GRID_SIZE] = {};
    std::vector<SDL_Point> path;
    int startX = 2, startY = 2;
    int endX = 17, endY = 17;
    bool showInstructions = true;
    enum class Algorithm { AStar, JumpFill };
    Algorithm algorithm = Algorithm::AStar;
    enum class EditMode { Walls, Start, End };
    EditMode editMode = EditMode::Walls;
    bool leftWasPressed = false;
    bool rightWasPressed = false;

public:
    NavGame(const char* title, [[maybe_unused]] int width, [[maybe_unused]] int height) : Game(title, 800, 600) {}

    void onStart() override {
        findPath();
    }

    void syncAlgorithmFromBrowser() {
#ifdef __EMSCRIPTEN__
        int algoId = emscripten_run_script_int("window.__umbraNavAlgo || 0");
        algorithm = (algoId == 1) ? Algorithm::JumpFill : Algorithm::AStar;
#endif
    }

    Algorithm getBrowserAlgorithm() const {
#ifdef __EMSCRIPTEN__
        int algoId = emscripten_run_script_int("window.__umbraNavAlgo || 0");
        return (algoId == 1) ? Algorithm::JumpFill : Algorithm::AStar;
#else
        return algorithm;
#endif
    }

    void rebuildPathFromParents(
        const std::map<std::pair<int,int>, std::pair<int,int>>& parents,
        int targetX,
        int targetY) {
        int cx = targetX, cy = targetY;
        while(cx != startX || cy != startY) {
            path.push_back({cx, cy});
            auto p = parents.at({cx, cy});
            cx = p.first;
            cy = p.second;
        }
    }

    void findPathAStar() {
        auto dist = [](int x1, int y1, int x2, int y2) { return (float)(abs(x1-x2) + abs(y1-y2)); };

        std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
        std::map<std::pair<int,int>, float> gScore;
        std::map<std::pair<int,int>, std::pair<int,int>> parents;

        open.push({startX, startY, 0, dist(startX, startY, endX, endY), nullptr});
        gScore[{startX, startY}] = 0;

        while(!open.empty()) {
            Node current = open.top();
            open.pop();
            if (current.x == endX && current.y == endY) {
                rebuildPathFromParents(parents, endX, endY);
                break;
            }

            int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
            for(int i=0; i<4; i++) {
                int nx = current.x + dx[i], ny = current.y + dy[i];
                if(nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE || grid[ny][nx]) continue;

                float tentG = gScore[{current.x, current.y}] + 1;
                if(gScore.find({nx,ny}) == gScore.end() || tentG < gScore[{nx,ny}]) {
                    gScore[{nx,ny}] = tentG;
                    parents[{nx,ny}] = {current.x, current.y};
                    open.push({nx, ny, tentG, dist(nx, ny, endX, endY), nullptr});
                }
            }
        }
    }

    void findPathJumpFill() {
        std::queue<SDL_Point> frontier;
        std::map<std::pair<int,int>, bool> visited;
        std::map<std::pair<int,int>, std::pair<int,int>> parents;

        frontier.push({startX, startY});
        visited[{startX, startY}] = true;

        bool found = false;
        while (!frontier.empty() && !found) {
            SDL_Point cur = frontier.front();
            frontier.pop();

            int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
            for (int i = 0; i < 4; ++i) {
                int nx = cur.x + dx[i];
                int ny = cur.y + dy[i];
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE || grid[ny][nx]) continue;
                if (visited[{nx, ny}]) continue;

                visited[{nx, ny}] = true;
                parents[{nx, ny}] = {cur.x, cur.y};

                if (nx == endX && ny == endY) {
                    found = true;
                    rebuildPathFromParents(parents, endX, endY);
                    break;
                }

                frontier.push({nx, ny});
            }
        }
    }

    void findPath() {
        path.clear();
        syncAlgorithmFromBrowser();
        if (algorithm == Algorithm::JumpFill) {
            findPathJumpFill();
        } else {
            findPathAStar();
        }
    }

    void update() override {
        Renderer* renderer = getRenderer();
        const int cellW = GRID_PIXELS / GRID_SIZE;
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);

        Algorithm browserAlgorithm = getBrowserAlgorithm();
        if (browserAlgorithm != algorithm) {
            algorithm = browserAlgorithm;
            findPath();
        }

        if (keystate[SDL_SCANCODE_1]) editMode = EditMode::Walls;
        if (keystate[SDL_SCANCODE_2]) editMode = EditMode::Start;
        if (keystate[SDL_SCANCODE_3]) editMode = EditMode::End;
        
        int mx, my;
        Uint32 mouseState = SDL_GetMouseState(&mx, &my);
        bool leftPressed = (mouseState & SDL_BUTTON(1)) != 0;
        bool rightPressed = (mouseState & SDL_BUTTON(3)) != 0;
        bool leftClicked = leftPressed && !leftWasPressed;
        bool rightClicked = rightPressed && !rightWasPressed;
        leftWasPressed = leftPressed;
        rightWasPressed = rightPressed;

        if (leftClicked || rightClicked) {
            if (mx >= OFFSET_X && mx < OFFSET_X + GRID_PIXELS &&
                my >= OFFSET_Y && my < OFFSET_Y + GRID_PIXELS) {
                int gx = (mx - OFFSET_X) / cellW;
                int gy = (my - OFFSET_Y) / cellW;

                if (rightClicked) {
                    if ((gx != startX || gy != startY) && (gx != endX || gy != endY)) {
                        grid[gy][gx] = 0;
                        findPath();
                    }
                } else {
                    if (editMode == EditMode::Start) {
                        if (gx != endX || gy != endY) {
                            startX = gx;
                            startY = gy;
                            grid[gy][gx] = 0;
                            findPath();
                        }
                    } else if (editMode == EditMode::End) {
                        if (gx != startX || gy != startY) {
                            endX = gx;
                            endY = gy;
                            grid[gy][gx] = 0;
                            findPath();
                        }
                    } else {
                        if((gx != startX || gy != startY) && (gx != endX || gy != endY)) {
                            grid[gy][gx] = 1;
                            findPath();
                        }
                    }
                }
            }
        }

        // Clear with visible dark blue background
        renderer->clearScreen(20, 30, 50, 255);

        // Draw grid in a fixed square region that fully fits the 800x600 viewport.
        for(int y=0; y<GRID_SIZE; y++) {
            for(int x=0; x<GRID_SIZE; x++) {
                SDL_Rect r = {OFFSET_X + x*cellW + 1, OFFSET_Y + y*cellW + 1, cellW - 2, cellW - 2};
                if(grid[y][x]) {
                    // Obstacles - bright red/brown
                    SDL_SetRenderDrawColor(renderer->renderer, 180, 60, 60, 255);
                } else {
                    // Empty cells - lighter blue
                    SDL_SetRenderDrawColor(renderer->renderer, 40, 50, 80, 255);
                }
                if(x == startX && y == startY) {
                    // Start - bright green
                    SDL_SetRenderDrawColor(renderer->renderer, 50, 255, 50, 255);
                }
                if(x == endX && y == endY) {
                    // End - bright red
                    SDL_SetRenderDrawColor(renderer->renderer, 255, 50, 50, 255);
                }
                SDL_RenderFillRect(renderer->renderer, &r);
            }
        }

        // Draw path - bright yellow
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 50, 255);
        for(auto p : path) {
            SDL_Rect r = {OFFSET_X + p.x*cellW + cellW/4, OFFSET_Y + p.y*cellW + cellW/4, cellW/2, cellW/2};
            SDL_RenderFillRect(renderer->renderer, &r);
        }

        // Draw instructions panel (top-right, semi-transparent black)
        if (showInstructions) {
            SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 200);
            SDL_Rect panel = {550, 10, 240, 220};
            SDL_RenderFillRect(renderer->renderer, &panel);
            
            // Draw panel border
            SDL_SetRenderDrawColor(renderer->renderer, 0, 217, 255, 255);
            SDL_RenderDrawRect(renderer->renderer, &panel);
            
            // Draw title
            wchar_t titleText[] = L"A* Pathfinding";
            Rect<float> titleBounds(560, 20, 200, 25);
            renderer->getTextWriter()->drawTextToRenderer(titleText, renderer->renderer, titleBounds, "/default.ttf");
            
            // Draw instructions
            const wchar_t* lines[] = {
                L"Green = Start point",
                L"Red = End point",
                L"Red-brown = Walls",
                L"Yellow = Path",
                L"",
                algorithm == Algorithm::JumpFill ? L"Algorithm: Jump Fill" : L"Algorithm: A*",
                L"(Use top-right dropdown)",
                L"",
                L"1: Wall mode (LMB place)",
                L"2: Set Start (LMB place)",
                L"3: Set End (LMB place)",
                L"RMB: Clear wall",
                L"",
                L"Press SPACE to hide/show"
            };
            
            int lineY = 55;
            for (const auto& line : lines) {
                Rect<float> lineBounds(560.0f, (float)lineY, 220.0f, 18.0f);
                renderer->getTextWriter()->drawTextToRenderer(line, renderer->renderer, lineBounds, "/default.ttf");
                lineY += 20;
            }
        }
        
        // Handle SPACE to toggle instructions
        static bool spaceWasPressed = false;
        if (keystate[SDL_SCANCODE_SPACE]) {
            if (!spaceWasPressed) {
                showInstructions = !showInstructions;
            }
            spaceWasPressed = true;
        } else {
            spaceWasPressed = false;
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    static NavGame app("Umbra Navigation A*", 800, 600);
    app.run();
    return 0;
}
