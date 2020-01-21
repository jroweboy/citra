// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <mutex>
#include <glad/glad.h>
#include "common/logging/log.h"
#include "core/frontend/video_presentation.h"
#include "core/settings.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/renderer_opengl/post_processing_opengl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace Frontend {

constexpr float LEFT_MARGIN = 10.0f;   // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;    // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f; // Pixels between subsequent OSD messages.

using namespace std::chrono_literals;

struct Message {
    std::string message;
    u32 color;
    std::chrono::milliseconds duration;
    Position position;
};

struct FPS : Message {
    std::function<std::string()> value_provider;
};

struct Progress : Message {
    std::function<std::tuple<u32, u32>()> value_provider;
};

class OnScreenDisplay::MessageQueue {
public:
    MessageQueue() = default;

    std::multimap<MessageType, Message> queue = {};
    std::mutex queue_mutex = {};

    // Single-type messages only have one of each at at time, so don't add them to the multimap
    FPS fps = {};
    std::atomic<bool> show_fps = false;

    std::vector<Progress> progress = {};
};

OnScreenDisplay::OnScreenDisplay() : queue(std::make_unique<OnScreenDisplay::MessageQueue>()) {
    // TODO init imgui
}

void OnScreenDisplay::AddMessage(std::string message, MessageType type,
                                 std::chrono::milliseconds ms, u32 rgba) {}

void OnScreenDisplay::ShowFPS(std::string message, std::function<std::string()> value_provider,
                              Position position) {
    if (queue->show_fps) {
        return;
    }
    queue->fps = {message, Color::YELLOW, 0ms, position, value_provider};
    queue->show_fps = true;
}

void OnScreenDisplay::RemoveFPS() {
    if (!queue->show_fps) {
        return;
    }
    queue->show_fps = false;
}

void OnScreenDisplay::ShowProgress(std::string message,
                                   std::function<std::tuple<u32, u32>()> value_provider,
                                   Position position) {
    queue->progress.push_back({message, Color::WHITE, 0ms, position, value_provider});
}

void OnScreenDisplay::Render() {
    // LOG_CRITICAL(Render_OpenGL, "Rendering OSD");
    if (queue->show_fps) {
        LOG_ERROR(Render_OpenGL, "{} {}", queue->fps.message, queue->fps.value_provider());
    }
    for (auto& progress : queue->progress) {
        auto [current, total] = progress.value_provider();
        LOG_ERROR(Render_OpenGL, "{} {} / {}", progress.message, current, total);
    }
}

static constexpr char vertex_shader[] = R"(
in vec2 vert_position;
in vec2 vert_tex_coord;
out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

static constexpr char fragment_shader[] = R"(
in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;

uniform vec4 i_resolution;
uniform vec4 o_resolution;
uniform int layer;

uniform sampler2D color_texture;

void main() {
    color = texture(color_texture, frag_tex_coord);
}
)";

static constexpr char fragment_shader_anaglyph[] = R"(

// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009
const mat3 l = mat3( 0.437, 0.449, 0.164,
              -0.062,-0.062,-0.024,
              -0.048,-0.050,-0.017);
const mat3 r = mat3(-0.011,-0.032,-0.007,
               0.377, 0.761, 0.009,
              -0.026,-0.093, 1.234);

in vec2 frag_tex_coord;
out vec4 color;

uniform vec4 resolution;
uniform int layer;

uniform sampler2D color_texture;
uniform sampler2D color_texture_r;

void main() {
    vec4 color_tex_l = texture(color_texture, frag_tex_coord);
    vec4 color_tex_r = texture(color_texture_r, frag_tex_coord);
    color = vec4(color_tex_l.rgb*l+color_tex_r.rgb*r, color_tex_l.a);
}
)";

static constexpr char fragment_shader_interlaced[] = R"(

in vec2 frag_tex_coord;
out vec4 color;

uniform vec4 o_resolution;

uniform sampler2D color_texture;
uniform sampler2D color_texture_r;

void main() {
    float screen_row = o_resolution.x * frag_tex_coord.x;
    if (int(screen_row) % 2 == 0)
        color = texture(color_texture, frag_tex_coord);
    else
        color = texture(color_texture_r, frag_tex_coord);
}
)";

/**
 * Vertex structure that the drawn screen rectangles are composed of.
 */
struct ScreenRectVertex {
    ScreenRectVertex(GLfloat x, GLfloat y, GLfloat u, GLfloat v) {
        position[0] = x;
        position[1] = y;
        tex_coord[0] = u;
        tex_coord[1] = v;
    }

    GLfloat position[2];
    GLfloat tex_coord[2];
};

