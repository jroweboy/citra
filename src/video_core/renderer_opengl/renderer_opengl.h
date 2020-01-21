// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/hw/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Frontend {

struct Frame {
    struct Screen {
        OpenGL::OGLTexture texture;
        u32 scaled_width;
        u32 scaled_height;
    };
    std::array<Screen, 3> screens; /// 3DS textures (TopLeft, TopRight, Bottom)

    OpenGL::OGLFramebuffer render{};  /// FBO created on the render thread
    OpenGL::OGLFramebuffer present{}; /// FBO created on the present thread

    bool texture_reloaded = false; /// Texture attachment was recreated (ie: resized)

    u16 res_scale; /// Used to define the texture size

    GLsync render_fence{};  /// Fence created on the render thread
    GLsync present_fence{}; /// Fence created on the presentation thread
};
} // namespace Frontend

namespace OpenGL {

/// Structure used for storing information about the textures for each 3DS screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GPU::Regs::PixelFormat format;
    GLenum gl_format;
    GLenum gl_type;
};

/// Structure used for storing information about the display target for each 3DS screen
struct ScreenInfo {
    GLuint display_texture;
    Common::Rectangle<float> display_texcoords;
    TextureInfo texture;
};

class RendererOpenGL : public RendererBase {
public:
    explicit RendererOpenGL(Frontend::EmuWindow& window);
    ~RendererOpenGL() override;

    /// Initialize the renderer
    VideoCore::ResultStatus Init() override;

    /// Shutdown the renderer
    void ShutDown() override;

    /// Finalizes rendering the guest frame
    void SwapBuffers() override;

    /// Prepares for video dumping (e.g. create necessary buffers, etc)
    void PrepareVideoDumping() override;

    /// Cleans up after video dumping is ended
    void CleanupVideoDumping() override;

private:
    void InitOpenGLObjects();
    void PrepareRendertarget();
    void RenderVideoDumping();
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const GPU::Regs::FramebufferConfig& framebuffer);

    // Loads framebuffer from emulated memory into the display information structure
    void LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                            ScreenInfo& screen_info, bool right_eye);
    // Fills active OpenGL texture with the given RGB color.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, const TextureInfo& texture);

    void InitVideoDumpingGLObjects();
    void ReleaseVideoDumpingGLObjects();

    OpenGLState state;
    OGLFramebuffer swap_framebuffer;

    /// Display information for top and bottom screens respectively
    std::array<ScreenInfo, 3> screen_infos;

    // Frame dumping
    OGLFramebuffer frame_dumping_framebuffer;
    GLuint frame_dumping_renderbuffer;

    // Whether prepare/cleanup video dumping has been requested.
    // They will be executed on next frame.
    std::atomic_bool prepare_video_dumping = false;
    std::atomic_bool cleanup_video_dumping = false;

    // PBOs used to dump frames faster
    std::array<OGLBuffer, 2> frame_dumping_pbos;
    GLuint current_pbo = 1;
    GLuint next_pbo = 0;
};

} // namespace OpenGL
