// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <glad/glad.h>
#include <queue>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/dumping/backend.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "core/settings.h"
#include "core/tracer/recorder.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

namespace OpenGL {

constexpr std::size_t SWAP_CHAIN_SIZE = 4;

class OGLTextureMailbox : public Frontend::TextureMailbox {
public:
    std::mutex swap_chain_lock;
    std::condition_variable free_cv;
    std::condition_variable present_cv;
    std::array<Frontend::Frame, SWAP_CHAIN_SIZE> swap_chain{};
    std::queue<Frontend::Frame*> free_queue{};
    std::deque<Frontend::Frame*> present_queue{};
    Frontend::Frame* previous_frame = nullptr;

    OGLTextureMailbox() {
        for (auto& frame : swap_chain) {
            free_queue.push(&frame);
        }
    }

    ~OGLTextureMailbox() override {
        // lock the mutex and clear out the present and free_queues and notify any people who are
        // blocked to prevent deadlock on shutdown
        std::scoped_lock lock(swap_chain_lock);
        std::queue<Frontend::Frame*>().swap(free_queue);
        present_queue.clear();
        present_cv.notify_all();
    }

    void ReloadPresentFrame(Frontend::Frame* frame) override {
        GLint previous_draw_fbo{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
        for (auto i : {0, 1, 2}) {
            auto& screen = frame->screens[i];
            screen.present.Release();
            screen.present.Create();
            glBindFramebuffer(GL_FRAMEBUFFER, screen.present.handle);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
                                 screen.texture.handle);
        }
        if (!glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            LOG_CRITICAL(Render_OpenGL, "Failed to recreate present FBO!");
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previous_draw_fbo);
        frame->texture_reloaded = false;
    }

    void ReloadRenderFrame(Frontend::Frame* frame, u16 res_scale) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        OpenGLState state = OpenGLState::GetCurState();

        // Recreate the screen texture attachment
        for (auto i : {0, 1, 2}) {
            auto& screen = frame->screens[i];
            screen.texture.Release();
            screen.texture.Create();
            screen.scaled_width =
                (i == 2 ? Core::kScreenBottomWidth : Core::kScreenTopWidth) * res_scale;
            screen.scaled_height =
                (i == 2 ? Core::kScreenBottomHeight : Core::kScreenTopHeight) * res_scale;
            state.texture_units[i].texture_2d = screen.texture.handle;
        }
        state.Apply();

        // Mark the read and draw framebuffer as dirty
        state.draw.read_framebuffer = 0;
        state.draw.draw_framebuffer = 0;
        state.Apply();

        // TODO: need a better way to consolidate texture allocation
        for (auto i : {0, 1, 2}) {
            auto& screen = frame->screens[i];

            // Recreate the FBO for the render target
            screen.render.Release();
            screen.render.Create();

            // Configure the framebuffer for this texture
            glBindFramebuffer(GL_FRAMEBUFFER, screen.render.handle);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glBindTexture(GL_TEXTURE_2D, screen.texture.handle);

            // Allocate textures for the screens
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, screen.scaled_width, screen.scaled_height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, screen.texture.handle, 0);
            if (!glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                LOG_CRITICAL(Render_OpenGL, "Failed to recreate render FBO!");
            }
        }

