// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service {
namespace DLP {

enum class ServerState : u32 {
    Invalid = 0,
    Initialized = 1,
    OpenedSessions = 2,
    /*
        PrepareForSystemDistribution = 3,
        DistributingSystem = 4,
        WaitingReconnect = 5,
        PrepareForTitleDistribution = 6,
        DistributingTitle = 7,
    */
    CompleteDistribution = 8,
    RebootingClients = 9,
    Error = 10,
    Distributing = 11

};

static ServerState server_state = ServerState::Invalid;

static void Initialize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    server_state = ServerState::Initialized;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

static void Finalize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    server_state = ServerState::Invalid;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void GetState(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(server_state);
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void OpenSessions(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    server_state = ServerState::OpenedSessions;

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void GetConnectingClients(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1] & 0xFFFF; // u16
    // cmd_buff[3] -> size * 32 | 0xC
    u32 addr = cmd_buff[3]; // u16*

    Memory::Write16(addr, 0x1234);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1; // connecting clients

    LOG_WARNING(Service_DLP, "(STUBBED) called, size=0x%X, addr=0x%08X", size, addr);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010183, Initialize, "Initialize"},
    {0x00020000, Finalize, "Finalize"},
    {0x00030000, GetState, "GetState"},
    {0x00040000, nullptr, "GetEventDesc"},
    {0x00050080, OpenSessions, "OpenSessions"},
    {0x00060000, nullptr, "CloseSessions"},
    {0x00070000, nullptr, "StartDistribute"},
    {0x000800C0, nullptr, "RebootAllClients"},
    {0x00090040, nullptr, "AcceptClient"},
    {0x000A0040, nullptr, "DisconnectClient"},
    {0x000C0040, nullptr, "GetClientInfo"},
    {0x000B0042, GetConnectingClients, "GetConnectingClients"},
    {0x000D0040, nullptr, "GetClientStatus"},
    {0x000E0040, IsChild, "IsChild"},
    {0x000F0303, nullptr, "InitializeWithName"},
    {0x00100000, nullptr, "GetDupNoticeNeed"},
};

DLP_SRVR_Interface::DLP_SRVR_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
