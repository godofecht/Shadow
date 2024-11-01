#pragma once

#include <SDL.h>

#ifdef _WIN32
#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#include <wincodec.h>  // For IWICBitmap and IWICBitmapLock
#include <string>
#include <SDL_syswm.h>  // Required for SDL_SysWMinfo

class Col 
{
public:
    static D2D1::ColorF green() { return D2D1::ColorF(D2D1::ColorF::Green); }
    static D2D1::ColorF yellow() { return D2D1::ColorF(D2D1::ColorF::Yellow); }
    static D2D1::ColorF red() { return D2D1::ColorF(D2D1::ColorF::Red); }
    static D2D1::ColorF blue() { return D2D1::ColorF(D2D1::ColorF::Blue); }
    static D2D1::ColorF black() { return D2D1::ColorF(D2D1::ColorF::Black); }
    static D2D1::ColorF white() { return D2D1::ColorF(D2D1::ColorF::White); }
    static D2D1::ColorF transparent() { return D2D1::ColorF(0, 0, 0, 0); }  // Fully transparent
};

class TextWriter 
{
public:
    TextWriter(Renderer* sdlRenderer) : sdlRenderer(sdlRenderer->renderer) 
    {
        InitializeDirectWrite();
        CreateWICBitmap();
    }

    ~TextWriter() = default;

    void RenderTextToTexture(const std::wstring& text, SDL_Texture* texture, D2D1::ColorF color) 
    {
        // Begin drawing and clear the previous text with a transparent background
        bitmapRenderTarget->BeginDraw();

        // Clear only the render target with transparent color (Alpha=0)
        bitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));  // Fully transparent

        // Define text layout
        D2D1_RECT_F layoutRect = D2D1::RectF(0.0f, 0.0f, static_cast<FLOAT>(bitmapWidth), static_cast<FLOAT>(bitmapHeight));

        // Set the brush color
        brush->SetColor(color);

        // Render text directly on top of the transparent cleared background
        bitmapRenderTarget->DrawText(text.c_str(), text.size(), textFormat.Get(), layoutRect, brush.Get());

        // Finish drawing
        HRESULT hr = bitmapRenderTarget->EndDraw();
        if (FAILED(hr)) 
        {
            SDL_Log("Direct2D drawing failed.");
        }

        // Copy the WIC bitmap data to the SDL texture
        CopyWICBitmapToSDLTexture(texture);
    }

private:
    SDL_Renderer* sdlRenderer;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> bitmapRenderTarget;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<IWICBitmap> wicBitmap;
    int bitmapWidth = 800;
    int bitmapHeight = 600;

    void InitializeDirectWrite() 
    {
        D2D1_FACTORY_OPTIONS options = {};
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &options, reinterpret_cast<void**>(d2dFactory.GetAddressOf()));
        if (FAILED(hr)) {
            SDL_Log("Failed to create Direct2D factory.");
        }

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory);
        if (FAILED(hr)) {
            SDL_Log("Failed to create DirectWrite factory.");
        }

        hr = dwriteFactory->CreateTextFormat(
            L"Arial", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            36.0f,
            L"en-us",
            &textFormat
        );
        if (FAILED(hr)) {
            SDL_Log("Failed to create DirectWrite text format.");
        }

        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    void CreateWICBitmap() {
        Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (FAILED(hr)) {
            SDL_Log("Failed to create WIC Imaging Factory.");
        }

        hr = wicFactory->CreateBitmap(bitmapWidth, bitmapHeight, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap);
        if (FAILED(hr)) {
            SDL_Log("Failed to create WIC bitmap.");
        }

        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        hr = d2dFactory->CreateWicBitmapRenderTarget(wicBitmap.Get(), props, &bitmapRenderTarget);
        if (FAILED(hr)) {
            SDL_Log("Failed to create WIC bitmap render target.");
        }

        hr = bitmapRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);
        if (FAILED(hr)) {
            SDL_Log("Failed to create Direct2D brush.");
        }
    }

    void CopyWICBitmapToSDLTexture (SDL_Texture* texture) {
        Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
        WICRect rect = { 0, 0, bitmapWidth, bitmapHeight };
        HRESULT hr = wicBitmap->Lock (&rect, WICBitmapLockRead, &lock);
        if (FAILED (hr)) {
            SDL_Log ("Failed to lock WIC bitmap.");
            return;
        }

        UINT bufferSize = 0;
        BYTE* data = nullptr;
        hr = lock->GetDataPointer (&bufferSize, &data);
        if (FAILED (hr)) 
        {
            SDL_Log ("Failed to get WIC bitmap data pointer.");
            return;
        }

        void* pixels;
        int pitch;
        SDL_LockTexture (texture, nullptr, &pixels, &pitch);

        for (int y = 0; y < bitmapHeight; ++y) 
        {
            memcpy (static_cast<BYTE*>(pixels) + y * pitch, data + y * bitmapWidth * 4, bitmapWidth * 4);
        }
        SDL_UnlockTexture (texture);
    }
};
#endif // _WIN32
