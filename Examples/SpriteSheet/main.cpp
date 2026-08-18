// Sprite Sheet Showcase for Umbra Engine
// Renders an animated sprite from a sprite sheet
#include "Engine/Core/Game2D.h"
class SpriteSheetGame : public Game2D
{
    struct SpriteSheetAnimation {
        std::shared_ptr<SimpleSprite> sprite;
        float x, y;
        int currentFrame;
        int totalFrames;
        float frameDuration;
        float timer;
        bool isPlaying;
        int sheetRows, sheetCols;
        int frameWidth, frameHeight;
        std::string name;
    };
    
    std::vector<SpriteSheetAnimation> animations;

public:
    SpriteSheetGame() : Game2D("Umbra Sprite Sheet", 800, 600, 20) {}

    void initGame() override {
        // Create sprite sheet animations
        struct AnimConfig {
            float x, y;
            int rows, cols;
            float speed;
            const char* name;
        };
        
        AnimConfig configs[] = {
            {150, 150, 1, 4, 0.1f, "Walk Cycle"},
            {450, 150, 1, 6, 0.08f, "Attack"},
            {150, 350, 2, 4, 0.12f, "Jump"},
            {450, 350, 1, 8, 0.05f, "Run"},
            {350, 500, 2, 3, 0.15f, "Idle"}
        };
        
        for (int i = 0; i < 5; i++) {
            SpriteSheetAnimation anim;
            anim.x = configs[i].x;
            anim.y = configs[i].y;
            anim.sheetRows = configs[i].rows;
            anim.sheetCols = configs[i].cols;
            anim.frameDuration = configs[i].speed;
            anim.timer = 0;
            anim.currentFrame = 0;
            anim.totalFrames = configs[i].rows * configs[i].cols;
            anim.isPlaying = true;
            anim.frameWidth = 64;
            anim.frameHeight = 64;
            anim.name = configs[i].name;
            
            anim.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "sheet" + std::to_string(i));
            anim.sprite->setPosition(Point2D(anim.x, anim.y));
            anim.sprite->setSize((float)anim.frameWidth, (float)anim.frameHeight);
            
            animations.push_back(anim);
        }
    }

    void updateGame(float dt) override {
        for (auto& anim : animations) {
            if (anim.isPlaying) {
                anim.timer += dt;
                
                if (anim.timer >= anim.frameDuration) {
                    anim.timer -= anim.frameDuration;
                    anim.currentFrame = (anim.currentFrame + 1) % anim.totalFrames;
                }
                
                // Calculate UV coordinates for current frame
                int row = anim.currentFrame / anim.sheetCols;
                int col = anim.currentFrame % anim.sheetCols;
                
                // Note: In a full implementation, we'd set texture UV coordinates here
                // For this demo, we simulate by changing position slightly
                float offsetX = col * 5 - (anim.sheetCols * 5) / 2.0f;
                float offsetY = row * 5 - (anim.sheetRows * 5) / 2.0f;
                
                anim.sprite->setPosition(Point2D(anim.x + offsetX, anim.y + offsetY));
            }
            
            anim.sprite->update(dt);
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        for (auto& anim : animations) {
            // Draw sprite sheet frame indicator
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            
            // Draw frame strip
            int stripWidth = anim.sheetCols * 20;
            int stripX = (int)anim.x - stripWidth / 2;
            int stripY = (int)anim.y + 80;
            
            for (int i = 0; i < anim.totalFrames; i++) {
                SDL_Rect frameRect = {stripX + i * 20, stripY, 18, 18};
                if (i == anim.currentFrame) {
                    SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 0, 255);
                    SDL_RenderFillRect(renderer->renderer, &frameRect);
                } else {
                    SDL_SetRenderDrawColor(renderer->renderer, 150, 150, 150, 255);
                    SDL_RenderDrawRect(renderer->renderer, &frameRect);
                }
            }
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%hs (Frame %d/%d)", anim.name.c_str(), anim.currentFrame + 1, anim.totalFrames);
            Rect<float> textBounds(anim.x - 100.0f, (float)(stripY + 25), 220.0f, 30.0f);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
            
            anim.sprite->renderAndRunScripts(renderer);
        }
        
        // Draw info text
        std::wstring info = L"Sprite Sheet - Frame-based animation from sprite sheets";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteSheetGame app;
    app.run();
    return 0;
}