        prev_state.Apply();
        frame->res_scale = res_scale;
        frame->texture_reloaded = true;
    }

    Frontend::Frame* GetRenderFrame() override {
        std::unique_lock<std::mutex> lock(swap_chain_lock);

        // If theres no free frames, we will reuse the oldest render frame
        if (free_queue.empty()) {
            auto frame = present_queue.back();
            present_queue.pop_back();
            return frame;
        }

        Frontend::Frame* frame = free_queue.front();
        free_queue.pop();
        return frame;
    }

    void ReleaseRenderFrame(Frontend::Frame* frame) override {
        std::unique_lock<std::mutex> lock(swap_chain_lock);
        present_queue.push_front(frame);
        present_cv.notify_one();
    }

    Frontend::Frame* TryGetPresentFrame(std::chrono::milliseconds timeout) override {
        std::unique_lock<std::mutex> lock(swap_chain_lock);
        // wait for new entries in the present_queue
        present_cv.wait_for(lock, timeout, [&] { return !present_queue.empty(); });
        if (present_queue.empty()) {
            // timed out waiting for a frame to draw so return the previous frame
            return previous_frame;
        }

        // free the previous frame and add it back to the free queue
        if (previous_frame) {
            free_queue.push(previous_frame);
        }

        // the newest entries are pushed to the front of the queue
        Frontend::Frame* frame = present_queue.front();
        present_queue.pop_front();
        // remove all old entries from the present queue and move them back to the free_queue
        for (auto f : present_queue) {
            free_queue.push(f);
        }
        present_queue.clear();
        previous_frame = frame;
        return frame;
    }
};

RendererOpenGL::RendererOpenGL(Frontend::EmuWindow& window) : RendererBase{window} {
    window.mailbox = std::make_shared<OGLTextureMailbox>();
}

RendererOpenGL::~RendererOpenGL() = default;

MICROPROFILE_DEFINE(OpenGL_RenderFrame, "OpenGL", "Render Frame", MP_RGB(128, 128, 64));
MICROPROFILE_DEFINE(OpenGL_WaitPresent, "OpenGL", "Wait For Present", MP_RGB(128, 128, 128));

/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {
    // Maintain the rasterizer's state as a priority
    OpenGLState prev_state = OpenGLState::GetCurState();
    state.Apply();

    PrepareRendertarget();

    // RenderScreenshot();

    // RenderVideoDumping();

    const auto& layout = render_window.GetFramebufferLayout();

    Frontend::Frame* frame;
    {
        MICROPROFILE_SCOPE(OpenGL_WaitPresent);

        frame = render_window.mailbox->GetRenderFrame();

        // Clean up sync objects before drawing

        // INTEL driver workaround. We can't delete the previous render sync object until we are
        // sure that the presentation is done
        if (frame->present_fence) {
            glClientWaitSync(frame->present_fence, 0, GL_TIMEOUT_IGNORED);
        }

        // delete the draw fence if the frame wasn't presented
        if (frame->render_fence) {
            glDeleteSync(frame->render_fence);
            frame->render_fence = 0;
        }

        // wait for the presentation to be done
        if (frame->present_fence) {
            glWaitSync(frame->present_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(frame->present_fence);
            frame->present_fence = 0;
        }
    }
    {
        MICROPROFILE_SCOPE(OpenGL_RenderFrame);

        // Recreate the frame if the res_scale has changed
        u16 res_scale = VideoCore::GetResolutionScaleFactor();
        if (res_scale != frame->res_scale) {
            LOG_CRITICAL(Render_OpenGL, "Reloading render frame");
            render_window.mailbox->ReloadRenderFrame(frame, res_scale);
        }

        state.draw.read_framebuffer = swap_framebuffer.handle;
        state.draw.draw_framebuffer = 0;
        state.Apply();

        for (auto i : {0, 1, 2}) {
            const auto& read = screen_infos[i];
            auto& draw = frame->screens[i];
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw.render.handle);
            glFramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, read.display_texture,
                                 0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindTexture(GL_TEXTURE_2D, draw.texture.handle);

            u32 x = std::min(read.display_texcoords.left, read.display_texcoords.right);
            u32 y = std::min(read.display_texcoords.bottom, read.display_texcoords.top);
            // due to 3DS screen rotate, width refers to top/bottom and height is right/left
            u32 width = std::abs(read.display_texcoords.bottom - read.display_texcoords.top) *
                        read.texture.width * res_scale;
            u32 height = std::abs(read.display_texcoords.right - read.display_texcoords.left) *
                         read.texture.height * res_scale;
            glBlitFramebuffer(x, y, width, height, 0, 0, draw.scaled_width, draw.scaled_height,
                              GL_COLOR_BUFFER_BIT,
                              Settings::values.filter_mode ? GL_LINEAR : GL_NEAREST);
        }

        // Create a fence for the frontend to wait on and swap this frame to OffTex
        frame->render_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        render_window.mailbox->ReleaseRenderFrame(frame);
        m_current_frame++;
    }

    Core::System::GetInstance().perf_stats->EndSystemFrame();

    render_window.PollEvents();

    Core::System::GetInstance().frame_limiter.DoFrameLimiting(
        Core::System::GetInstance().CoreTiming().GetGlobalTimeUs());
    Core::System::GetInstance().perf_stats->BeginSystemFrame();

    prev_state.Apply();
    RefreshRasterizerSetting();

    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        Pica::g_debug_context->recorder->FrameFinished();
    }
}

