// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_clnt.h"

namespace Service {
namespace DLP {

enum class SessionType : u8 {
    Invalid = 0x00,
    Title = 0x01,
    System = 0x02,
};

enum class ClientState : u8 {
    Invalid = 0x00,
    DisconnectedNetwork = 0x01,
    Scanning = 0x02,
    WaitingConnect = 0x03,
    WaitingInvite = 0x04,
    JoinedSession = 0x05,
    Downloading = 0x06,
    DownloadComplete = 0x7,
    Rebooting = 0x9,
    Error = 0x80,
    WaitingAccept = 0x40
};

struct ClientStatus {
    u16 nodeId = 1;
    SessionType   sessionType = SessionType::Title;
    ClientState state = ClientState::Invalid;
    u32 totalNum = 0;
    u32 downloadedNum = 0;
};

static ClientStatus client_status;

static void Initialize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    client_status.state = ClientState::WaitingConnect;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

static void GetMyStatus(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    memcpy(&cmd_buff[2], &client_status, sizeof(ClientStatus));
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

static void StopSession(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    client_status.state = ClientState::Invalid;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C3, Initialize, "Initialize"},
    {0x00020000, nullptr, "Finalize"},
    {0x00030000, nullptr, "GetEventDesc"},
    {0x00040000, nullptr, "GetChannels"},
    {0x00050180, nullptr, "StartScan"},
    {0x00060000, nullptr, "StopScan"},
    {0x00070080, nullptr, "GetServerInfo"},
    {0x00080100, nullptr, "GetTitleInfo"},
    {0x00090040, nullptr, "GetTitleInfoInOrder"},
    {0x000A0080, nullptr, "DeleteScanInfo"},
    {0x000B0100, nullptr, "PrepareForSystemDownload"},
    {0x000C0000, nullptr, "StartSystemDownload"},
    {0x000D0100, nullptr, "StartTitleDownload"},
    {0x000E0000, GetMyStatus, "GetMyStatus"},
    {0x000F0040, nullptr, "GetConnectingNodes"},
    {0x00100040, nullptr, "GetNodeInfo"},
    {0x00110000, nullptr, "GetPassphrase"},
    {0x00120000, StopSession, "StopSession"},
    {0x00130100, nullptr, "GetCupVersion"},
    {0x00140100, nullptr, "GetDupAvailability"},
};

DLP_CLNT_Interface::DLP_CLNT_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
