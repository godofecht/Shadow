// Slime Mold Morphogenesis - UI Version with Sliders & Buttons
// Full interactive control panel for parameter tuning

#include "Engine/Core/Game2D.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <random>
#include <array>
#include <memory>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <SDL2/SDL_scancode.h>

// ============== UI Components ==============

struct UISlider {
    std::string label;
    float* value;
    float min, max;
    int x, y, width, height;
    bool dragging = false;
    
    UISlider(const std::string& l, float* v, float mn, float mx, int _x, int _y)
        : label(l), value(v), min(mn), max(mx), x(_x), y(_y), width(150), height(20) {}
    
    bool contains(int mx, int my) {
        return mx >= x && mx <= x + width && my >= y && my <= y + height;
    }
    
    void setValueFromMouse(int mx) {
        float t = (float)(mx - x) / width;
        t = std::max(0.0f, std::min(1.0f, t));
        *value = min + t * (max - min);
    }
    
    void render(SDL_Renderer* renderer, [[maybe_unused]] SDL_Color normal, SDL_Color highlight) {
        // Background
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
        SDL_Rect bg = {x, y, width, height};
        SDL_RenderFillRect(renderer, &bg);
        
        // Fill
        float t = (*value - min) / (max - min);
        SDL_SetRenderDrawColor(renderer, highlight.r, highlight.g, highlight.b, 255);
        SDL_Rect fill = {x, y, (int)(width * t), height};
        SDL_RenderFillRect(renderer, &fill);
        
        // Border
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &bg);
    }
};

struct UIButton {
    std::string label;
    std::function<void()> onClick;
    int x, y, width, height;
    bool hovered = false;
    
    UIButton(const std::string& l, std::function<void()> cb, int _x, int _y, int w, int h)
        : label(l), onClick(std::move(cb)), x(_x), y(_y), width(w), height(h) {}
    
    bool contains(int mx, int my) {
        return mx >= x && mx <= x + width && my >= y && my <= y + height;
    }
    
    void render(SDL_Renderer* renderer) {
        SDL_Color color = hovered ? SDL_Color{100, 200, 100, 255} : SDL_Color{60, 60, 60, 200};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect r = {x, y, width, height};
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &r);
        drawSimpleText(renderer, x + 10, y + std::max(2, (height - 14) / 2), label, {235, 235, 235, 255}, 0.72f);
    }
};

// ============== Simulation Config ==============

struct SimConfig {
    float sensorAngle = 0.785f;
    float sensorDist = 9.0f;
    float rotation = 0.35f;
    float speed = 1.5f;
    float decayRate = 0.90f;
    float depositRate = 0.15f;
    float maxDensity = 0.8f;
    float lowDensity = 0.02f;
    float highDensity = 0.06f;
    float diffuseCenter = 1.0f;
    float diffuseCardinal = 0.2f;
    float diffuseNormalizer = 2.0f;
    int agentsPerCell = 20;
    int diffuseSkip = 2;
    int width = 800;
    int height = 600;
    int colorScheme = 0;
    
    void save(const std::string& path) {
        std::ofstream f(path);
        f << std::fixed << std::setprecision(3);
        f << "sensorAngle=" << sensorAngle << "\n"
          << "sensorDist=" << sensorDist << "\n"
          << "rotation=" << rotation << "\n"
          << "speed=" << speed << "\n"
          << "decayRate=" << decayRate << "\n"
          << "depositRate=" << depositRate << "\n"
          << "lowDensity=" << lowDensity << "\n"
          << "highDensity=" << highDensity << "\n"
          << "diffuseSkip=" << diffuseSkip << "\n"
          << "agentsPerCell=" << agentsPerCell << "\n"
          << "colorScheme=" << colorScheme << "\n";
    }
    
