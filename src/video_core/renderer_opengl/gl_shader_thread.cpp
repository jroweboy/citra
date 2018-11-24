// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "video_core/renderer_opengl/gl_shader_thread.h"

ShaderCompileThread::ShaderCompileThread(bool program_binary_enabled) {
    Core::System::GetInstance().AddProcessChangedListener([this](const Core::System& system) {
        // cancel any active threads
        u64 program_id;
        system.GetAppLoader().ReadProgramId(program_id);
        // recreate the threads
        Init();
    });
    Init();
}

void ShaderCompileThread::Init() {

}
