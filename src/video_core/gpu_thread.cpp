// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/frontend/scope_acquire_window_context.h"
#include "core/settings.h"
#include "video_core/gpu_thread.h"
#include "video_core/renderer_base.h"

namespace VideoCommon::GPUThread {

/// Executes a single GPU thread command
static void ExecuteCommand(CommandData* command, VideoCore::RendererBase& renderer) {
    if (const auto submit_list = std::get_if<SubmitListCommand>(command)) {
        Pica::CommandProcessor::ProcessCommandList(std::move(submit_list->entries));
    } else if (const auto data = std::get_if<SwapBuffersCommand>(command)) {
        renderer.SwapBuffers();
    } else if (const auto data = std::get_if<MemoryFillCommand>(command)) {
        Pica::CommandProcessor::ProcessMemoryFill(std::move(data->config), data->is_second_filler);
    } else if (const auto data = std::get_if<DisplayTransferCommand>(command)) {
        Pica::CommandProcessor::ProcessDisplayTransfer(std::move(data->config));
    } else if (const auto data = std::get_if<FlushRegionCommand>(command)) {
        renderer.Rasterizer()->FlushRegion(data->addr, data->size);
    } else if (const auto data = std::get_if<InvalidateRegionCommand>(command)) {
        renderer.Rasterizer()->InvalidateRegion(data->addr, data->size);
    } else if (const auto data = std::get_if<FlushAndInvalidateRegionCommand>(command)) {
        renderer.Rasterizer()->FlushAndInvalidateRegion(data->addr, data->size);
    } else {
        UNREACHABLE();
    }
}

/// Runs the GPU thread
static void RunThread(VideoCore::RendererBase& renderer, SynchState& state) {

    MicroProfileOnThreadCreate("GpuThread");
    Common::SetCurrentThreadName("GpuThread");

    auto WaitForWakeup = [&]() {
        std::unique_lock<std::mutex> lock{state.signal_mutex};
        state.signal_condition.wait(lock, [&] { return !state.IsIdle() || !state.is_running; });
    };

    // Wait for first GPU command before acquiring the window context
    WaitForWakeup();

    // If emulation was stopped during disk shader loading, abort before trying to acquire context
    if (!state.is_running) {
        return;
    }

    Frontend::ScopeAcquireWindowContext acquire_context{renderer.GetRenderWindow()};

    while (state.is_running) {
        if (!state.is_running) {
            return;
        }

        {
            // Thread has been woken up, so make the previous write queue the next read queue
            std::lock_guard<std::mutex> lock{state.signal_mutex};
            state.SwapQueues();
        }

        {
            std::lock_guard lock(state.idle_mutex);
            state.is_idle = false;
        }

        // Execute all of the GPU commands
        while (!state.pop_queue->empty()) {
            ExecuteCommand(&state.pop_queue->front(), renderer);
            state.pop_queue->pop();
        }

        // Signal that the GPU thread has finished processing commands
        if (state.IsIdle()) {
            {
                std::lock_guard lock(state.idle_mutex);
                state.is_idle = true;
            }
            state.idle_condition.notify_one();
        }

        // Wait for CPU thread to send more GPU commands
        WaitForWakeup();
    }
}

ThreadManager::ThreadManager(VideoCore::RendererBase& renderer) : renderer{renderer} {
    thread = std::make_unique<std::thread>(RunThread, std::ref(renderer), std::ref(state));
    thread_id = thread->get_id();
}

ThreadManager::~ThreadManager() {
    {
        // Notify GPU thread that a shutdown is pending
        std::lock_guard<std::mutex> lock{state.signal_mutex};
        state.is_running = false;
    }

    state.signal_condition.notify_one();
    thread->join();
}

void ThreadManager::SubmitList(Pica::CommandProcessor::CommandList&& entries) {
    if (entries.empty()) {
        return;
    }

    PushCommand(SubmitListCommand(std::move(entries)), false, false);
}

void ThreadManager::SwapBuffers() {
    PushCommand(SwapBuffersCommand{}, true, false);
}

void ThreadManager::DisplayTransfer(GPU::Regs::DisplayTransferConfig&& config) {
    PushCommand(DisplayTransferCommand{std::move(config)}, false, false);
}

void ThreadManager::MemoryFill(GPU::Regs::MemoryFillConfig&& config, bool is_second_filler) {
    PushCommand(MemoryFillCommand{std::move(config), is_second_filler}, false, false);
}

void ThreadManager::FlushRegion(VAddr addr, u64 size) {
    PushCommand(FlushRegionCommand(addr, size), true, false);
}

void ThreadManager::InvalidateRegion(VAddr addr, u64 size) {
    PushCommand(InvalidateRegionCommand(addr, size), false, false);
}

void ThreadManager::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    PushCommand(FlushAndInvalidateRegionCommand(addr, size), true, false);
}

void ThreadManager::PushCommand(CommandData&& command_data, bool wait_for_idle, bool allow_on_cpu) {
    {
        std::lock_guard<std::mutex> lock{state.signal_mutex};

        if ((allow_on_cpu && state.IsIdle()) || IsGpuThread()) {
            // Execute the command synchronously on the current thread
            ExecuteCommand(&command_data, renderer);
            return;
        }

        // Push the command to the GPU thread
        state.push_queue->emplace(command_data);
    }

    // Signal the GPU thread that commands are pending
    state.signal_condition.notify_one();

    if (wait_for_idle) {
        // Wait for the GPU to be idle (all commands to be executed)
        std::unique_lock<std::mutex> lock{state.idle_mutex};
        state.idle_condition.wait(lock, [this] {
            return state.IsIdle() || state.is_idle;
        });
    }
}

} // namespace VideoCommon::GPUThread