    void load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            float val = std::stof(line.substr(eq+1));
            std::string key = line.substr(0, eq);
            if (key == "sensorAngle") sensorAngle = val;
            else if (key == "sensorDist") sensorDist = val;
            else if (key == "rotation") rotation = val;
            else if (key == "speed") speed = val;
            else if (key == "decayRate") decayRate = val;
            else if (key == "depositRate") depositRate = val;
            else if (key == "lowDensity") lowDensity = val;
            else if (key == "highDensity") highDensity = val;
            else if (key == "diffuseSkip") diffuseSkip = (int)val;
            else if (key == "agentsPerCell") agentsPerCell = (int)val;
            else if (key == "colorScheme") colorScheme = (int)val;
        }
    }
};

static SimConfig config;

// ============== Trail Map ==============

class TrailMap {
public:
    int width, height;
    std::vector<std::array<float, 2>> grid;
    std::vector<std::array<float, 2>> nextGrid;

    TrailMap(int w, int h) : width(w), height(h) {
        grid.resize(static_cast<std::size_t>(height) * static_cast<std::size_t>(width), {0.0f, 0.0f});
        nextGrid.resize(static_cast<std::size_t>(height) * static_cast<std::size_t>(width), {0.0f, 0.0f});
    }

    // Row-major flat index, computed in size_t so the multiplication can't overflow
    static std::size_t flat(int x, int y, int w) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
    }

    void deposit(int x, int y) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            float& d = grid[flat(x, y, width)][0];
            d = std::min(config.maxDensity, d + config.depositRate);
        }
    }

    void diffuseAndDecay() {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float c = grid[flat(x, y, width)][0];
                int ym = (y - 1 + height) % height, yp = (y + 1) % height;
                int xm = (x - 1 + width) % width, xp = (x + 1) % width;
                float n = grid[flat(x, ym, width)][0], e = grid[flat(xp, y, width)][0];
                float s = grid[flat(x, yp, width)][0], w = grid[flat(xm, y, width)][0];
                float diff = (config.diffuseCenter * c + config.diffuseCardinal * (n+e+s+w)) / config.diffuseNormalizer;
                nextGrid[flat(x, y, width)][0] = diff * config.decayRate;
            }
        }
        std::swap(grid, nextGrid);
    }

    float sample(float x, float y) const {
        int ix = ((static_cast<int>(x) % width) + width) % width;
        int iy = ((static_cast<int>(y) % height) + height) % height;
        return grid[flat(ix, iy, width)][0];
    }
};

// ============== Agent ==============

struct Agent {
    float x, y, heading;
    Agent(float _x, float _y, float _h) : x(_x), y(_y), heading(_h) {}

    void senseAndSteer(const TrailMap& t) {
        float l = t.sample(x + cos(heading - config.sensorAngle) * config.sensorDist,
                           y + sin(heading - config.sensorAngle) * config.sensorDist);
        float f = t.sample(x + cos(heading) * config.sensorDist,
                           y + sin(heading) * config.sensorDist);
        float r = t.sample(x + cos(heading + config.sensorAngle) * config.sensorDist,
                           y + sin(heading + config.sensorAngle) * config.sensorDist);
        if (l < config.lowDensity && f < config.highDensity && config.highDensity <= r) heading += config.rotation;
        else if (r < config.lowDensity && f < config.highDensity && config.highDensity <= l) heading -= config.rotation;
        else if (config.highDensity <= l && f < config.highDensity && config.highDensity <= r)
            heading += (cos(11131.3f*x)*cos(7131.3f*y) > 0 ? 1.0f : -1.0f) * config.rotation;
    }

    void move(int w, int h) {
        x += cos(heading) * config.speed;
        y += sin(heading) * config.speed;
        if (x < 0) x += w; if (x >= w) x -= w;
        if (y < 0) y += h; if (y >= h) y -= h;
    }
};

// ============== Main Game ==============

class SlimeMoldGameUI : public Game2D {
    std::unique_ptr<TrailMap> trails;
    std::vector<Agent> agents;
    int fps, frameCount, diffuseCounter, screenshotCount;
    Uint32 lastFpsTime;
    SDL_Surface* surface;
    SDL_Texture* texture;
    
