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

namespace VideoCommon::GPUThread {

/// Executes a single GPU thread command
static void ExecuteCommand(CommandData* command, VideoCore::RendererBase& renderer) {
    if (const auto submit_list = std::get_if<SubmitListCommand>(command)) {
        Pica::CommandProcessor::ProcessCommandList(submit_list->head, submit_list->length);
    } else if (const auto data = std::get_if<SwapBuffersCommand>(command)) {
        renderer.SwapBuffers();
        data->barrier->set_value();
    } else if (const auto data = std::get_if<MemoryFillCommand>(command)) {
        Pica::CommandProcessor::ProcessMemoryFill(data->config, data->is_second_filler);
    } else if (const auto data = std::get_if<DisplayTransferCommand>(command)) {
        Pica::CommandProcessor::ProcessDisplayTransfer(data->config);
    } else if (const auto data = std::get_if<FlushRegionCommand>(command)) {
        renderer.Rasterizer()->FlushRegion(data->addr, data->size);
        data->barrier->set_value();
    } else if (const auto data = std::get_if<InvalidateRegionCommand>(command)) {
        renderer.Rasterizer()->InvalidateRegion(data->addr, data->size);
    } else if (const auto data = std::get_if<FlushAndInvalidateRegionCommand>(command)) {
        renderer.Rasterizer()->FlushAndInvalidateRegion(data->addr, data->size);
        data->barrier->set_value();
    } else {
        UNREACHABLE();
    }
}

/// Runs the GPU thread
static void RunThread(VideoCore::RendererBase& renderer, SynchState& state) {

    MicroProfileOnThreadCreate("GpuThread");
    Common::SetCurrentThreadName("GpuThread");

    // auto WaitForWakeup = [&]() {
    //    std::unique_lock<std::mutex> lock{state.signal_mutex};
    //    state.signal_condition.wait(lock, [&] { return !state.IsIdle() || !state.is_running; });
    //};

    // Wait for first GPU command before acquiring the window context
    // WaitForWakeup();

    // If emulation was stopped during disk shader loading, abort before trying to acquire context
    if (!state.is_running) {
        return;
    }

    Frontend::ScopeAcquireWindowContext acquire_context{renderer.GetRenderWindow()};

    while (state.is_running) {
        if (!state.is_running) {
            return;
        }

        //{
        // Thread has been woken up, so make the previous write queue the next read queue
        //    std::lock_guard<std::mutex> lock{state.signal_mutex};
        //    state.SwapQueues();
        //}

        //{
        //    std::lock_guard lock(state.idle_mutex);
        //    state.is_idle = false;
        // }

        // Execute all of the GPU commands
        // while (!state.->empty()) {
        // CommandData value;
        // while (state.command_queue.pop(value)) {
        //    ExecuteCommand(&value, renderer);
        //}

        ExecuteCommand(&state.command_queue.PopWait(), renderer);

        //   state.pop_queue->pop();
        //}

        // Signal that the GPU thread has finished processing commands
        // if (state.IsIdle()) {
        //     {
        //        std::lock_guard lock(state.idle_mutex);
        //       state.is_idle = true;
        //    }
        //    state.idle_condition.notify_one();
        //}

        // Wait for CPU thread to send more GPU commands
        // WaitForWakeup();
    }
}

ThreadManager::ThreadManager(VideoCore::RendererBase& renderer) : renderer{renderer} {
    thread = std::make_unique<std::thread>(RunThread, std::ref(renderer), std::ref(state));
    thread_id = thread->get_id();
}

ThreadManager::~ThreadManager() {
    //{
    // Notify GPU thread that a shutdown is pending
    //    std::lock_guard<std::mutex> lock{state.signal_mutex};
    //    state.is_running = false;
    //}

    // state.signal_condition.notify_one();
    thread->join();
}

void ThreadManager::SubmitList(const u32* head, u32 length) {
    if (length == 0) {
        return;
    }

    PushCommand(SubmitListCommand(head, length), false);
}

void ThreadManager::SwapBuffers() {
    std::promise<void> barrier;
    std::future<void> future = barrier.get_future();
    PushCommand(SwapBuffersCommand{&barrier}, &future);
}

void ThreadManager::DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    PushCommand(DisplayTransferCommand{config});
}

void ThreadManager::MemoryFill(const GPU::Regs::MemoryFillConfig& config, bool is_second_filler) {
    PushCommand(MemoryFillCommand{config, is_second_filler});
}

void ThreadManager::FlushRegion(VAddr addr, u64 size) {
    std::promise<void> barrier;
    std::future<void> future = barrier.get_future();
    PushCommand(FlushRegionCommand(addr, size, &barrier), &future);
}

void ThreadManager::InvalidateRegion(VAddr addr, u64 size) {
    PushCommand(InvalidateRegionCommand(addr, size));
}

void ThreadManager::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    std::promise<void> barrier;
    std::future<void> future = barrier.get_future();
    PushCommand(FlushAndInvalidateRegionCommand(addr, size, &barrier), &future);
}

void ThreadManager::PushCommand(CommandData&& command_data, std::future<void>* wait_for_idle) {
    // Push the command to the GPU thread
    // state.command_queue.push(std::move(command_data));
    state.command_queue.Push(std::move(command_data));

    // Signal the GPU thread that commands are pending
    // state.signal_condition.notify_one();

    if (wait_for_idle != nullptr) {
        wait_for_idle->wait();
        // Wait for the GPU to be idle (all commands to be executed)
        // std::unique_lock<std::mutex> lock{state.idle_mutex};

        // for (int i = 10; i > 0; i--)
        //    HLE::g_hle_lock.unlock();

        // for (int i = 10; i > 0; i--)
        //    HLE::g_hle_lock.lock();
    }
}

} // namespace VideoCommon::GPUThread
