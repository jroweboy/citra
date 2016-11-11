// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/mic_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace MIC_U

namespace MIC_U {

enum SamplingType {
    SAMPLING_TYPE_8BIT = 0, //!< Enable 8-bit (unsigned) sampling. Output data range is 0 to 255.
    SAMPLING_TYPE_16BIT =
        1, //!< Enable 16-bit (unsigned) sampling. Output data range is 0 to 65535.
    SAMPLING_TYPE_SIGNED_8BIT =
        2, //!< Enable 8-bit (signed) sampling. Output data range is -128 to 127.
    SAMPLING_TYPE_SIGNED_16BIT =
        3 //!< Enable 16-bit (signed) sampling. Output data range is -32768 to 32767.
};

enum class SamplingRate: u8 {
    SAMPLING_RATE_32730 = 0, //!< 32.73kHz (actually 32728.498046875 Hz)
    SAMPLING_RATE_16360 = 1, //!< 16.36kHz (actually  16364.2490234375 Hz)
    SAMPLING_RATE_10910 = 2, //!< 10.91kHz (actually  10909.4993489583 Hz)
    SAMPLING_RATE_8180 = 3   //!< 8.18kHz  (actually  8182.12451171875 Hz)
};

static Kernel::SharedPtr<Kernel::Event> buffer_full_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static u8 pgab = 0x28;  // TODO -> move to constructor  Sets the microphone amp's gain. Specify a value in the range 0-119 (10.5-70.0 dB).
static bool mic_bias;
static bool is_sampling;
static bool except_shell_close;
static bool clamp;
static SamplingType sampling_type;
static SamplingRate sampling_rate;
static s32 offset;
static u32 size;
static bool loop;

static void AllocateBuffer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 size = cmd_buff[1];
    // cmd_buff[2] -> CopyHandleDesc
    Handle mem_handle = cmd_buff[3];
    shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(mem_handle);
    if (shared_memory) {
        shared_memory->name = "MIC_U:shared_memory"; // TODO: how to allocate memory
        memset(shared_memory->GetPointer(), 0, size); // TODO: do we really need this?
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, size=%d", size);
}

static void FreeBuffer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO free allocated buffer

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void GetPGAB(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = pgab;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void StartSampling(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    sampling_type = static_cast<SamplingType>(cmd_buff[1] & 0xFF);
    sampling_rate = static_cast<SamplingRate>(cmd_buff[2] & 0xFF);
    offset = cmd_buff[3];
    size = cmd_buff[4];
    loop = cmd_buff[5] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    is_sampling = true;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void StopSampling(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    is_sampling = false;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void SetPGAB(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    pgab = cmd_buff[1] & 0xFF;
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, pgab = %d", pgab);
}

static void GetMicBias(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = mic_bias;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void SetMicBias(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // SetMicBias(bool)
    mic_bias = cmd_buff[1] & 0xFF;
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, power = %u", mic_bias);
}

static void GetBufferFullEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(buffer_full_event).MoveFrom();
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void IsSampling(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = is_sampling;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void AdjustSampling(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    sampling_rate = static_cast<SamplingRate>(cmd_buff[1] & 0xFF);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void ExceptShellClose(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    except_shell_close = cmd_buff[1] & 0xFF;
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void SetClamp(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    clamp = cmd_buff[1] & 0xFF;
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void GetClamp(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = clamp;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

static void SetIirFilterMic(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    //cmd_buff[1] -> size_t size
    //cmd_buff[2] -> shifted size
    //cmd_buff[3] -> buffer


    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, AllocateBuffer, "AllocateBuffer"},
    {0x00020000, FreeBuffer, "FreeBuffer"},
    {0x00030140, StartSampling, "StartSampling"},
    {0x00040040, AdjustSampling, "AdjustSampling"},
    {0x00050000, StopSampling, "StopSampling"},
    {0x00060000, IsSampling, "IsSampling"},
    {0x00070000, GetBufferFullEvent, "GetBufferFullEvent"},
    {0x00080040, SetPGAB, "SetGain"},
    {0x00090000, GetPGAB, "GetGain"},
    {0x000A0040, SetMicBias, "SetPower"},
    {0x000B0000, GetMicBias, "GetPower"},
    {0x000C0042, SetIirFilterMic, "SetIirFilterMic"},
    {0x000D0040, SetClamp, "SetClamp"},
    {0x000E0000, GetClamp, "GetClamp"},
    {0x000F0040, ExceptShellClose, "ExceptShellClose"},
    {0x00100040, nullptr, "SetClientSDKVersion"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
    shared_memory = nullptr;
    buffer_full_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "MIC_U::buffer_full_event");
    is_sampling = false;
}

Interface::~Interface() {
    shared_memory = nullptr;
    buffer_full_event = nullptr;
}

} // namespace