    // UI State
    std::vector<UISlider> uiSliders;
    std::vector<UIButton> uiButtons;
    std::vector<std::shared_ptr<TextDisplay>> uiTextLabels;
    bool showPanel = true;
    int panelWidth = 280;
    std::shared_ptr<TextDisplay> fpsText, statsText;
    
    // Slider value pointers
    float* pDecay = &config.decayRate;
    float* pDeposit = &config.depositRate;
    float* pSpeed = &config.speed;
    float* pSensorDist = &config.sensorDist;
    float* pRotation = &config.rotation;

public:
    SlimeMoldGameUI() : Game2D("Slime Mold UI", config.width, config.height, 1),
                        fps(0), frameCount(0), diffuseCounter(0), screenshotCount(0),
                        lastFpsTime(0), surface(nullptr), texture(nullptr) {
        
        config.load("slime_config.txt");
        
        int n = (config.width * config.height) / config.agentsPerCell;
        trails = std::make_unique<TrailMap>(config.width, config.height);
        std::srand(static_cast<unsigned>(std::random_device{}()));
        
        std::vector<std::pair<float,float>> centers = {
            {config.width*0.25f, config.height*0.25f}, {config.width*0.75f, config.height*0.25f},
            {config.width*0.50f, config.height*0.50f}, {config.width*0.25f, config.height*0.75f}, {config.width*0.75f, config.height*0.75f}
        };
        for (auto& c : centers)
            for (int i = 0; i < n/5; ++i) {
                float a = (rand()%100/100.0f) * static_cast<float>(2.0 * M_PI), s = 40;
                agents.emplace_back(c.first+(rand()%100/100.0f-0.5f)*s, c.second+(rand()%100/100.0f-0.5f)*s, a);
            }
    }

    ~SlimeMoldGameUI() override { if(surface)SDL_FreeSurface(surface); if(texture)SDL_DestroyTexture(texture); }

