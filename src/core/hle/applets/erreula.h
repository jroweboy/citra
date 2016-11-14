// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"

namespace HLE {
namespace Applets {

enum class ErrorType : u32 {
    ErrorCode,
    ErrorText,
    Eula,
    EulaFirstBoot,
    EulaDrawOnly,
    Agree,
    LocalizedErrorText = ErrorText | 0x100,
};

enum class ReturnCode : s32 {
    Unknown = -1,
    None,
    Success,
    NotSupported,
    HomeButton = 10,
    SoftwareReset,
    PowerButton
};

struct ErrEulaConfig {
    ErrorType error_type;
    u32 error_code;
    u16 upper_screen_flag;
    u16 use_language;
    char16_t error_text[1900];
    bool home_button;
    bool software_reset;
    bool app_jump;
    INSERT_PADDING_BYTES(137);
    ReturnCode return_code;
    u16 eula_version;
    INSERT_PADDING_BYTES(10);
};

static_assert(sizeof(ErrEulaConfig) == 0xF80, "ErrEulaConfig structure size is wrong");

class ErrEula final : public Applet {
public:
    explicit ErrEula(Service::APT::AppletId id) : Applet(id) {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;
    bool IsRunning() const override {
        return started;
    }

    ErrEulaConfig config;

    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

private:
    /// Whether this applet is currently running instead of the host application or not.
    bool started = false;
};

} // namespace Applets
} // namespace HLE
