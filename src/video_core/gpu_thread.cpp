// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/frontend/scope_acquire_window_context.h"
#include "core/hle/lock.h"
#include "core/settings.h"
#include "video_core/gpu_thread.h"
#include "video_core/renderer_base.h"

namespace VideoCore::GPUThread {

/// Executes a single GPU thread command
inline void ExecuteCommand(CommandData* command, VideoCore::RendererBase& renderer) {
    if (const auto submit_list = std::get_if<SubmitListCommand>(command)) {
        Pica::CommandProcessor::ProcessCommandList(submit_list->head, submit_list->length);
    } else if (const auto data = std::get_if<SwapBuffersCommand>(command)) {
        renderer.SwapBuffers();
    } else if (const auto data = std::get_if<MemoryFillCommand>(command)) {
        Pica::CommandProcessor::ProcessMemoryFill(*(data->config), data->is_second_filler);
    } else if (const auto data = std::get_if<DisplayTransferCommand>(command)) {
        Pica::CommandProcessor::ProcessDisplayTransfer(*(data->config));
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

    // Wait for first GPU command before acquiring the window context
    state.WaitForCommands();

    // If emulation was stopped during disk shader loading, abort before trying to acquire context
    if (!state.is_running) {
        return;
    }

    Frontend::ScopeAcquireWindowContext acquire_context{renderer.GetRenderWindow()};

    CommandDataContainer next;
    while (state.is_running) {
        state.WaitForCommands();

        CommandDataContainer next;
        while (state.queue.Pop(next)) {
            ExecuteCommand(&next.data, renderer);
            state.signaled_fence = next.fence;
            state.TrySynchronize();
        }
    }
}

ThreadManager::ThreadManager(VideoCore::RendererBase& renderer) : renderer{renderer} {
    thread = std::make_unique<std::thread>(RunThread, std::ref(renderer), std::ref(state));
    thread_id = thread->get_id();
}

ThreadManager::~ThreadManager() {
    // Notify GPU thread that a shutdown is pending
    state.is_running.exchange(false);
    thread->join();
}

void ThreadManager::SubmitList(const u32* head, u32 length) {
    if (length == 0) {
        return;
    }

    PushCommand(SubmitListCommand(head, length));
}

void ThreadManager::SwapBuffers() {
    PushCommand(SwapBuffersCommand{});
}

void ThreadManager::DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    PushCommand(DisplayTransferCommand{config});
}

void ThreadManager::MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) {
    PushCommand(MemoryFillCommand{config, is_second_filler});
}

void ThreadManager::FlushRegion(VAddr addr, u64 size) {
    PushCommand(FlushRegionCommand(addr, size));
}

void ThreadManager::InvalidateRegion(VAddr addr, u64 size) {
    PushCommand(InvalidateRegionCommand(addr, size));
}

void ThreadManager::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    PushCommand(FlushAndInvalidateRegionCommand(addr, size));
}

u64 ThreadManager::PushCommand(CommandData&& command_data) {
    const u64 fence{++state.last_fence};
    state.queue.Push(CommandDataContainer(std::move(command_data), fence));
    state.SignalCommands();
    return fence;
}

MICROPROFILE_DEFINE(GPU_wait, "GPU", "Wait for the GPU", MP_RGB(128, 128, 192));
void SynchState::WaitForSynchronization(u64 fence) {
    if (signaled_fence >= fence) {
        return;
    }

    // Wait for the GPU to be idle (all commands to be executed)
    {
        MICROPROFILE_SCOPE(GPU_wait);
        std::unique_lock lock{synchronization_mutex};
        synchronization_condition.wait(lock, [this, fence] { return signaled_fence >= fence; });
    }
}

} // namespace VideoCore::GPUThread
