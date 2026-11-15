#include "game.h"

#include <windows.h>
#include <d3d11.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <xinput.h>
#include <xaudio2.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {
constexpr float width = 1280, height = 720;

void check(HRESULT result) {
    if (FAILED(result)) throw std::runtime_error("A Windows graphics operation failed.");
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(window, message, wparam, lparam);
}

struct Buttons {
    bool up = false, down = false, left = false, right = false;
    bool fire = false, bomb = false, menu = false, start = false, join = false;
};

class Controls {
public:
    std::array<pluto::Input, pluto::max_devices> read() {
        std::array<pluto::Input, pluto::max_devices> result{};
        Buttons keyboard;
        const auto key = [](int code) { return (GetAsyncKeyState(code) & 0x8000) != 0; };
        keyboard.up = key('W') || key(VK_UP);
        keyboard.down = key('S') || key(VK_DOWN);
        keyboard.left = key('A') || key(VK_LEFT);
        keyboard.right = key('D') || key(VK_RIGHT);
        keyboard.fire = key(VK_CONTROL);
        keyboard.bomb = key(VK_SHIFT);
        keyboard.menu = key(VK_ESCAPE);
        keyboard.start = keyboard.join = key(VK_RETURN);
        update(0, keyboard, result[0]);
        result[0].x = static_cast<float>(keyboard.right) - static_cast<float>(keyboard.left);
        result[0].y = static_cast<float>(keyboard.down) - static_cast<float>(keyboard.up);

        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            XINPUT_STATE state{};
            Buttons buttons;
            float x = 0, y = 0;
            if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                const auto& pad = state.Gamepad;
                const auto pressed = [&pad](WORD button) { return (pad.wButtons & button) != 0; };
                const float stick_x = static_cast<float>(pad.sThumbLX) / 32767.0f;
                const float stick_y = static_cast<float>(pad.sThumbLY) / 32767.0f;
                x = std::abs(stick_x) > 0.25f ? std::clamp(stick_x, -1.0f, 1.0f) : 0;
                y = std::abs(stick_y) > 0.25f ? -std::clamp(stick_y, -1.0f, 1.0f) : 0;
                buttons.left = pressed(XINPUT_GAMEPAD_DPAD_LEFT) || x < -0.5f;
                buttons.right = pressed(XINPUT_GAMEPAD_DPAD_RIGHT) || x > 0.5f;
                buttons.up = pressed(XINPUT_GAMEPAD_DPAD_UP) || y < -0.5f;
                buttons.down = pressed(XINPUT_GAMEPAD_DPAD_DOWN) || y > 0.5f;
                if (pressed(XINPUT_GAMEPAD_DPAD_LEFT)) x = -1;
                if (pressed(XINPUT_GAMEPAD_DPAD_RIGHT)) x = 1;
                if (pressed(XINPUT_GAMEPAD_DPAD_UP)) y = -1;
                if (pressed(XINPUT_GAMEPAD_DPAD_DOWN)) y = 1;
                buttons.fire = pressed(XINPUT_GAMEPAD_A) || pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
                buttons.join = pressed(XINPUT_GAMEPAD_A);
                buttons.bomb = pressed(XINPUT_GAMEPAD_B) || pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
                buttons.start = pressed(XINPUT_GAMEPAD_START);
                buttons.menu = pressed(XINPUT_GAMEPAD_BACK);
            }
            update(i + 1, buttons, result[i + 1]);
            result[i + 1].x = x;
            result[i + 1].y = y;
        }
        return result;
    }

    void reset() { previous_ = {}; }

private:
    void update(std::size_t device, const Buttons& now, pluto::Input& out) {
        const auto& old = previous_[device];
        out.up = now.up && !old.up;
        out.down = now.down && !old.down;
        out.left = now.left && !old.left;
        out.right = now.right && !old.right;
        out.fire = now.fire;
        out.fire_pressed = now.fire && !old.fire;
        out.bomb = now.bomb && !old.bomb;
        out.menu = now.menu && !old.menu;
        out.start = now.start && !old.start;
        out.join = now.join && !old.join;
        previous_[device] = now;
    }
    std::array<Buttons, pluto::max_devices> previous_{};
};