/**
 * Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
 * corner and (width, height) on the lower-bottom.
 *
 * The projection part of the matrix is trivial, hence these operations are represented
 * by a 3x2 matrix.
 */
static std::array<GLfloat, 3 * 2> MakeOrthographicMatrix(const float width, const float height) {
    std::array<GLfloat, 3 * 2> matrix; // Laid out in column-major order

    // clang-format off
    matrix[0] = 2.f / width; matrix[2] = 0.f;           matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] = 1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].
    // clang-format on

    return matrix;
}

using namespace OpenGL;
class VideoPresentation::Impl {
public:
    // OpenGL object IDs
    OGLVertexArray vertex_array{};
    OGLBuffer vertex_buffer{};
    OGLProgram shader{};
    OGLFramebuffer screenshot_framebuffer{};
    OGLRenderbuffer screenshot_storage{};
    OGLSampler filter_sampler{};

    // Shader uniform location indices
    GLuint uniform_modelview_matrix;
    GLuint uniform_color_texture;
    GLuint uniform_color_texture_r;

    // Shader uniform for Dolphin compatibility
    GLuint uniform_i_resolution;
    GLuint uniform_o_resolution;
    GLuint uniform_layer;

    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_tex_coord;

    void Init() {
        glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue,
                     0.0f);

