// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>
#include "common/common_types.h"

namespace Log {

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
    Trace,    ///< Extremely detailed and repetitive debugging information that is likely to
              ///  pollute logs.
    Debug,    ///< Less detailed debugging information.
    Info,     ///< Status information from important points during execution.
    Warning,  ///< Minor or potential problems found during execution of a task.
    Error,    ///< Major problems found during execution of a task that prevent it from being
              ///  completed.
    Critical, ///< Major problems during execution that threathen the stability of the entire
              ///  application.

    Count ///< Total number of logging levels
};

typedef u8 ClassType;

/**
 * Specifies the sub-system that generated the log message.
 *
 * @note If you add a new entry here, also add a corresponding one to `ALL_LOG_CLASSES` in
 * backend.cpp.
 */
enum class Class : ClassType {
    Log,               ///< Messages about the log system itself
    Common,            ///< Library routines
    Common_Filesystem, ///< Filesystem interface library
    Common_Memory,     ///< Memory mapping and management functions
    Core,              ///< LLE emulation core
    Core_ARM11,        ///< ARM11 CPU core
    Core_Timing,       ///< CoreTiming functions
    Config,            ///< Emulator configuration (including commandline)
    Debug,             ///< Debugging tools
    Debug_Emulated,    ///< Debug messages from the emulated programs
    Debug_GPU,         ///< GPU debugging tools
    Debug_Breakpoint,  ///< Logging breakpoints and watchpoints
    Debug_GDBStub,     ///< GDB Stub
    Kernel,            ///< The HLE implementation of the CTR kernel
    Kernel_SVC,        ///< Kernel system calls
    Service,           ///< HLE implementation of system services. Each major service
                       ///  should have its own subclass.
    Service_SRV,       ///< The SRV (Service Directory) implementation
    Service_FRD,       ///< The FRD (Friends) service
    Service_FS,        ///< The FS (Filesystem) service implementation
    Service_ERR,       ///< The ERR (Error) port implementation
    Service_APT,       ///< The APT (Applets) service
    Service_BOSS,      ///< The BOSS (SpotPass) service
    Service_GSP,       ///< The GSP (GPU control) service
    Service_AC,        ///< The AC (WiFi status) service
    Service_AM,        ///< The AM (Application manager) service
    Service_PTM,       ///< The PTM (Power status & misc.) service
    Service_LDR,       ///< The LDR (3ds dll loader) service
    Service_MIC,       ///< The MIC (Microphone) service
    Service_NDM,       ///< The NDM (Network daemon manager) service
    Service_NFC,       ///< The NFC service
    Service_NIM,       ///< The NIM (Network interface manager) service
    Service_NWM,       ///< The NWM (Network wlan manager) service
    Service_CAM,       ///< The CAM (Camera) service
    Service_CECD,      ///< The CECD (StreetPass) service
    Service_CFG,       ///< The CFG (Configuration) service
    Service_CSND,      ///< The CSND (CWAV format process) service
    Service_DSP,       ///< The DSP (DSP control) service
    Service_DLP,       ///< The DLP (Download Play) service
    Service_HID,       ///< The HID (Human interface device) service
    Service_HTTP,      ///< The HTTP service
    Service_SOC,       ///< The SOC (Socket) service
    Service_IR,        ///< The IR service
    Service_Y2R,       ///< The Y2R (YUV to RGB conversion) service
    HW,                ///< Low-level hardware emulation
    HW_Memory,         ///< Memory-map and address translation
    HW_LCD,            ///< LCD register emulation
    HW_GPU,            ///< GPU control emulation
    HW_AES,            ///< AES engine emulation
    Frontend,          ///< Emulator UI
    Render,            ///< Emulator video output and hardware acceleration
    Render_Software,   ///< Software renderer backend
    Render_OpenGL,     ///< OpenGL backend
    Audio,             ///< Audio emulation
    Audio_DSP,         ///< The HLE implementation of the DSP
    Audio_Sink,        ///< Emulator audio output backend
    Loader,            ///< ROM loader
    Input,             ///< Input emulation
    Count              ///< Total number of logging classes
};

/// Logs a message to the global logger.
void LogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_nr,
                const char* function,
#ifdef _MSC_VER
                _Printf_format_string_
#endif
                const char* format,
                ...)
#ifdef __GNUC__
    __attribute__((format(printf, 6, 7)))
#endif
    ;

void SpdLogImpl(u32 logger, Level log_level, const char* format, fmt::ArgList& args);

template <typename Arg1, typename... Args>
void SpdLogMessage(u32 logger, Level log_level, const char* filename, unsigned int line_nr,
                   const char* function, const Arg1& format, const Args&... args) {
    typedef fmt::internal::ArgArray<sizeof...(Args)> ArgArray;
    typename ArgArray::Type array{ArgArray::template make<fmt::BasicFormatter<char>>(args)...};
    fmt::MemoryWriter formatting_buffer;
    formatting_buffer << Common::TrimSourcePath(filename) << ':' << function << ':' << line_nr
                      << ": " << format;
    SpdLogImpl(logger, log_level, formatting_buffer.c_str(),
               fmt::ArgList(fmt::internal::make_type(args...), array));
}

u32 RegisterLogger(const char* class_name);

} // namespace Log

#define LOG_GENERIC(log_class, log_level, ...)                                                     \
    ::Log::LogMessage(log_class, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...)                                                                  \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Trace, __VA_ARGS__)
#else
#define LOG_TRACE(log_class, ...) (void(0))
#endif

#define LOG_DEBUG(log_class, ...)                                                                  \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Debug, __VA_ARGS__)
#define LOG_INFO(log_class, ...)                                                                   \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Info, __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                                \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Warning, __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                                  \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Error, __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    LOG_GENERIC(::Log::Class::log_class, ::Log::Level::Critical, __VA_ARGS__)

// Define the spdlog level macros

#define REGISTER_LOGGER(class_name) static u32 _logger = ::Log::RegisterLogger(class_name)

#ifdef _DEBUG
#define SPDLOG_TRACE(fmt, ...)                                                                     \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Trace, __FILE__, __LINE__, __func__, fmt,          \
                         ##__VA_ARGS__)
#else
#define SPDLOG_TRACE(fmt, ...) (void(0))
#endif

#define SPDLOG_DEBUG(fmt, ...)                                                                     \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Debug, __FILE__, __LINE__, __func__, fmt,          \
                         ##__VA_ARGS__)
#define SPDLOG_INFO(fmt, ...)                                                                      \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Info, __FILE__, __LINE__, __func__, fmt,           \
                         ##__VA_ARGS__)
#define SPDLOG_WARNING(fmt, ...)                                                                   \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Warning, __FILE__, __LINE__, __func__, fmt,        \
                         ##__VA_ARGS__)
#define SPDLOG_ERROR(fmt, ...)                                                                     \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Error, __FILE__, __LINE__, __func__, fmt,          \
                         ##__VA_ARGS__)
#define SPDLOG_CRITICAL(fmt, ...)                                                                  \
    ::Log::SpdLogMessage(_logger, ::Log::Level::Critical, __FILE__, __LINE__, __func__, fmt,       \
                         ##__VA_ARGS__)