void RendererOpenGL::PrepareRendertarget() {
    for (int i : {0, 1, 2}) {
        int fb_id = i == 2 ? 1 : 0;
        const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];

        // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
        u32 lcd_color_addr =
            (fb_id == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (color_fill.is_enabled) {
            LoadColorToActiveGLTexture(color_fill.color_r, color_fill.color_g, color_fill.color_b,
                                       screen_infos[i].texture);

            // Resize the texture in case the framebuffer size has changed
            screen_infos[i].texture.width = 1;
            screen_infos[i].texture.height = 1;
        } else {
            if (screen_infos[i].texture.width != (GLsizei)framebuffer.width ||
                screen_infos[i].texture.height != (GLsizei)framebuffer.height ||
                screen_infos[i].texture.format != framebuffer.color_format) {
                // Reallocate texture if the framebuffer size has changed.
                // This is expected to not happen very often and hence should not be a
                // performance problem.
                ConfigureFramebufferTexture(screen_infos[i].texture, framebuffer);
            }
            // Resize the texture in case the framebuffer size has changed
            screen_infos[i].texture.width = framebuffer.width;
            screen_infos[i].texture.height = framebuffer.height;
            LoadFBToScreenInfo(framebuffer, screen_infos[i], i == 1);
        }
    }
}

void RendererOpenGL::RenderVideoDumping() {
    if (cleanup_video_dumping.exchange(false)) {
        ReleaseVideoDumpingGLObjects();
    }

    if (Core::System::GetInstance().VideoDumper().IsDumping()) {
        if (prepare_video_dumping.exchange(false)) {
            InitVideoDumpingGLObjects();
        }

        const auto& layout = Core::System::GetInstance().VideoDumper().GetLayout();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_dumping_framebuffer.handle);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_dumping_framebuffer.handle);
        // TODO move video dumping to a new context
        // DrawScreens(layout);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, frame_dumping_pbos[current_pbo].handle);
        glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, frame_dumping_pbos[next_pbo].handle);

        GLubyte* pixels = static_cast<GLubyte*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
        VideoDumper::VideoFrame frame_data{layout.width, layout.height, pixels};
        Core::System::GetInstance().VideoDumper().AddVideoFrame(frame_data);

        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        current_pbo = (current_pbo + 1) % 2;
        next_pbo = (current_pbo + 1) % 2;
    }
}

/**
 * Loads framebuffer from emulated memory into the active OpenGL texture.
 */
