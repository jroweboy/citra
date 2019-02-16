// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <boost/lockfree/spsc_queue.hpp>
#include "common/threadsafe_queue.h"
#include "video_core/command_processor.h"

namespace VideoCore {
class RendererBase;
}

namespace VideoCommon::GPUThread {

/// Command to signal to the GPU thread that a command list is ready for processing
struct SubmitListCommand {
    // In order for the variant to be default constructable, the first element needs a default
    // constructor
    constexpr SubmitListCommand() : head(nullptr), length(0) {}
    explicit constexpr SubmitListCommand(const u32* head, u32 length)
        : head(head), length(length) {}
    const u32* head;
    u32 length;
};

static_assert(std::is_copy_assignable<SubmitListCommand>::value,
              "SubmitListCommand is not copy assignable");
static_assert(std::is_copy_constructible<SubmitListCommand>::value,
              "SubmitListCommand is not copy constructable");

/// Command to signal to the GPU thread that a swap buffers is pending
struct SwapBuffersCommand final {
    explicit constexpr SwapBuffersCommand(std::promise<void>* barrier) : barrier{barrier} {}

    std::promise<void>* barrier;
};

static_assert(std::is_copy_assignable<SwapBuffersCommand>::value,
              "SwapBuffersCommand is not copy assignable");
static_assert(std::is_copy_constructible<SwapBuffersCommand>::value,
              "SwapBuffersCommand is not copy constructable");

struct MemoryFillCommand final {
    explicit constexpr MemoryFillCommand(const GPU::Regs::MemoryFillConfig* config,
                                         bool is_second_filler)
        : config{config}, is_second_filler(is_second_filler) {}

    const GPU::Regs::MemoryFillConfig* config;
    bool is_second_filler;
};

static_assert(std::is_copy_assignable<MemoryFillCommand>::value,
              "MemoryFillCommand is not copy assignable");
static_assert(std::is_copy_constructible<MemoryFillCommand>::value,
              "MemoryFillCommand is not copy constructable");

struct DisplayTransferCommand final {
    explicit constexpr DisplayTransferCommand(const GPU::Regs::DisplayTransferConfig* config)
        : config{config} {}

    const GPU::Regs::DisplayTransferConfig* config;
};
static_assert(std::is_copy_assignable<DisplayTransferCommand>::value,
              "DisplayTransferCommand is not copy assignable");
static_assert(std::is_copy_constructible<DisplayTransferCommand>::value,
              "DisplayTransferCommand is not copy constructable");

/// Command to signal to the GPU thread to flush a region
struct FlushRegionCommand final {
    explicit constexpr FlushRegionCommand(VAddr addr, u64 size, std::promise<void>* barrier)
        : addr{addr}, size{size}, barrier{barrier} {}

    VAddr addr;
    u64 size;
    std::promise<void>* barrier;
};
static_assert(std::is_copy_assignable<FlushRegionCommand>::value,
              "FlushRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<FlushRegionCommand>::value,
              "FlushRegionCommand is not copy constructable");

/// Command to signal to the GPU thread to invalidate a region
struct InvalidateRegionCommand final {
    explicit constexpr InvalidateRegionCommand(VAddr addr, u64 size) : addr{addr}, size{size} {}

    VAddr addr;
    u64 size;
};
static_assert(std::is_copy_assignable<InvalidateRegionCommand>::value,
              "InvalidateRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<InvalidateRegionCommand>::value,
              "InvalidateRegionCommand is not copy constructable");

/// Command to signal to the GPU thread to flush and invalidate a region
struct FlushAndInvalidateRegionCommand final {
    explicit constexpr FlushAndInvalidateRegionCommand(VAddr addr, u64 size,
                                                       std::promise<void>* barrier)
        : addr{addr}, size{size}, barrier{barrier} {}

    VAddr addr;
    u64 size;
    std::promise<void>* barrier;
};
static_assert(std::is_copy_assignable<FlushAndInvalidateRegionCommand>::value,
              "FlushAndInvalidateRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<FlushAndInvalidateRegionCommand>::value,
              "FlushAndInvalidateRegionCommand is not copy constructable");

using CommandData =
    std::variant<SubmitListCommand, SwapBuffersCommand, MemoryFillCommand, DisplayTransferCommand,
                 FlushRegionCommand, InvalidateRegionCommand, FlushAndInvalidateRegionCommand>;

/// Struct used to synchronize the GPU thread
struct SynchState final {
    std::atomic<bool> is_running{true};

    boost::lockfree::spsc_queue<CommandData, boost::lockfree::capacity<1024>> command_queue;
};

/// Class used to manage the GPU thread
class ThreadManager final {
public:
    explicit ThreadManager(VideoCore::RendererBase& renderer);
    ~ThreadManager();

    /// Push GPU command entries to be processed
    void SubmitList(const u32* head, u32 length);

    /// Swap buffers (render frame)
    void SwapBuffers();

    void DisplayTransfer(const GPU::Regs::DisplayTransferConfig*);

    void MemoryFill(const GPU::Regs::MemoryFillConfig*, bool is_second_filler);

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch
    /// memory
    void FlushRegion(VAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be invalidated
    void InvalidateRegion(VAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be flushed and
    /// invalidated
    void FlushAndInvalidateRegion(VAddr addr, u64 size);

private:
    /// Pushes a command to be executed by the GPU thread
    void PushCommand(CommandData&& command_data, std::future<void>* wait_for_idle = nullptr);

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