    void initGame() override {
        surface = SDL_CreateRGBSurface(0, config.width, config.height, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
        texture = SDL_CreateTextureFromSurface(getRenderer()->renderer, surface);
        
        // Create UI panel - positions are relative to panel origin (px, py)
        int px = config.width - panelWidth - 10;
        int py = 10;
        int lh = 25;

        // Sliders - store their Y positions for text labels
        uiSliders.emplace_back("Decay Rate", pDecay, 0.5f, 0.99f, px, py += lh);
        uiSliders.emplace_back("Deposit Rate", pDeposit, 0.01f, 0.5f, px, py += lh);
        uiSliders.emplace_back("Speed", pSpeed, 0.5f, 3.0f, px, py += lh);
        uiSliders.emplace_back("Sensor Dist", pSensorDist, 5.0f, 20.0f, px, py += lh);
        uiSliders.emplace_back("Rotation", pRotation, 0.1f, 0.8f, px, py += lh);
        
        int slidersEndY = py;  // Remember where sliders end

        py += 10;

        // Buttons
        uiButtons.emplace_back("Reset", [this](){
            std::fill(trails->grid.begin(), trails->grid.end(), std::array<float,2>{0,0});
            std::cout << "Reset\n";
        }, px, py += lh, 130, 30);

        uiButtons.emplace_back("Screenshot", [this](){
            saveScreenshot();
        }, px + 140, py, 130, 30);

        uiButtons.emplace_back("Save Config", [](){
            config.save("slime_config.txt");
            std::cout << "Config saved\n";
        }, px, py += lh + 5, 130, 30);

        uiButtons.emplace_back("Toggle Panel", [this](){
            showPanel = !showPanel;
            std::cout << "Panel: " << (showPanel ? "ON" : "OFF") << "\n";
        }, px + 140, py, 130, 30);
        
        int buttonsRow1EndY = py;

        py += 10;

        // Color scheme buttons
        uiButtons.emplace_back("C:0", [](){ config.colorScheme = 0; }, px, py += lh + 5, 60, 25);
        uiButtons.emplace_back("C:1", [](){ config.colorScheme = 1; }, px + 65, py, 60, 25);
        uiButtons.emplace_back("C:2", [](){ config.colorScheme = 2; }, px + 130, py, 60, 25);
        uiButtons.emplace_back("C:3", [](){ config.colorScheme = 3; }, px + 195, py, 60, 25);
        
        int colorRowY = py;

        py += 10;

        // Preset buttons
        uiButtons.emplace_back("Fast", [this](){ loadPreset("config_fast.txt"); }, px, py += lh + 5, 130, 25);
        uiButtons.emplace_back("Organic", [this](){ loadPreset("config_organic.txt"); }, px + 140, py, 130, 25);
        uiButtons.emplace_back("Chaotic", [this](){ loadPreset("config_chaotic.txt"); }, px, py += 30, 130, 25);
        uiButtons.emplace_back("Randomize", [this](){ randomizeAgents(); }, px + 140, py, 130, 25);
        
        int presetsEndY = py;

        // Stats display - anchored to bottom of panel
        fpsText = createText(px + 10, config.height - 52, "FPS: --");
        statsText = createText(px + 10, config.height - 32, "Agents: --");
        
        // Panel toggle hint (shown when panel is hidden)
        createText(config.width - 92, 9, "[P] Show Panel");
        
        // Store panel metrics for dynamic text positioning
        panelMetrics = {
            .px = px,
            .slidersEndY = slidersEndY,
            .buttonsRow1EndY = buttonsRow1EndY,
            .colorRowY = colorRowY,
            .presetsEndY = presetsEndY
        };
        
        // Create text labels with relative positioning
        createTextLabels();

        // Keyboard bindings
        bindKey(KEY_R).onPress([this](){ std::fill(trails->grid.begin(), trails->grid.end(), std::array<float,2>{0,0}); });
        bindKey(KEY_S).onPress([this](){ saveScreenshot(); });
        bindKey(KEY_P).onPress([this](){ showPanel = !showPanel; std::cout << "Panel: " << (showPanel ? "ON" : "OFF") << "\n"; });
        bindKey(KEY_TAB).onPress([this](){ showPanel = !showPanel; });

        std::cout << "Slime Mold UI - Mouse for controls, P/TAB to toggle panel\n";
    }
    
    struct PanelMetrics {
        int px, slidersEndY, buttonsRow1EndY, colorRowY, presetsEndY;
    } panelMetrics;
    
    void createTextLabels() {
        int px = panelMetrics.px;
        
        // Slider labels - LEFT of sliders, vertically centered (sliders are 20px high)
        // Use integer positions for crisp text rendering
        createText(px - 65, 37, "Decay:");
        createText(px - 65, 62, "Deposit:");
        createText(px - 65, 87, "Speed:");
        createText(px - 65, 112, "Sensor:");
        createText(px - 65, 137, "Rotation:");
        
        // Slider values - RIGHT of sliders, vertically centered
        createText(px + 158, 37, "0.90");
        createText(px + 158, 62, "0.15");
        createText(px + 158, 87, "1.50");
        createText(px + 158, 112, "9.00");
        createText(px + 158, 137, "0.35");
        
    }
    
    void loadPreset(const std::string& path) {
        config.load(path);
        std::cout << "Loaded: " << path << "\n";
        // Reinitialize agents with new count
        agents.clear();
        int n = (config.width * config.height) / config.agentsPerCell;
        std::vector<std::pair<float,float>> centers = {
            {config.width*0.25f, config.height*0.25f}, {config.width*0.75f, config.height*0.25f},
            {config.width*0.50f, config.height*0.50f}, {config.width*0.25f, config.height*0.75f}, {config.width*0.75f, config.height*0.75f}
        };
        for (auto& c : centers)
            for (int i = 0; i < n/5; ++i) {
                float a = (rand()%100/100.0f) * static_cast<float>(2.0 * M_PI), s = 40;
                agents.emplace_back(c.first+(rand()%100/100.0f-0.5f)*s, c.second+(rand()%100/100.0f-0.5f)*s, a);
            }
    }
    
    void randomizeAgents() {
        for (auto& a : agents) {
            a.x = (rand() % 100 / 100.0f) * config.width;
            a.y = (rand() % 100 / 100.0f) * config.height;
            a.heading = (rand() % 100 / 100.0f) * static_cast<float>(2.0 * M_PI);
        }
        std::fill(trails->grid.begin(), trails->grid.end(), std::array<float,2>{0,0});
    }

    void saveScreenshot() {
        std::ostringstream ss;
        ss << "slime_" << std::setw(4) << std::setfill('0') << screenshotCount++ << ".bmp";
        SDL_SaveBMP(surface, ss.str().c_str());
        std::cout << "Saved: " << ss.str() << "\n";
    }
    
    void handleMouseInput() {
        int mx, my;
        Uint32 buttons = SDL_GetMouseState(&mx, &my);
        
        // Check if mouse is in panel area
        if (!showPanel || mx < config.width - panelWidth - 10) return;
        
        // Handle slider dragging
        for (auto& slider : uiSliders) {
            if (buttons & SDL_BUTTON(1)) {
                if (slider.contains(mx, my)) {
                    slider.dragging = true;
                    slider.setValueFromMouse(mx);
                }
            } else {
                slider.dragging = false;
            }
            if (slider.dragging) {
                slider.setValueFromMouse(mx);
            }
        }
        
        // Handle button hover/click
        for (auto& btn : uiButtons) {
            btn.hovered = btn.contains(mx, my);
            if (buttons & SDL_BUTTON(1) && btn.hovered) {
                btn.onClick();
                SDL_Delay(150); // Prevent double-click
                break;
            }
        }
    }

    void update() override {
        Game2D::update();
        handleMouseInput();
        
        for (auto& a : agents) { a.senseAndSteer(*trails); a.move(config.width, config.height); trails->deposit(static_cast<int>(a.x), static_cast<int>(a.y)); }
        if (++diffuseCounter >= config.diffuseSkip) { trails->diffuseAndDecay(); diffuseCounter = 0; }
        
        // Render trails
        Uint32* px = (Uint32*)surface->pixels;
        const std::size_t pixelCount = static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            float d = trails->grid[i][0];
            Uint8 r,g,b;
            
            switch (config.colorScheme) {
                case 1: r = static_cast<Uint8>(d * 255); g = static_cast<Uint8>((1-d) * 100); b = 50; break;
                case 2: r = g = b = static_cast<Uint8>(d * 255); break;
                case 3: r = static_cast<Uint8>(d * 255); g = static_cast<Uint8>((1-d) * 255); b = static_cast<Uint8>(128 + d * 127); break;
                default:
                    if (d < 0.05f) { r=static_cast<Uint8>(d*600); g=0; b=static_cast<Uint8>(d*1600); }
                    else if (d < 0.2f) { float t=(d-0.05f)/0.15f; r=static_cast<Uint8>(t*50); g=static_cast<Uint8>(t*150); b=static_cast<Uint8>(80+t*100); }
                    else if (d < 0.4f) { float t=(d-0.2f)/0.2f; r=static_cast<Uint8>(50+t*150); g=static_cast<Uint8>(150+t*100); b=static_cast<Uint8>(180-t*80); }
                    else { float t=std::min(1.0f,(d-0.4f)/0.4f); r=static_cast<Uint8>(200+t*55); g=static_cast<Uint8>(250+t*5); b=static_cast<Uint8>(100-t*50); }
            }
            px[i] = (255u<<24) | (static_cast<Uint32>(b)<<16u) | (static_cast<Uint32>(g)<<8u) | static_cast<Uint32>(r);
        }
        if (texture) SDL_DestroyTexture(texture);
        texture = SDL_CreateTextureFromSurface(getRenderer()->renderer, surface);
        
        // Update stats
        if (++frameCount >= 1000) { 
            Uint32 n = SDL_GetTicks(); 
            if (n - lastFpsTime >= 1000) {
                fps = frameCount; frameCount = 0; lastFpsTime = n;
            }
        }
    }
    
