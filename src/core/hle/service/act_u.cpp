// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/act_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace ACT_U

namespace ACT_U {

// 3 params a1, a2, a3
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 version = cmd_buff[1];
    u32 a3 = cmd_buff[2];
    u32 _0x20 = cmd_buff[3];
    u32 zero = cmd_buff[5];
    u32 a2 = cmd_buff[6];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_ACT, "(STUBBED) called, version=0x%X, a2=0x%X, a3=0x%X", version, a2, a3);
}
const Interface::FunctionInfo FunctionTable[] = {
    {0x00010084, Initialize, "Initialize"},
    {0x00020040, nullptr, "GetErrorCode"},
    {0x000600C2, nullptr, "GetAccountDataBlock"},
    {0x000D0040, nullptr, "GenerateUuid"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
