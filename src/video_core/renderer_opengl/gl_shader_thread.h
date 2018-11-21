// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <future>
#include <string>
#include <thread>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"

struct ShaderProgram {};

struct GLSLProgram {
    std::string vs;
    std::string gs;
    std::string fs;
};

class ShaderCompileThread {
public:
    explicit ShaderCompileThread(bool, u64);

private:
    //
    Common::SPSCQueue<std::packaged_task<GLSLProgram(ShaderProgram)>> decompile_work_queue;

    Common::SPSCQueue<std::packaged_task<u32(GLSLProgram)>> compile_shader_queue;
    // Coordinates and launches the other threads.
    std::thread main_thread;
    // Generates GLSL VS + GS source from registers
    std::thread decompile_thread;
    // Has the GraphicsContext current in order to load and compile programs
    std::thread gl_thread;
};