    void updateGame([[maybe_unused]] float dt) override {
        // Handle keyboard input for panel toggle
        if (input.isKeyPressed(KEY_P) || input.isKeyPressed(KEY_TAB)) {
            showPanel = !showPanel;
            std::cout << "Panel: " << (showPanel ? "ON" : "OFF") << "\n";
            // Debounce - wait for key release
            SDL_Delay(150);
        }
        
        // Reset with R key
        if (input.isKeyPressed(KEY_R)) {
            std::fill(trails->grid.begin(), trails->grid.end(), std::array<float,2>{0,0});
            std::cout << "Reset\n";
            SDL_Delay(150);
        }
        
        // Screenshot with S key
        if (input.isKeyPressed(KEY_S)) {
            saveScreenshot();
            SDL_Delay(150);
        }
    }

    void renderGame() override {
        // Draw simulation
        SDL_Rect simRect = {0, 0, config.width, config.height};
        SDL_RenderCopy(getRenderer()->renderer, texture, nullptr, &simRect);

        if (!showPanel) {
            // Show a small hint when panel is hidden
            SDL_SetRenderDrawColor(getRenderer()->renderer, 0, 0, 0, 150);
            SDL_Rect hintBg = {config.width - 100, 5, 95, 25};
            SDL_RenderFillRect(getRenderer()->renderer, &hintBg);
            SDL_SetRenderDrawColor(getRenderer()->renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(getRenderer()->renderer, &hintBg);
            // Only render the panel hint text (index 0 = first text created after stats)
            // Stats are at end, panel hint is at index textDisplays.size()-3
            if (textDisplays.size() >= 3) {
                textDisplays[textDisplays.size() - 3]->render(getRenderer());
            }
            return;
        }

        // Draw panel background
        int px = panelMetrics.px;
        SDL_SetRenderDrawColor(getRenderer()->renderer, 30, 30, 30, 220);
        SDL_Rect panelBg = {px - 5, 5, panelWidth + 10, config.height - 10};
        SDL_RenderFillRect(getRenderer()->renderer, &panelBg);
        
        // Draw panel border
        SDL_SetRenderDrawColor(getRenderer()->renderer, 80, 80, 80, 255);
        SDL_RenderDrawRect(getRenderer()->renderer, &panelBg);

        // Draw sliders
        SDL_Color sliderNormal = {100, 150, 255, 255};
        SDL_Color sliderActive = {150, 200, 255, 255};
        
        for (auto& slider : uiSliders) {
            slider.render(getRenderer()->renderer, sliderNormal, slider.dragging ? sliderActive : sliderNormal);
        }
        
        // Draw buttons
        for (auto& btn : uiButtons) {
            btn.render(getRenderer()->renderer);
        }
        
        // Render all text labels except the panel hint
        for (size_t i = 0; i < textDisplays.size() - 2; i++) {
            textDisplays[i]->render(getRenderer());
        }
        
        // Update FPS and stats text (last 2 textDisplays)
        if (textDisplays.size() >= 2) {
            textDisplays[textDisplays.size() - 2]->setText("FPS: " + std::to_string(fps));
            textDisplays[textDisplays.size() - 1]->setText("Agents: " + std::to_string(agents.size()));
        }
    }
};

int main(int argc, char* args[]) { 
    try {
        if (argc > 1) { config.load(args[1]); std::cout << "Loaded: " << args[1] << "\n"; }
        SlimeMoldGameUI app; 
        app.run(); 
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    return 0; 
}