class Renderer {
public:
    explicit Renderer(HWND window) : window_(window) {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &desc,
            &swap_chain_, &device_, nullptr, &context_);
        if (FAILED(result)) {
            check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &desc,
                &swap_chain_, &device_, nullptr, &context_));
        }
        check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_));
        check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(write_factory_.GetAddressOf())));
        check(write_factory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 28, L"en-us", &text_format_));
        check(text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    }

    void draw(const pluto::Game& game) {
        RECT area{};
        GetClientRect(window_, &area);
        const UINT w = area.right - area.left, h = area.bottom - area.top;
        if (!w || !h) return;
        if (w != pixel_width_ || h != pixel_height_ || !target_) resize(w, h);
        target_->BeginDraw();
        target_->SetTransform(D2D1::Matrix3x2F::Scale(w / width, h / height));
        target_->Clear(D2D1::ColorF(0.015f, 0.025f, 0.07f));
        switch (game.screen()) {
        case pluto::Screen::title: title(game); break;
        case pluto::Screen::select: select(game); break;
        case pluto::Screen::playing: playing(game); break;
        case pluto::Screen::options: options(game); break;
        case pluto::Screen::credits:
            text(L"Credits", 100, 100, 1000, 60);
            text(L"A game from Joel Ivory Johnson", 100, 200, 1000, 50);
            text(L"Fire or menu: return", 100, 600, 1000, 50);
            break;
        }
        const HRESULT result = target_->EndDraw();
        if (result == D2DERR_RECREATE_TARGET) { brush_.Reset(); target_.Reset(); return; }
        check(result);
        check(swap_chain_->Present(1, 0));
    }