        filter_sampler.Create();
        screenshot_framebuffer.Create();
        screenshot_storage.Create();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, screenshot_framebuffer.handle);
        glBindRenderbuffer(GL_RENDERBUFFER, screenshot_storage.handle);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  screenshot_storage.handle);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        ReloadSampler();

        ReloadShader();

        // Generate VBO handle for drawing
        vertex_buffer.Create();

        // Generate VAO
        vertex_array.Create();

        glBindVertexArray(vertex_array.handle);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer.handle);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Attach vertex data to VAO
        glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
        glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex),
                              (GLvoid*)offsetof(ScreenRectVertex, position));
        glVertexAttribPointer(attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex),
                              (GLvoid*)offsetof(ScreenRectVertex, tex_coord));
        glEnableVertexAttribArray(attrib_position);
        glEnableVertexAttribArray(attrib_tex_coord);
    }

    void ReloadSampler() {
        glSamplerParameteri(filter_sampler.handle, GL_TEXTURE_MIN_FILTER,
                            Settings::values.filter_mode ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(filter_sampler.handle, GL_TEXTURE_MAG_FILTER,
                            Settings::values.filter_mode ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(filter_sampler.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(filter_sampler.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void ReloadShader() {
        // Link shaders and get variable locations
        std::string shader_data;
        if (GLES) {
            shader_data += fragment_shader_precision_OES;
        }
        if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph) {
            if (Settings::values.pp_shader_name == "dubois (builtin)") {
                shader_data += fragment_shader_anaglyph;
            } else {
                std::string shader_text =
                    OpenGL::GetPostProcessingShaderCode(true, Settings::values.pp_shader_name);
                if (shader_text.empty()) {
                    // Should probably provide some information that the shader couldn't load
                    shader_data += fragment_shader_anaglyph;
                } else {
                    shader_data += shader_text;
                }
            }
        } else if (Settings::values.render_3d == Settings::StereoRenderOption::Interlaced) {
            if (Settings::values.pp_shader_name == "horizontal (builtin)") {
                shader_data += fragment_shader_interlaced;
            } else {
                std::string shader_text =
                    OpenGL::GetPostProcessingShaderCode(true, Settings::values.pp_shader_name);
                if (shader_text.empty()) {
                    // Should probably provide some information that the shader couldn't load
                    shader_data += fragment_shader_interlaced;
                } else {
                    shader_data += shader_text;
                }
            }
        } else {
            if (Settings::values.pp_shader_name == "none (builtin)") {
                shader_data += fragment_shader;
            } else {
                std::string shader_text =
                    OpenGL::GetPostProcessingShaderCode(false, Settings::values.pp_shader_name);
                if (shader_text.empty()) {
                    // Should probably provide some information that the shader couldn't load
                    shader_data += fragment_shader;
                } else {
                    shader_data += shader_text;
                }
            }
        }
        shader.Create(vertex_shader, shader_data.c_str());
        glUseProgram(shader.handle);

        uniform_modelview_matrix = glGetUniformLocation(shader.handle, "modelview_matrix");
        uniform_color_texture = glGetUniformLocation(shader.handle, "color_texture");
        if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph ||
            Settings::values.render_3d == Settings::StereoRenderOption::Interlaced) {
            uniform_color_texture_r = glGetUniformLocation(shader.handle, "color_texture_r");
        }
        uniform_i_resolution = glGetUniformLocation(shader.handle, "i_resolution");
        uniform_o_resolution = glGetUniformLocation(shader.handle, "o_resolution");
        uniform_layer = glGetUniformLocation(shader.handle, "layer");
        attrib_position = glGetAttribLocation(shader.handle, "vert_position");
        attrib_tex_coord = glGetAttribLocation(shader.handle, "vert_tex_coord");
    }

    void DrawScreens(const Layout::FramebufferLayout& layout, const Frame& frame) {
        if (VideoCore::g_renderer_bg_color_update_requested.exchange(false)) {
            // Update background color before drawing
            glClearColor(Settings::values.bg_red, Settings::values.bg_green,
                         Settings::values.bg_blue, 0.0f);
        }

        if (VideoCore::g_renderer_sampler_update_requested.exchange(false)) {
            // Set the new filtering mode for the sampler
            ReloadSampler();
        }

        if (VideoCore::g_renderer_shader_update_requested.exchange(false)) {
            // Update fragment shader before drawing
            shader.Release();
            // Link shaders and get variable locations
            ReloadShader();
        }

        const auto& top_screen = layout.top_screen;
        const auto& bottom_screen = layout.bottom_screen;

        glViewport(0, 0, layout.width, layout.height);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set projection matrix
        std::array<GLfloat, 3 * 2> ortho_matrix =
            MakeOrthographicMatrix((float)layout.width, (float)layout.height);
        glUniformMatrix3x2fv(uniform_modelview_matrix, 1, GL_FALSE, ortho_matrix.data());

        // Bind texture in Texture Unit 0
        glUniform1i(uniform_color_texture, 0);

        const bool stereo_single_screen =
            Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph ||
            Settings::values.render_3d == Settings::StereoRenderOption::Interlaced;

        // Bind a second texture for the right eye if in Anaglyph mode
        if (stereo_single_screen) {
            glUniform1i(uniform_color_texture_r, 1);
        }

        auto& [topl, topr, bot] = frame.screens;

        glUniform1i(uniform_layer, 0);
        if (layout.top_screen_enabled) {
            if (layout.is_rotated) {
                if (Settings::values.render_3d == Settings::StereoRenderOption::Off) {
                    DrawSingleScreenRotated(topl, (float)top_screen.left, (float)top_screen.top,
                                            (float)top_screen.GetWidth(),
                                            (float)top_screen.GetHeight());
                } else if (Settings::values.render_3d == Settings::StereoRenderOption::SideBySide) {
                    DrawSingleScreenRotated(topl, (float)top_screen.left / 2, (float)top_screen.top,
                                            (float)top_screen.GetWidth() / 2,
                                            (float)top_screen.GetHeight());
                    glUniform1i(uniform_layer, 1);
                    DrawSingleScreenRotated(
                        topr, ((float)top_screen.left / 2) + ((float)layout.width / 2),
                        (float)top_screen.top, (float)top_screen.GetWidth() / 2,
                        (float)top_screen.GetHeight());
                } else if (stereo_single_screen) {
                    DrawSingleScreenStereoRotated(
                        topl, topr, (float)top_screen.left, (float)top_screen.top,
                        (float)top_screen.GetWidth(), (float)top_screen.GetHeight());
                }
            } else {
                if (Settings::values.render_3d == Settings::StereoRenderOption::Off) {
                    DrawSingleScreen(topl, (float)top_screen.left, (float)top_screen.top,
                                     (float)top_screen.GetWidth(), (float)top_screen.GetHeight());
                } else if (Settings::values.render_3d == Settings::StereoRenderOption::SideBySide) {
                    DrawSingleScreen(topl, (float)top_screen.left / 2, (float)top_screen.top,
                                     (float)top_screen.GetWidth() / 2,
                                     (float)top_screen.GetHeight());
                    glUniform1i(uniform_layer, 1);
                    DrawSingleScreen(topr, ((float)top_screen.left / 2) + ((float)layout.width / 2),
                                     (float)top_screen.top, (float)top_screen.GetWidth() / 2,
                                     (float)top_screen.GetHeight());
                } else if (stereo_single_screen) {
                    DrawSingleScreenStereo(topl, topr, (float)top_screen.left,
                                           (float)top_screen.top, (float)top_screen.GetWidth(),
                                           (float)top_screen.GetHeight());
                }
            }
        }
        glUniform1i(uniform_layer, 0);
        if (layout.bottom_screen_enabled) {
            if (layout.is_rotated) {
                if (Settings::values.render_3d == Settings::StereoRenderOption::Off) {
                    DrawSingleScreenRotated(
                        bot, (float)bottom_screen.left, (float)bottom_screen.top,
                        (float)bottom_screen.GetWidth(), (float)bottom_screen.GetHeight());
                } else if (Settings::values.render_3d == Settings::StereoRenderOption::SideBySide) {
                    DrawSingleScreenRotated(
                        bot, (float)bottom_screen.left / 2, (float)bottom_screen.top,
                        (float)bottom_screen.GetWidth() / 2, (float)bottom_screen.GetHeight());
                    glUniform1i(uniform_layer, 1);
                    DrawSingleScreenRotated(
                        bot, ((float)bottom_screen.left / 2) + ((float)layout.width / 2),
                        (float)bottom_screen.top, (float)bottom_screen.GetWidth() / 2,
                        (float)bottom_screen.GetHeight());
                } else if (stereo_single_screen) {
                    DrawSingleScreenStereoRotated(
                        bot, bot, (float)bottom_screen.left, (float)bottom_screen.top,
                        (float)bottom_screen.GetWidth(), (float)bottom_screen.GetHeight());
                }
            } else {
                if (Settings::values.render_3d == Settings::StereoRenderOption::Off) {
                    DrawSingleScreen(bot, (float)bottom_screen.left, (float)bottom_screen.top,
                                     (float)bottom_screen.GetWidth(),
                                     (float)bottom_screen.GetHeight());
                } else if (Settings::values.render_3d == Settings::StereoRenderOption::SideBySide) {
                    DrawSingleScreen(bot, (float)bottom_screen.left / 2, (float)bottom_screen.top,
                                     (float)bottom_screen.GetWidth() / 2,
                                     (float)bottom_screen.GetHeight());
                    glUniform1i(uniform_layer, 1);
                    DrawSingleScreen(bot,
                                     ((float)bottom_screen.left / 2) + ((float)layout.width / 2),
                                     (float)bottom_screen.top, (float)bottom_screen.GetWidth() / 2,
                                     (float)bottom_screen.GetHeight());
                } else if (stereo_single_screen) {
                    DrawSingleScreenStereo(
                        bot, bot, (float)bottom_screen.left, (float)bottom_screen.top,
                        (float)bottom_screen.GetWidth(), (float)bottom_screen.GetHeight());
                }
            }
        }
    }

    void DrawSingleScreen(const Frame::Screen& screen, float x, float y, float w, float h) {

        const std::array<ScreenRectVertex, 4> vertices = {{
            ScreenRectVertex(x, y, 1, 1),
            ScreenRectVertex(x + w, y, 0, 1),
            ScreenRectVertex(x, y + h, 1, 0),
            ScreenRectVertex(x + w, y + h, 0, 0),
        }};

        glUniform4f(uniform_i_resolution, screen.scaled_width, screen.scaled_height,
                    1.0 / screen.scaled_width, 1.0 / screen.scaled_height);
        glUniform4f(uniform_o_resolution, w, h, 1.0f / w, 1.0f / h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screen.texture.handle);
        glBindSampler(0, filter_sampler.handle);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void DrawSingleScreenStereo(const Frame::Screen& screen_l, const Frame::Screen& screen_r,
                                float x, float y, float w, float h) {

        const std::array<ScreenRectVertex, 4> vertices = {{
            ScreenRectVertex(x, y, 1, 1),
            ScreenRectVertex(x + w, y, 0, 1),
            ScreenRectVertex(x, y + h, 1, 0),
            ScreenRectVertex(x + w, y + h, 0, 0),
        }};

        glUniform4f(uniform_i_resolution, screen_l.scaled_width, screen_l.scaled_height,
                    1.0 / screen_l.scaled_width, 1.0 / screen_l.scaled_height);
        glUniform4f(uniform_o_resolution, w, h, 1.0f / w, 1.0f / h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screen_l.texture.handle);
        glBindSampler(0, filter_sampler.handle);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, screen_r.texture.handle);
        glBindSampler(1, filter_sampler.handle);

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void DrawSingleScreenRotated(const Frame::Screen& screen, float x, float y, float w, float h) {

        const std::array<ScreenRectVertex, 4> vertices = {{
            ScreenRectVertex(x, y, 1, 0),
            ScreenRectVertex(x + w, y, 1, 1),
            ScreenRectVertex(x, y + h, 0, 0),
            ScreenRectVertex(x + w, y + h, 0, 1),
        }};

        // As this is the "DrawSingleScreenRotated" function, the output resolution dimensions have
        // been swapped. If a non-rotated draw-screen function were to be added for book-mode games,
        // those should probably be set to the standard (w, h, 1.0 / w, 1.0 / h) ordering.
        glUniform4f(uniform_i_resolution, screen.scaled_width, screen.scaled_height,
                    1.0 / screen.scaled_width, 1.0 / screen.scaled_height);
        glUniform4f(uniform_o_resolution, h, w, 1.0f / h, 1.0f / w);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screen.texture.handle);
        glBindSampler(0, filter_sampler.handle);

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void DrawSingleScreenStereoRotated(const Frame::Screen& screen_l, const Frame::Screen& screen_r,
                                       float x, float y, float w, float h) {

        const std::array<ScreenRectVertex, 4> vertices = {{
            ScreenRectVertex(x, y, 1, 0),
            ScreenRectVertex(x + w, y, 1, 1),
            ScreenRectVertex(x, y + h, 0, 0),
            ScreenRectVertex(x + w, y + h, 0, 1),
        }};

        glUniform4f(uniform_i_resolution, screen_l.scaled_width, screen_l.scaled_height,
                    1.0 / screen_l.scaled_width, 1.0 / screen_l.scaled_height);
        glUniform4f(uniform_o_resolution, h, w, 1.0f / h, 1.0f / w);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screen_l.texture.handle);
        glBindSampler(0, filter_sampler.handle);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, screen_r.texture.handle);
        glBindSampler(1, filter_sampler.handle);

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void CaptureScreenshot(const Frontend::Frame& frame, void* write_ptr,
                           const Layout::FramebufferLayout& layout,
                           std::function<void(void)> callback) {

        // TODO: Don't query the framebuffer binding, find a better way to restore whatever the
        // frontend sets as the FBO
        GLint original_draw_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &original_draw_fbo);

        // Draw this frame to the screenshot framebuffer (and set it as the read buffer to read the
        // pixels back after)
        glBindFramebuffer(GL_FRAMEBUFFER, screenshot_framebuffer.handle);

        // Recreate the screenshot_storage. (TODO: don't recreate it if its already large enough)
        glBindRenderbuffer(GL_RENDERBUFFER, screenshot_storage.handle);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, layout.width, layout.height);

        DrawScreens(layout, frame);

        // Now read back the screenshot we just drew
        glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     write_ptr);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, original_draw_fbo);

        callback();
    }
};

