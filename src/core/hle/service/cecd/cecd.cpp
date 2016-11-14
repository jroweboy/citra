// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

enum class SaveDataType {
    MBoxList = 1,
    MBoxInfo = 2,
    InBoxInfo = 3,
    OutBoxInfo = 4,
    OutBoxIndex = 5,
    InBoxMessage = 6,
    OutBoxMessage = 7,
    RootDir = 10,
    MBoxDir = 11,
    InBoxDir = 12,
    OutBoxDir = 13,
    MBoxDataStart = 100,
    MBoxDataEnd = 199
};

static Kernel::SharedPtr<Kernel::Event> cecinfo_event;
static Kernel::SharedPtr<Kernel::Event> change_state_event;

static Service::FS::ArchiveHandle cec_system_save_data_archive;
static const std::vector<u8> cec_system_savedata_id = {0x00, 0x00, 0x00, 0x00,
                                                       0x26, 0x00, 0x01, 0x00};

static std::string EncodeBase64(const std::vector<u8>& in, const std::string& dictionary) {
    std::string out;
    out.reserve((in.size() * 4) / 3);
    int b;
    for (int i = 0; i < in.size(); i += 3) {
        b = (in[i] & 0xFC) >> 2;
        out += dictionary[b];
        b = (in[i] & 0x03) << 4;
        if (i + 1 < in.size()) {
            b |= (in[i + 1] & 0xF0) >> 4;
            out += dictionary[b];
            b = (in[i + 1] & 0x0F) << 2;
            if (i + 2 < in.size()) {
                b |= (in[i + 2] & 0xC0) >> 6;
                out += dictionary[b];
                b = in[i + 2] & 0x3F;
                out += dictionary[b];
            } else {
                out += dictionary[b];
            }
        } else {
            out += dictionary[b];
        }
    }
    return out;
}