private:
    void resize(UINT w, UINT h) {
        brush_.Reset();
        target_.Reset();
        check(swap_chain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0));
        ComPtr<IDXGISurface> surface;
        check(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface)));
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        check(factory_->CreateDxgiSurfaceRenderTarget(surface.Get(), &properties, &target_));
        check(target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_));
        pixel_width_ = w;
        pixel_height_ = h;
    }

    void text(const std::wstring& value, float x, float y, float w, float h,
              D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::White)) {
        brush_->SetColor(color);
        target_->DrawTextW(value.c_str(), static_cast<UINT32>(value.size()), text_format_.Get(),
            D2D1::RectF(x, y, x + w, y + h), brush_.Get());
    }
    void box(float x, float y, float w, float h, D2D1_COLOR_F color) {
        brush_->SetColor(color);
        target_->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush_.Get());
    }
    void title(const pluto::Game& game) {
        text(L"PLUTO FALL", 100, 70, 1000, 80, D2D1::ColorF(0.4f, 0.8f, 1.0f));
        text(L"A game from Joel Ivory Johnson", 100, 150, 1000, 50);
        const std::array<const wchar_t*, 4> names{L"Start game", L"View credits", L"Visit game website", L"Game options"};
        for (int i = 0; i < 4; ++i)
            text(std::wstring(i == game.menu_selection() ? L"> " : L"   ") + names[i], 400, 270.0f + i * 58.0f, 500, 48);
        text(L"Up/Down: select    Enter or Fire: confirm", 100, 630, 1100, 50);
    }
    void select(const pluto::Game& game) {
        text(L"PLAYER SELECT", 80, 40, 1000, 60);
        text(L"Enter / controller A: join    Left/Right: choose", 80, 115, 1100, 40);
        text(L"Ctrl / A: lock    Shift / B: unlock or leave", 80, 155, 1100, 40);
        for (std::size_t i = 0; i < game.players().size(); ++i) {
            const auto& player = game.players()[i];
            std::wstring line = L"Player " + std::to_wstring(i + 1) + L": ";
            if (player.joined)
                line += L"ship " + std::to_wstring(player.model + 1) + (player.ready ? L"  READY" : L"  choosing");
            else line += L"waiting";
            text(line, 170, 235.0f + static_cast<float>(i) * 70, 900, 50);
        }
        text(L"When everyone is ready, press Enter / Start", 80, 620, 1100, 50);
    }
    void options(const pluto::Game& game) {
        text(L"OPTIONS", 100, 100, 1000, 60);
        text(std::wstring(game.menu_selection() == 0 ? L"> " : L"   ") + L"Sound effects: " +
            std::to_wstring(static_cast<int>(std::round(game.effects_volume() * 100))) + L"%", 250, 240, 800, 50);
        text(std::wstring(game.menu_selection() == 1 ? L"> " : L"   ") + L"Music: " +
            std::to_wstring(static_cast<int>(std::round(game.music_volume() * 100))) + L"%", 250, 310, 800, 50);
        text(L"Up/Down: select    Left/Right: adjust    Esc / Back: return", 100, 600, 1100, 50);
    }
    void playing(const pluto::Game& game) {
        for (int i = 0; i < 25; ++i)
            box(static_cast<float>((i * 197) % 1200 + 30), static_cast<float>((i * 113) % 670 + 20), 2, 2,
                D2D1::ColorF(0.5f, 0.6f, 0.8f));
        for (const auto& enemy : game.enemies())
            box(enemy.x * width - 18, enemy.y * height - 18, 36, 36, D2D1::ColorF(0.95f, 0.25f, 0.25f));
        for (const auto& shot : game.projectiles())
            box(shot.x * width - 4, shot.y * height - 10, shot.bomb ? 14 : 8, shot.bomb ? 20 : 16,
                shot.bomb ? D2D1::ColorF(1.0f, 0.55f, 0.1f) : D2D1::ColorF(1.0f, 1.0f, 0.4f));
        const std::array<D2D1_COLOR_F, 4> colors{D2D1::ColorF(0.2f, 0.8f, 1.0f),
            D2D1::ColorF(0.3f, 1.0f, 0.4f), D2D1::ColorF(1.0f, 0.4f, 0.9f), D2D1::ColorF(1.0f, 0.8f, 0.3f)};
        for (std::size_t i = 0; i < game.players().size(); ++i) {
            const auto& player = game.players()[i];
            if (!player.joined) continue;
            box(player.x * width - 15, player.y * height - 15, 30, 30, colors[i]);
            text(L"P" + std::to_wstring(i + 1) + L" HP " + std::to_wstring(player.health) +
                L" Lives " + std::to_wstring(player.lives) + L" Bombs " + std::to_wstring(player.bombs),
                15, 8.0f + static_cast<float>(i) * 32, 500, 30, colors[i]);
        }
    }

    HWND window_;
    UINT pixel_width_ = 0, pixel_height_ = 0;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<ID2D1RenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<IDWriteFactory> write_factory_;
    ComPtr<IDWriteTextFormat> text_format_;
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com)) return 1;
    int exit_code = 0;
    try {
        WNDCLASSW cls{};
        cls.lpfnWndProc = window_proc;
        cls.hInstance = instance;
        cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        cls.lpszClassName = L"PlutoFallWindow";
        if (!RegisterClassW(&cls)) throw std::runtime_error("Unable to register the window.");
        HWND window = CreateWindowExW(0, cls.lpszClassName, L"Pluto Fall", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("Unable to create the window.");
        ShowWindow(window, show);
        {
            Renderer renderer(window);
            ComPtr<IXAudio2> audio;
            IXAudio2MasteringVoice* master = nullptr;
            if (SUCCEEDED(XAudio2Create(&audio))) audio->CreateMasteringVoice(&master);
            pluto::Game game;
            Controls controls;
            auto previous = std::chrono::steady_clock::now();
            MSG msg{};
            bool running = true;
            while (running) {
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) { running = false; break; }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                if (!running) break;
                const auto now = std::chrono::steady_clock::now();
                const float dt = std::chrono::duration<float>(now - previous).count();
                previous = now;
                if (GetForegroundWindow() == window && !IsIconic(window)) {
                    game.update(dt, controls.read());
                } else {
                    controls.reset();
                    Sleep(16);
                }
                if (game.take_website_request())
                    ShellExecuteW(window, L"open", L"https://j2i.net/apps/pluto-fall", nullptr, nullptr, SW_SHOWNORMAL);
                renderer.draw(game);
            }
            if (master) master->DestroyVoice();
        }
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"Pluto Fall could not initialize its window or graphics.", L"Pluto Fall", MB_OK | MB_ICONERROR);
        exit_code = 1;
    }
    CoUninitialize();
    return exit_code;
}
