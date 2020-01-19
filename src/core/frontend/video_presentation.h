// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include "common/common_types.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/framebuffer_layout.h"

namespace Frontend {

enum class MessageType {
    ControllerConnected,
    SettingChanged,

    // This entry must be kept last so that other typed messages are
    // displayed before these messages
    Typeless,
};

enum class Position {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    CenterCenter,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

namespace Color {
constexpr u32 CYAN = 0xFF00FFFF;
constexpr u32 GREEN = 0xFF00FF00;
constexpr u32 RED = 0xFFFF0000;
constexpr u32 YELLOW = 0xFFFFFF30;
constexpr u32 WHITE = 0xFFFFFFFF;
}; // namespace Color

class OnScreenDisplay {
public:
    OnScreenDisplay();

    void AddMessage(std::string message, MessageType type = MessageType::Typeless,
                    std::chrono::milliseconds ms = std::chrono::seconds(2),
                    u32 rgba = Color::YELLOW);

    void ShowFPS(std::string message, std::function<std::string()> value_provider,
                 Position position = Position::TopLeft);

    void RemoveFPS();

    /**
     * Progress should auto hide after completion. If you need to cancel, make sure to update to
     * 100% so that it'll cancel
     */
    void ShowProgress(std::string message, std::function<std::tuple<u32, u32>()> value_provider,
                      Position position = Position::TopCenter);

private:
    friend class VideoPresentation;

    class MessageQueue;
    std::unique_ptr<MessageQueue> queue;

    void Render();
};

class VideoPresentation {
public:
    VideoPresentation();

    ~VideoPresentation();

    void Init();

    /**
     * Runs all stages of presentation and swaps on completion
     */
    void Present(const Layout::FramebufferLayout&);

    /**
     * Enable rendering the most recent guest frame
     */
    void EnableMailbox(std::shared_ptr<Frontend::TextureMailbox> mailbox);

    void ToggleOSD(bool osd);

    OnScreenDisplay& GetOSD();

    const OnScreenDisplay& GetOSD() const;

private:
    class Impl;

    std::unique_ptr<Impl> impl;
    std::shared_ptr<Frontend::TextureMailbox> mailbox;

    OnScreenDisplay osd;

    std::atomic<bool> osd_enabled;
};

} // namespace Frontend
