// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Frontend {

class EmuWindow;

/// Helper class to acquire/release window context within a given scope
class ScopeAcquireWindowContext : NonCopyable {
public:
    explicit ScopeAcquireWindowContext(EmuWindow& window);
    ~ScopeAcquireWindowContext();

private:
    EmuWindow& emu_window;
};

} // namespace Frontend
