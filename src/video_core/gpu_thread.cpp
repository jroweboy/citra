// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/core_timing.h"
#include "core/frontend/scope_acquire_window_context.h"
#include "core/hle/lock.h"
#include "core/settings.h"
#include "video_core/command_processor.h"
#include "video_core/gpu_thread.h"
#include "video_core/renderer_base.h"

namespace VideoCore::GPUThread {

/// Runs the GPU thread
static void RunThread(VideoCore::RendererBase& renderer, SynchState& state,
                      Core::TimingEventType* command_list_processing_event,
                      Core::TimingEventType* display_transfer_event,
                      Core::TimingEventType* memory_fill_event,
                      Core::TimingEventType* swap_buffers_event) {

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
            auto command = &next.data;
            auto fence = next.fence;
            if (const auto submit_list = std::get_if<SubmitListCommand>(command)) {
                Pica::CommandProcessor::ProcessCommandList(submit_list->head, submit_list->length);
                Core::System::GetInstance().CoreTiming().ScheduleEventThreadsafe(
                    0, command_list_processing_event, fence);
            } else if (const auto data = std::get_if<SwapBuffersCommand>(command)) {
                renderer.SwapBuffers();
                Core::System::GetInstance().CoreTiming().ScheduleEventThreadsafe(
                    0, swap_buffers_event, fence);
            } else if (const auto data = std::get_if<MemoryFillCommand>(command)) {
                Pica::CommandProcessor::ProcessMemoryFill(*(data->config));
                auto fence_and_bool = data->is_second_filler; // fence &
                Core::System::GetInstance().CoreTiming().ScheduleEventThreadsafe(
                    0, memory_fill_event, fence_and_bool);
            } else if (const auto data = std::get_if<DisplayTransferCommand>(command)) {
                Pica::CommandProcessor::ProcessDisplayTransfer(*(data->config));
                Core::System::GetInstance().CoreTiming().ScheduleEventThreadsafe(
                    0, display_transfer_event, fence);
            } else if (const auto data = std::get_if<FlushRegionCommand>(command)) {
                renderer.Rasterizer()->FlushRegion(data->addr, data->size);
            } else if (const auto data = std::get_if<InvalidateRegionCommand>(command)) {
                renderer.Rasterizer()->InvalidateRegion(data->addr, data->size);
            } else if (const auto data = std::get_if<FlushAndInvalidateRegionCommand>(command)) {
                renderer.Rasterizer()->FlushAndInvalidateRegion(data->addr, data->size);
            } else {
                UNREACHABLE();
            }
            state.signaled_fence = next.fence;
            state.TrySynchronize();
        }
    }
}

ThreadManager::ThreadManager(Core::System& system, VideoCore::RendererBase& renderer)
    : system{system}, renderer{renderer} {
    swap_buffers_event =
        system.CoreTiming().RegisterEvent("GPUThreadSwapBuffers", [this](u64 fence, s64) {
            Pica::CommandProcessor::AfterSwapBuffers();
        });
    command_list_processing_event =
        system.CoreTiming().RegisterEvent("GPUThreadCommandListProcessing", [this](u64 fence, s64) {
            Pica::CommandProcessor::AfterCommandList();
        });
    display_transfer_event =
        system.CoreTiming().RegisterEvent("GPUThreadDisplayTransfer", [this](u64 fence, s64) {
            Pica::CommandProcessor::AfterDisplayTransfer();
        });
    memory_fill_event =
        system.CoreTiming().RegisterEvent("GPUThreadMemoryFill", [this](u64 fence, s64) {
            // Since we only get a single u64 in the callback, we store the bool as the most
            // significant bit on the fence
            const bool is_second_filler = fence; // & (1 << 63);
            // state.WaitForSynchronization(fence & (1 << 63));
            Pica::CommandProcessor::AfterMemoryFill(is_second_filler);
        });
    thread = std::make_unique<std::thread>(RunThread, std::ref(renderer), std::ref(state),
                                           command_list_processing_event, display_transfer_event,
                                           memory_fill_event, swap_buffers_event);
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

    const u64 fence{PushCommand(SubmitListCommand(head, length))};
    // state.WaitForSynchronization(fence);
    // const s64 synchronization_ticks{usToCycles(9000)};
    // system.CoreTiming().ScheduleEvent(synchronization_ticks, synchronization_event, fence);
}

void ThreadManager::SwapBuffers() {
    const u64 fence{PushCommand(SwapBuffersCommand{})};
    // state.WaitForSynchronization(fence);
}

void ThreadManager::DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    const u64 fence{PushCommand(DisplayTransferCommand{config})};
    // state.WaitForSynchronization(fence);
    // Pica::CommandProcessor::AfterDisplayTransfer(*config);
}

void ThreadManager::MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) {
    const u64 fence{PushCommand(MemoryFillCommand{config, is_second_filler})};
}

void ThreadManager::FlushRegion(VAddr addr, u64 size) {
    const u64 fence{PushCommand(FlushRegionCommand(addr, size))};
    state.WaitForSynchronization(fence);
}

void ThreadManager::InvalidateRegion(VAddr addr, u64 size) {
    const u64 fence{PushCommand(InvalidateRegionCommand(addr, size))};
    // state.WaitForSynchronization(fence);
}

void ThreadManager::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    const u64 fence{PushCommand(FlushAndInvalidateRegionCommand(addr, size))};
    state.WaitForSynchronization(fence);
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