void RendererOpenGL::LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                                        ScreenInfo& screen_info, bool right_eye) {
    if (framebuffer.address_right1 == 0 || framebuffer.address_right2 == 0)
        right_eye = false;

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0
            ? (!right_eye ? framebuffer.address_left1 : framebuffer.address_right1)
            : (!right_eye ? framebuffer.address_left2 : framebuffer.address_right2);

    LOG_TRACE(Render_OpenGL, "0x{:08x} bytes from 0x{:08x}({}x{}), fmt {:x}",
              framebuffer.stride * framebuffer.height, framebuffer_addr, (int)framebuffer.width,
              (int)framebuffer.height, (int)framebuffer.format);

    int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    std::size_t pixel_stride = framebuffer.stride / bpp;

    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    ASSERT(pixel_stride * bpp == framebuffer.stride);

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    ASSERT(pixel_stride % 4 == 0);

    if (!Rasterizer()->AccelerateDisplay(framebuffer, framebuffer_addr,
                                         static_cast<u32>(pixel_stride), screen_info)) {
        // Reset the screen info's display texture to its own permanent texture
        screen_info.display_texture = screen_info.texture.resource.handle;
        screen_info.display_texcoords = Common::Rectangle<float>(0.f, 0.f, 1.f, 1.f);

        Memory::RasterizerFlushRegion(framebuffer_addr, framebuffer.stride * framebuffer.height);

        const u8* framebuffer_data = VideoCore::g_memory->GetPhysicalPointer(framebuffer_addr);

        state.texture_units[0].texture_2d = screen_info.texture.resource.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pixel_stride);

        // Update existing texture
        // TODO: Test what happens on hardware when you change the framebuffer dimensions so that
        //       they differ from the LCD resolution.
        // TODO: Applications could theoretically crash Citra here by specifying too large
        //       framebuffer sizes. We should make sure that this cannot happen.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer.width, framebuffer.height,
                        screen_info.texture.gl_format, screen_info.texture.gl_type,
                        framebuffer_data);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        state.texture_units[0].texture_2d = 0;
        state.Apply();
    }
}

/**
 * Fills active OpenGL texture with the given RGB color. Since the color is solid, the texture can
 * be 1x1 but will stretch across whatever it's rendered on.
 */
