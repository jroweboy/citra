// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <queue>
#include "video_core/command_processor.h"

namespace VideoCore {
class RendererBase;
}

namespace VideoCommon::GPUThread {

/// Command to signal to the GPU thread that a command list is ready for processing
struct SubmitListCommand final {
    explicit SubmitListCommand(Pica::CommandProcessor::CommandList&& entries)
        : entries{std::move(entries)} {}

    Pica::CommandProcessor::CommandList entries;
};

/// Command to signal to the GPU thread that a swap buffers is pending
struct SwapBuffersCommand final {
    SwapBuffersCommand() {}
};

struct MemoryFillCommand final {
    explicit constexpr MemoryFillCommand(GPU::Regs::MemoryFillConfig&& config,
                                         bool is_second_filler)
        : config{std::move(config)}, is_second_filler(is_second_filler) {}

    GPU::Regs::MemoryFillConfig config;
    bool is_second_filler;
};

struct DisplayTransferCommand final {
    explicit constexpr DisplayTransferCommand(GPU::Regs::DisplayTransferConfig&& config)
        : config{std::move(config)} {}

    GPU::Regs::DisplayTransferConfig config;
};

/// Command to signal to the GPU thread to flush a region
struct FlushRegionCommand final {
    explicit constexpr FlushRegionCommand(VAddr addr, u64 size) : addr{addr}, size{size} {}

    const VAddr addr;
    const u64 size;
};

/// Command to signal to the GPU thread to invalidate a region
struct InvalidateRegionCommand final {
    explicit constexpr InvalidateRegionCommand(VAddr addr, u64 size) : addr{addr}, size{size} {}

    const VAddr addr;
    const u64 size;
};

/// Command to signal to the GPU thread to flush and invalidate a region
struct FlushAndInvalidateRegionCommand final {
    explicit constexpr FlushAndInvalidateRegionCommand(VAddr addr, u64 size)
        : addr{addr}, size{size} {}

    const VAddr addr;
    const u64 size;
};

using CommandData =
    std::variant<SubmitListCommand, SwapBuffersCommand, MemoryFillCommand, DisplayTransferCommand,
                 FlushRegionCommand, InvalidateRegionCommand, FlushAndInvalidateRegionCommand>;

/// Struct used to synchronize the GPU thread
struct SynchState final {
    std::atomic<bool> is_running{true};
    std::atomic<bool> is_idle{true};
    std::condition_variable signal_condition;
    std::mutex signal_mutex;
    std::condition_variable idle_condition;
    std::mutex idle_mutex;

    // We use two queues for sending commands to the GPU thread, one for writing (push_queue) to and
    // one for reading from (pop_queue). These are swapped whenever the current pop_queue becomes
    // empty. This allows for efficient thread-safe access, as it does not require any copies.

    using CommandQueue = std::queue<CommandData>;
    std::array<CommandQueue, 2> command_queues;
    CommandQueue* push_queue{&command_queues[0]};
    CommandQueue* pop_queue{&command_queues[1]};

    /// Returns true if the GPU thread should be idle, meaning there are no commands to process
    bool IsIdle() const {
        return command_queues[0].empty() && command_queues[1].empty();
    }

    /// Swaps the write queue (push_queue) and the read queue (pop_queue)
    void SwapQueues() {
        CommandQueue* const next_push_queue = pop_queue;
        pop_queue = push_queue;
        push_queue = next_push_queue;
    }
};

/// Class used to manage the GPU thread
class ThreadManager final {
public:
    explicit ThreadManager(VideoCore::RendererBase& renderer);
    ~ThreadManager();

    /// Push GPU command entries to be processed
    void SubmitList(Pica::CommandProcessor::CommandList&& entries);

    /// Swap buffers (render frame)
    void SwapBuffers();

    void DisplayTransfer(GPU::Regs::DisplayTransferConfig&&);

    void MemoryFill(GPU::Regs::MemoryFillConfig&&, bool is_second_filler);

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    void FlushRegion(VAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be invalidated
    void InvalidateRegion(VAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be flushed and invalidated
    void FlushAndInvalidateRegion(VAddr addr, u64 size);

private:
    /// Pushes a command to be executed by the GPU thread
    void PushCommand(CommandData&& command_data, bool wait_for_idle, bool allow_on_cpu);

    /// Returns true if this is called by the GPU thread
    bool IsGpuThread() const {
        return std::this_thread::get_id() == thread_id;
    }

private:
    SynchState state;
    std::unique_ptr<std::thread> thread;
    std::thread::id thread_id{};
    VideoCore::RendererBase& renderer;
};

} // namespace VideoCommon::GPUThread