static std::string EncodeMessageId(const std::vector<u8>& in) {
    return EncodeBase64(in, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-");
}

static std::string GetSaveDataPath(SaveDataType type, u32 title_id,
                                   const std::vector<u8>& message_id = std::vector<u8>()) {
    switch (type) {
    case SaveDataType::MBoxList:
        return "CEC/MBoxList____";
    case SaveDataType::MBoxInfo:
        return Common::StringFromFormat("CEC/%08x/MBoxInfo____", title_id);
    case SaveDataType::InBoxInfo:
        return Common::StringFromFormat("CEC/%08x/InBox___/BoxInfo_____", title_id);
    case SaveDataType::OutBoxInfo:
        return Common::StringFromFormat("CEC/%08x/OutBox__/BoxInfo_____", title_id);
    case SaveDataType::OutBoxIndex:
        return Common::StringFromFormat("CEC/%08x/OutBox__/OBIndex_____", title_id);
    case SaveDataType::InBoxMessage:
        return Common::StringFromFormat("CEC/%08x/InBox___/_%s", title_id,
                                        EncodeMessageId(message_id).data());
    case SaveDataType::OutBoxMessage:
        return Common::StringFromFormat("CEC/%08x/OutBox__/_%s", title_id,
                                        EncodeMessageId(message_id).data());
    case SaveDataType::RootDir:
        return "CEC";
    case SaveDataType::MBoxDir:
        return Common::StringFromFormat("CEC/%08x", title_id);
    case SaveDataType::InBoxDir:
        return Common::StringFromFormat("CEC/%08x/InBox___", title_id);
    case SaveDataType::OutBoxDir:
        return Common::StringFromFormat("CEC/%08x/OutBox__", title_id);
    }

    int index = static_cast<int>(type) - 100;
    if (index > 0 && index < 100) {
        return Common::StringFromFormat("CEC/%08x/MBoxData.%03d", title_id, index);
    }

    UNREACHABLE();
}

static bool IsSaveDataDir(SaveDataType type) {
    switch (type) {
    case SaveDataType::RootDir:
    case SaveDataType::MBoxDir:
    case SaveDataType::InBoxDir:
    case SaveDataType::OutBoxDir:
        return true;
    default:
        return false;
    }
}

void Open(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 title_id = cmd_buff[1];
    SaveDataType save_data_type = static_cast<SaveDataType>(cmd_buff[2]);
    u32 option = cmd_buff[3];

    LOG_CRITICAL(Service_CECD,
                 "(STUBBED) called. title_id = 0x%08X, save_data_type = %d, option = 0x%08X",
                 title_id, save_data_type, option);

    FileSys::Path path(GetSaveDataPath(save_data_type, title_id).data());

    if (!IsSaveDataDir(save_data_type)) {
        FileSys::Mode mode = {};
        if ((option & 7) == 2) {
            mode.read_flag.Assign(1);
        } else if ((option & 7) == 4) {
            mode.write_flag.Assign(1);
            mode.create_flag.Assign(1);
        } else if ((option & 7) == 6) {
            mode.read_flag.Assign(1);
            mode.write_flag.Assign(1);
        } else {
            UNREACHABLE();
        }

        auto open_result =
            Service::FS::OpenFileFromArchive(cec_system_save_data_archive, path, mode);
        if (!open_result.Succeeded()) {
            LOG_CRITICAL(Service_CECD, "failed");
            cmd_buff[1] = open_result.Code().raw;
            cmd_buff[2] = 0;
            return;
        }

        auto file = open_result.MoveFrom();
        cmd_buff[2] = file->backend->GetSize();
    } else {
        /*!*/ cmd_buff[1] =
            Service::FS::CreateDirectoryFromArchive(cec_system_save_data_archive, path).raw;
        return;
        // ASSERT_MSG(false, "folder");
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void Read(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    ASSERT(IPC::MappedBufferDesc(size, IPC::W) == cmd_buff[2]);
    VAddr buffer_address = cmd_buff[3];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called. buffer_address = 0x%08X, size = 0x%X",
                 buffer_address, size);
}

void ReadMessage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void ReadMessageAlt(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void Write(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void WriteMessage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void WriteMessageAlt(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 title_id = cmd_buff[1];
    u8 box_type = cmd_buff[2] & 0xFF;
    u32 message_id_size = cmd_buff[3];
    u32 buffer_size = cmd_buff[4];
    ASSERT(IPC::MappedBufferDesc(buffer_size, IPC::R) == cmd_buff[5]);
    VAddr buffer_addr = cmd_buff[6];
    ASSERT(IPC::MappedBufferDesc(32, IPC::R) == cmd_buff[7]);
    VAddr key_addr = cmd_buff[8];
    ASSERT(IPC::MappedBufferDesc(message_id_size, IPC::RW) == cmd_buff[9]);
    VAddr message_id_addr = cmd_buff[10];

    LOG_CRITICAL(Service_CECD, "(STUBBED) called. title_id = 0x%08X, box_type = %d, "
                               "message_id_addr = 0x%08X, message_id_size = 0x%X, buffer_addr = "
                               "0x%08X, buffer_size = 0x%X, "
                               "key_addr = 0x%08X",
                 title_id, box_type, message_id_addr, message_id_size, buffer_addr, buffer_size,
                 key_addr);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void Delete(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 title_id = cmd_buff[1];
    SaveDataType save_data_type = static_cast<SaveDataType>(cmd_buff[2]);
    u8 box_type = cmd_buff[2] & 0xFF;
    u32 message_id_size = cmd_buff[4];
    ASSERT(IPC::MappedBufferDesc(message_id_size, IPC::R) == cmd_buff[5]);
    VAddr message_id_addr = cmd_buff[6];

    LOG_CRITICAL(Service_CECD, "(STUBBED) called. title_id = 0x%08X, save_data_type = %d, box_type "
                               "= %d, message_id_size = 0x%X, message_id_addr = 0x%08X",
                 title_id, save_data_type, box_type, message_id_size, message_id_addr);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void cecd9(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 title_id = cmd_buff[1];
    u32 size = cmd_buff[2];
    u32 option = cmd_buff[3];
    ASSERT(IPC::MappedBufferDesc(size, IPC::R) == cmd_buff[4]);
    VAddr buffer_address = cmd_buff[5];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called, title_id = 0x%08X, option = 0x%08X, "
                               "buffer_address = 0x%08X, size = 0x%X",
                 title_id, option, buffer_address, size);
}

enum class SystemInfoType { EulaVersion = 1, Eula = 2, ParentControl = 3 };

void GetSystemInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 info_size = cmd_buff[1];
    SystemInfoType type = static_cast<SystemInfoType>(cmd_buff[2]);
    u32 param_size = cmd_buff[3];
    ASSERT(IPC::MappedBufferDesc(param_size, IPC::R) == cmd_buff[4]);
    VAddr param_addr = cmd_buff[5];
    ASSERT(IPC::MappedBufferDesc(info_size, IPC::W) == cmd_buff[6]);
    VAddr info_addr = cmd_buff[7];

    LOG_CRITICAL(Service_CECD, "(STUBBED) called, info_addr = 0x%08X, info_size = 0x%X, type = %d, "
                               "param_addr = 0x%08X, param_size = 0x%X",
                 info_addr, info_size, type, param_addr, param_size);

    switch (type) {
    case SystemInfoType::EulaVersion:
        if (info_size != 2) {
            cmd_buff[1] = 0xC8810BEF;
        } else {
            // TODO read from cfg
            Memory::Write16(info_addr, 1);
            cmd_buff[1] = RESULT_SUCCESS.raw;
        }
        break;
    case SystemInfoType::Eula:
        if (info_size != 1) {
            cmd_buff[1] = 0xC8810BEF;
        } else {
            // TODO read from cfg
            Memory::Write8(info_addr, 1);
            cmd_buff[1] = RESULT_SUCCESS.raw;
        }
        break;
    case SystemInfoType::ParentControl:
        if (info_size != 1) {
            cmd_buff[1] = 0xC8810BEF;
        } else {
            // TODO read from cfg
            Memory::Write8(info_addr, 0);
            cmd_buff[1] = RESULT_SUCCESS.raw;
        }
        break;
    default:
        LOG_ERROR(Service_CECD, "Unknown system info type %d", type);
        cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    }
}

void cecdB(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void cecdC(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void GetCecStateAbbreviated(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(CecStateAbbreviated::CEC_STATE_ABBREV_IDLE);

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void GetCecInfoEventHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;                                      // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(cecinfo_event).MoveFrom(); // Event handle

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void GetChangeStateEventHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;                                           // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(change_state_event).MoveFrom(); // Event handle

    LOG_CRITICAL(Service_CECD, "(STUBBED) called");
}

void OpenAndWrite(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 title_id = cmd_buff[2];
    SaveDataType save_data_type = static_cast<SaveDataType>(cmd_buff[3]);
    u32 option = cmd_buff[4];
    ASSERT(IPC::MappedBufferDesc(size, IPC::R) == cmd_buff[7]);
    VAddr buffer_address = cmd_buff[8];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_CRITICAL(Service_CECD, "(STUBBED) called. title_id = 0x%08X, save_data_type = %d, option = "
                               "0x%08X, buffer_address = 0x%08X, size = 0x%X",
                 title_id, save_data_type, option, buffer_address, size);

    FileSys::Mode mode = {};
    if ((option & 7) == 2) {
        mode.read_flag.Assign(1);
    } else if ((option & 7) == 4) {
        mode.write_flag.Assign(1);
        mode.create_flag.Assign(1);
    } else if ((option & 7) == 6) {
        mode.read_flag.Assign(1);
        mode.write_flag.Assign(1);
    } else {
        UNREACHABLE();
    }

    FileSys::Path path(GetSaveDataPath(save_data_type, title_id).data());
    auto open_result = Service::FS::OpenFileFromArchive(cec_system_save_data_archive, path, mode);
    if (!open_result.Succeeded()) {
        LOG_CRITICAL(Service_CECD, "failed");
        cmd_buff[1] = open_result.Code().raw;
        cmd_buff[2] = 0;
        return;
    }

    auto file = open_result.MoveFrom();
    std::vector<u8> buffer(size);
    Memory::ReadBlock(buffer_address, buffer.data(), size);
    size_t written_size = *(file->backend->Write(0, size, true, buffer.data()));

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = written_size;

    LOG_CRITICAL(Service_CECD, "written %X", written_size);
}

void OpenAndRead(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 title_id = cmd_buff[2];
    SaveDataType save_data_type = static_cast<SaveDataType>(cmd_buff[3]);
    u32 option = cmd_buff[4];
    ASSERT(IPC::MappedBufferDesc(size, IPC::W) == cmd_buff[7]);
    VAddr buffer_address = cmd_buff[8];

    LOG_CRITICAL(Service_CECD, "(STUBBED) called. title_id = 0x%08X, save_data_type = %d, option = "
                               "0x%08X, buffer_address = 0x%08X, size = 0x%X",
                 title_id, save_data_type, option, buffer_address, size);

    FileSys::Mode mode = {};
    if ((option & 7) == 2) {
        mode.read_flag.Assign(1);
    } else if ((option & 7) == 4) {
        mode.write_flag.Assign(1);
        mode.create_flag.Assign(1);
    } else if ((option & 7) == 6) {
        mode.read_flag.Assign(1);
        mode.write_flag.Assign(1);
    } else {
        UNREACHABLE();
    }

    FileSys::Path path(GetSaveDataPath(save_data_type, title_id).data());
    auto open_result = Service::FS::OpenFileFromArchive(cec_system_save_data_archive, path, mode);
    if (!open_result.Succeeded()) {
        LOG_CRITICAL(Service_CECD, "failed");
        cmd_buff[1] = open_result.Code().raw;
        cmd_buff[2] = 0;
        return;
    }

    auto file = open_result.MoveFrom();
    std::vector<u8> buffer(size);
    size_t read_size = *(file->backend->Read(0, size, buffer.data()));
    Memory::WriteBlock(buffer_address, buffer.data(), read_size);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = read_size;

    LOG_CRITICAL(Service_CECD, "read %X", read_size);
}

void Init() {
    AddService(new CECD_S_Interface);
    AddService(new CECD_U_Interface);

    cecinfo_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "CECD_U::cecinfo_event");
    change_state_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "CECD_U::change_state_event");

    // Open the SystemSaveData archive 0x00010026
    FileSys::Path archive_path(cec_system_savedata_id);
    auto archive_result =
        Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code().description == ErrorDescription::FS_NotFormatted) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SystemSaveData,
                                   FileSys::ArchiveFormatInfo(), archive_path);

        // Open it again to get a valid archive now that the folder exists
        archive_result =
            Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);
    }

    ASSERT_MSG(archive_result.Succeeded(), "Could not open the CEC SystemSaveData archive!");

    cec_system_save_data_archive = *archive_result;
}

void Shutdown() {
    cecinfo_event = nullptr;
    change_state_event = nullptr;
}

} // namespace CECD

} // namespace Service