void RendererOpenGL::LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b,
                                                const TextureInfo& texture) {
    state.texture_units[0].texture_2d = texture.resource.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    u8 framebuffer_data[3] = {color_r, color_g, color_b};

    // Update existing texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer_data);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

/**
 * Initializes the OpenGL state and creates persistent objects.
 */
void RendererOpenGL::InitOpenGLObjects() {
    swap_framebuffer.Create();

    // Allocate textures for each screen
    for (auto& screen_info : screen_infos) {
        screen_info.texture.resource.Create();

        // Allocation of storage is deferred until the first frame, when we
        // know the framebuffer size.

        state.texture_units[0].texture_2d = screen_info.texture.resource.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        screen_info.display_texture = screen_info.texture.resource.handle;
    }

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

void RendererOpenGL::ConfigureFramebufferTexture(TextureInfo& texture,
                                                 const GPU::Regs::FramebufferConfig& framebuffer) {
    GPU::Regs::PixelFormat format = framebuffer.color_format;
    GLint internal_format;

    texture.format = format;
    texture.width = framebuffer.width;
    texture.height = framebuffer.height;

    switch (format) {
    case GPU::Regs::PixelFormat::RGBA8:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GLES ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT_8_8_8_8;
        break;

    case GPU::Regs::PixelFormat::RGB8:
        // This pixel format uses BGR since GL_UNSIGNED_BYTE specifies byte-order, unlike every
        // specific OpenGL type used in this function using native-endian (that is, little-endian
        // mostly everywhere) for words or half-words.
        // TODO: check how those behave on big-endian processors.
        internal_format = GL_RGB;

        // GLES Dosen't support BGR , Use RGB instead
        texture.gl_format = GLES ? GL_RGB : GL_BGR;
        texture.gl_type = GL_UNSIGNED_BYTE;
        break;

    case GPU::Regs::PixelFormat::RGB565:
        internal_format = GL_RGB;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case GPU::Regs::PixelFormat::RGB5A1:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case GPU::Regs::PixelFormat::RGBA4:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        break;

    default:
        UNIMPLEMENTED();
    }

    state.texture_units[0].texture_2d = texture.resource.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0,
                 texture.gl_format, texture.gl_type, nullptr);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

void RendererOpenGL::PrepareVideoDumping() {
    prepare_video_dumping = true;
}

void RendererOpenGL::CleanupVideoDumping() {
    cleanup_video_dumping = true;
}

void RendererOpenGL::InitVideoDumpingGLObjects() {
    const auto& layout = Core::System::GetInstance().VideoDumper().GetLayout();

    frame_dumping_framebuffer.Create();
    glGenRenderbuffers(1, &frame_dumping_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, frame_dumping_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, layout.width, layout.height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_dumping_framebuffer.handle);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              frame_dumping_renderbuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    for (auto& buffer : frame_dumping_pbos) {
        buffer.Create();
        glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer.handle);
        glBufferData(GL_PIXEL_PACK_BUFFER, layout.width * layout.height * 4, nullptr,
                     GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
}

void RendererOpenGL::ReleaseVideoDumpingGLObjects() {
    frame_dumping_framebuffer.Release();
    glDeleteRenderbuffers(1, &frame_dumping_renderbuffer);

    for (auto& buffer : frame_dumping_pbos) {
        buffer.Release();
    }
}

static const char* GetSource(GLenum source) {
#define RET(s)                                                                                     \
    case GL_DEBUG_SOURCE_##s:                                                                      \
        return #s
    switch (source) {
        RET(API);
        RET(WINDOW_SYSTEM);
        RET(SHADER_COMPILER);
        RET(THIRD_PARTY);
        RET(APPLICATION);
        RET(OTHER);
    default:
        UNREACHABLE();
    }
#undef RET
}

static const char* GetType(GLenum type) {
#define RET(t)                                                                                     \
    case GL_DEBUG_TYPE_##t:                                                                        \
        return #t
    switch (type) {
        RET(ERROR);
        RET(DEPRECATED_BEHAVIOR);
        RET(UNDEFINED_BEHAVIOR);
        RET(PORTABILITY);
        RET(PERFORMANCE);
        RET(OTHER);
        RET(MARKER);
    default:
        UNREACHABLE();
    }
#undef RET
}

static void APIENTRY DebugHandler(GLenum source, GLenum type, GLuint id, GLenum severity,
                                  GLsizei length, const GLchar* message, const void* user_param) {
    Log::Level level;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        level = Log::Level::Critical;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        level = Log::Level::Warning;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    case GL_DEBUG_SEVERITY_LOW:
        level = Log::Level::Debug;
        break;
    }
    LOG_GENERIC(Log::Class::Render_OpenGL, level, "{} {} {}: {}", GetSource(source), GetType(type),
                id, message);
}

/// Initialize the renderer
VideoCore::ResultStatus RendererOpenGL::Init() {
    if (!gladLoadGL()) {
        return VideoCore::ResultStatus::ErrorBelowGL33;
    }

    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugHandler, nullptr);
    }

    const char* gl_version{reinterpret_cast<char const*>(glGetString(GL_VERSION))};
    const char* gpu_vendor{reinterpret_cast<char const*>(glGetString(GL_VENDOR))};
    const char* gpu_model{reinterpret_cast<char const*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    auto& telemetry_session = Core::System::GetInstance().TelemetrySession();
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_Vendor", gpu_vendor);
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_Model", gpu_model);
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_OpenGL_Version", gl_version);

    if (!strcmp(gpu_vendor, "GDI Generic")) {
        return VideoCore::ResultStatus::ErrorGenericDrivers;
    }

    if (!(GLAD_GL_VERSION_3_3 || GLAD_GL_ES_VERSION_3_1)) {
        return VideoCore::ResultStatus::ErrorBelowGL33;
    }

    InitOpenGLObjects();

    RefreshRasterizerSetting();

    return VideoCore::ResultStatus::Success;
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {}

} // namespace OpenGL