VideoPresentation::VideoPresentation() : osd(), impl(std::make_unique<Impl>()) {}

VideoPresentation::~VideoPresentation() = default;

void VideoPresentation::Init() {
    impl->Init();
}

void VideoPresentation::Present(const Layout::FramebufferLayout& layout) {
    using namespace std::chrono_literals;
    auto frame = mailbox->TryGetPresentFrame(0ms);
    if (!frame) {
        LOG_DEBUG(Render_OpenGL, "TryGetPresentFrame returned no frame to present");
        return;
    }

    // Clearing before a full overwrite of a fbo can signal to drivers that they can avoid a
    // readback since we won't be doing any blending
    glClear(GL_COLOR_BUFFER_BIT);

    // Recreate the presentation FBO if the color attachment was changed
    if (frame->texture_reloaded) {
        LOG_CRITICAL(Render_OpenGL, "Reloading present frame");
        mailbox->ReloadPresentFrame(frame);
    }
    glWaitSync(frame->render_fence, 0, GL_TIMEOUT_IGNORED);
    // INTEL workaround.
    // Normally we could just delete the draw fence here, but due to driver bugs, we can just
    // delete it on the emulation thread without too much penalty
    // glDeleteSync(frame.render_sync);
    // frame.render_sync = 0;

    impl->DrawScreens(layout, *frame);

    osd.Render();

    /* insert fence for the main thread to block on */
    frame->present_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
}

void VideoPresentation::EnableMailbox(std::shared_ptr<Frontend::TextureMailbox> mailbox_) {
    mailbox = mailbox_;
}

void VideoPresentation::CaptureScreenshot(void* write_ptr, const Layout::FramebufferLayout& layout,
                                          std::function<void(void)> callback) {
    auto frame = mailbox->TryGetPresentFrame(0ms);
    if (!frame) {
        LOG_DEBUG(Render_OpenGL, "Could not capture screenshot");
        return;
    }
    impl->CaptureScreenshot(*frame, write_ptr, layout, callback);
}

void VideoPresentation::ToggleOSD(bool osd) {
    osd_enabled.exchange(osd);
}

OnScreenDisplay& VideoPresentation::GetOSD() {
    return osd;
}

const OnScreenDisplay& VideoPresentation::GetOSD() const {
    return osd;
}

} // namespace Frontend