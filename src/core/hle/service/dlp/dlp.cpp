// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_clnt.h"
#include "core/hle/service/dlp/dlp_fkcl.h"
#include "core/hle/service/dlp/dlp_srvr.h"
#include "core/hle/service/service.h"

namespace Service {
namespace DLP {

void IsChild(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

void Init() {
    AddService(new DLP_CLNT_Interface);
    AddService(new DLP_FKCL_Interface);
    AddService(new DLP_SRVR_Interface);
}

void Shutdown() {}

} // namespace DLP
} // namespace Service
