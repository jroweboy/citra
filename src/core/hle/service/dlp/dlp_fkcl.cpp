// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_fkcl.h"

namespace Service {
namespace DLP {

struct UserName { // TODO(mailwl): use cfg struct
    u16 user_name[11];
    bool is_ng_user_name;
};

struct TitleInfo {
    u32 unique_id;
    u8 child_index;
    INSERT_PADDING_BYTES(3);
    u8 mac[6];
    u16 program_version;
    u8 rating_info[16];
    u16 short_title_name[64];
    u16 long_title_name[128];
    u16 icon[48 * 48];
    u32 import_size;
    u8 region_code; // TODO CfgRegionCode
    bool ulcd;
    INSERT_PADDING_BYTES(2);
};

static UserName user_name;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static Kernel::SharedPtr<Kernel::Event> event;
static u8 scan_num; // TODO u32
static u32 buf_size;

void InitializeWithName(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    buf_size = cmd_buff[1];
    scan_num = cmd_buff[2] & 0xFF;
    memcpy(&user_name, &cmd_buff[3], sizeof(UserName));

    // u32 copy_handle = cmd_buff[9];

    shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(cmd_buff[10]);
    if (shared_memory) {
        shared_memory->name = "DLP_FKCL:SharedMemory";
    }
    event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[11]);
    if (event) {
        event->name = "DLP_FKCL:Event";
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called, buf_size=%u, scan_num=%u", buf_size, scan_num);
}

void GetMyStatus(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // u16 nodeId, u8 sessionType, u8 ClientState
    cmd_buff[3] = 0; // totalNum
    cmd_buff[4] = 0; // downloadedNum
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void GetChannels(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // u16 channels
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void StartScan(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u16 s = cmd_buff[1];
    u64 id = (u64)cmd_buff[3] << 32 | cmd_buff[2];
    char d[6];
    memcpy(d, &cmd_buff[4], 6);

    bool b = cmd_buff[6] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void GetTitleInfoInOrder(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[64] >> 14; // 0x13a8
    u32 title_info = cmd_buff[65];
    TitleInfo* ti = reinterpret_cast<TitleInfo*>(Memory::GetPointer(title_info));

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

static void Finalize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010083, nullptr, "Initialize"},
    {0x00020000, Finalize, "Finalize"},
    {0x00030000, nullptr, "GetEventDesc"},
    {0x00040000, GetChannels, "GetChannels"},
    {0x00050180, StartScan, "StartScan"},
    {0x00060000, nullptr, "StopScan"},
    {0x00070080, nullptr, "GetServerInfo"},
    {0x00080100, nullptr, "GetTitleInfo"},
    {0x00090040, GetTitleInfoInOrder, "GetTitleInfoInOrder"},
    {0x000A0080, nullptr, "DeleteScanInfo"},
    {0x000B0100, nullptr, "StartFakeSession"},
    {0x000C0000, GetMyStatus, "GetMyStatus"},
    {0x000D0040, nullptr, "GetConnectingNodes"},
    {0x000E0040, nullptr, "GetNodeInfo"},
    {0x000F0000, nullptr, "GetPassphrase"},
    {0x00100000, nullptr, "StopSession"},
    {0x00110203, InitializeWithName, "InitializeWithName"},
};

DLP_FKCL_Interface::DLP_FKCL_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
