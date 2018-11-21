// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <boost/variant.hpp>
#include "common/file_util.h"
#include "common/linear_disk_cache.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"

static void SetShaderUniformBlockBinding(GLuint shader, const char* name, UniformBindings binding,
                                         std::size_t expected_size) {
    GLuint ub_index = glGetUniformBlockIndex(shader, name);
    if (ub_index == GL_INVALID_INDEX) {
        return;
    }
    GLint ub_size = 0;
    glGetActiveUniformBlockiv(shader, ub_index, GL_UNIFORM_BLOCK_DATA_SIZE, &ub_size);
    ASSERT_MSG(ub_size == expected_size, "Uniform block size did not match! Got {}, expected {}",
               static_cast<int>(ub_size), expected_size);
    glUniformBlockBinding(shader, ub_index, static_cast<GLuint>(binding));
}

static void SetShaderUniformBlockBindings(GLuint shader) {
    SetShaderUniformBlockBinding(shader, "shader_data", UniformBindings::Common,
                                 sizeof(UniformData));
    SetShaderUniformBlockBinding(shader, "vs_config", UniformBindings::VS, sizeof(VSUniformData));
    SetShaderUniformBlockBinding(shader, "gs_config", UniformBindings::GS, sizeof(GSUniformData));
}

static void SetShaderSamplerBinding(GLuint shader, const char* name,
                                    TextureUnits::TextureUnit binding) {
    GLint uniform_tex = glGetUniformLocation(shader, name);
    if (uniform_tex != -1) {
        glUniform1i(uniform_tex, binding.id);
    }
}

static void SetShaderImageBinding(GLuint shader, const char* name, GLuint binding) {
    GLint uniform_tex = glGetUniformLocation(shader, name);
    if (uniform_tex != -1) {
        glUniform1i(uniform_tex, static_cast<GLint>(binding));
    }
}

static void SetShaderSamplerBindings(GLuint shader) {
    OpenGLState cur_state = OpenGLState::GetCurState();
    GLuint old_program = std::exchange(cur_state.draw.shader_program, shader);
    cur_state.Apply();

    // Set the texture samplers to correspond to different texture units
    SetShaderSamplerBinding(shader, "tex0", TextureUnits::PicaTexture(0));
    SetShaderSamplerBinding(shader, "tex1", TextureUnits::PicaTexture(1));
    SetShaderSamplerBinding(shader, "tex2", TextureUnits::PicaTexture(2));
    SetShaderSamplerBinding(shader, "tex_cube", TextureUnits::TextureCube);

    // Set the texture samplers to correspond to different lookup table texture units
    SetShaderSamplerBinding(shader, "texture_buffer_lut_rg", TextureUnits::TextureBufferLUT_RG);
    SetShaderSamplerBinding(shader, "texture_buffer_lut_rgba", TextureUnits::TextureBufferLUT_RGBA);

    SetShaderImageBinding(shader, "shadow_buffer", ImageUnits::ShadowBuffer);
    SetShaderImageBinding(shader, "shadow_texture_px", ImageUnits::ShadowTexturePX);
    SetShaderImageBinding(shader, "shadow_texture_nx", ImageUnits::ShadowTextureNX);
    SetShaderImageBinding(shader, "shadow_texture_py", ImageUnits::ShadowTexturePY);
    SetShaderImageBinding(shader, "shadow_texture_ny", ImageUnits::ShadowTextureNY);
    SetShaderImageBinding(shader, "shadow_texture_pz", ImageUnits::ShadowTexturePZ);
    SetShaderImageBinding(shader, "shadow_texture_nz", ImageUnits::ShadowTextureNZ);

    cur_state.draw.shader_program = old_program;
    cur_state.Apply();
}

void PicaUniformsData::SetFromRegs(const Pica::ShaderRegs& regs,
                                   const Pica::Shader::ShaderSetup& setup) {
    std::transform(std::begin(setup.uniforms.b), std::end(setup.uniforms.b), std::begin(bools),
                   [](bool value) -> BoolAligned { return {value ? GL_TRUE : GL_FALSE}; });
    std::transform(std::begin(regs.int_uniforms), std::end(regs.int_uniforms), std::begin(i),
                   [](const auto& value) -> GLuvec4 {
                       return {value.x.Value(), value.y.Value(), value.z.Value(), value.w.Value()};
                   });
    std::transform(std::begin(setup.uniforms.f), std::end(setup.uniforms.f), std::begin(f),
                   [](const auto& value) -> GLvec4 {
                       return {value.x.ToFloat32(), value.y.ToFloat32(), value.z.ToFloat32(),
                               value.w.ToFloat32()};
                   });
}

class TrivialVertexShader {
public:
    explicit TrivialVertexShader() : shader() {
        shader.Create(GLShader::GenerateTrivialVertexShader().c_str(), GL_VERTEX_SHADER);
    }
    GLuint Get() const {
        return shader.handle;
    }

private:
    OGLShader shader;
};

template <typename KeyConfigType, std::string (*CodeGenerator)(const KeyConfigType&),
          GLenum ShaderType>
class ShaderCache {
public:
    ShaderCache() = default;
    GLuint Get(const KeyConfigType& config) {
        auto [iter, new_shader] = shaders.emplace(config, OGLShader{});
        OGLShader& cached_shader = iter->second;
        if (new_shader) {
            cached_shader.Create(CodeGenerator(config).c_str(), ShaderType);
        }
        return cached_shader.handle;
    }

private:
    std::unordered_map<KeyConfigType, OGLShader> shaders;
};

// This is a cache designed for shaders translated from PICA shaders. The first cache matches the
// config structure like a normal cache does. On cache miss, the second cache matches the generated
// GLSL code. The configuration is like this because there might be leftover code in the PICA shader
// program buffer from the previous shader, which is hashed into the config, resulting several
// different config values from the same shader program.
template <typename KeyConfigType,
          std::optional<std::string> (*CodeGenerator)(const Pica::Shader::ShaderSetup&,
                                                      const KeyConfigType&),
          GLenum ShaderType>
class ShaderDoubleCache {
public:
    ShaderDoubleCache() = default;
    GLuint Get(const KeyConfigType& key, const Pica::Shader::ShaderSetup& setup) {
        auto map_it = shader_map.find(key);
        if (map_it == shader_map.end()) {
            auto program_opt = CodeGenerator(setup, key);
            if (!program_opt) {
                shader_map[key] = nullptr;
                return 0;
            }

            std::string& program = *program_opt;
            auto [iter, new_shader] = shader_cache.emplace(program, OGLShader{});
            OGLShader& cached_shader = iter->second;
            if (new_shader) {
                cached_shader.Create(program.c_str(), ShaderType);
            }
            shader_map[key] = &cached_shader;
            return cached_shader.handle;
        }

        if (map_it->second == nullptr) {
            return 0;
        }

        return map_it->second->handle;
    }

private:
    std::unordered_map<KeyConfigType, OGLShader*> shader_map;
    std::unordered_map<std::string, OGLShader> shader_cache;
};

class SharableCacheReader : public Common::LinearDiskCacheReader<u64, u8> {
    void Read(const u64& key, const u8* value, u32 value_size) override {
        //
    }
};

class BinaryCacheReader : public Common::LinearDiskCacheReader<u64, u8> {
    void Read(const u64& key, const u8* value, u32 value_size) override {
        all_keys.push_back(key);
        if (value_size < sizeof(GLenum)) {
            failed_to_load.push_back(key);
            return;
        }
        GLShader::ProgramBinary config;
        memcpy(&config.format, value, sizeof(GLenum));
        config.data.reserve(value_size - sizeof(GLenum));
        config.data.insert(config.data.begin(), value + sizeof(GLenum), value + value_size);
        OGLProgram program;
        program.Create(config);
        if (program.handle == 0) {
            failed_to_load.push_back(key);
            return;
        }
    }

    const std::vector<u32> GetFailed() const {
        return failed_to_load;
    }

private:
    std::vector<u32> failed_to_load;
    std::vector<u32> all_keys;
};

using ProgrammableVertexShaders =
    ShaderDoubleCache<GLShader::PicaVSConfig, &GLShader::GenerateVertexShader, GL_VERTEX_SHADER>;

using ProgrammableGeometryShaders =
    ShaderDoubleCache<GLShader::PicaGSConfig, &GLShader::GenerateGeometryShader,
                      GL_GEOMETRY_SHADER>;

using FixedGeometryShaders =
    ShaderCache<GLShader::PicaFixedGSConfig, &GLShader::GenerateFixedGeometryShader,
                GL_GEOMETRY_SHADER>;

using FragmentShaders =
    ShaderCache<GLShader::PicaFSConfig, &GLShader::GenerateFragmentShader, GL_FRAGMENT_SHADER>;

class ShaderProgramManager::Impl {
public:
    explicit Impl(bool use_binary_shader_cache_, u64 program_id)
        : use_binary_shader_cache(use_binary_shader_cache_), programmable_vertex_shaders(),
          trivial_vertex_shader(), programmable_geometry_shaders(), fixed_geometry_shaders(),
          fragment_shaders() {

        std::string program{fmt::format("{:016X}", program_id)};
        binary_cache_path = FileUtil::GetUserPath(FileUtil::UserPath::BinaryCacheDir) + program;
        FileUtil::CreateFullPath(binary_cache_path);
        // disk_cache_handler = std::thread([&] {
        if (use_binary_shader_cache) {
            // First load all of the shaders from the disk cache
            binary_cache.OpenAndRead(binary_cache_path, binary_reader);
        }

        // For any shaders that failed to load from the disk cache, recompile what we can find
        // from the sharable cache.
        // sharable.OpenAndRead();
        //});
    }

    ~Impl() {
        is_running = false;
        if (disk_cache_handler.joinable())
            disk_cache_handler.join();
    }

    struct ShaderTuple {
        GLuint vs = 0;
        GLuint gs = 0;
        GLuint fs = 0;

        bool operator==(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) == std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        bool operator!=(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) != std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        struct Hash {
            std::size_t operator()(const ShaderTuple& tuple) const {
                std::size_t hash = 0;
                boost::hash_combine(hash, tuple.vs);
                boost::hash_combine(hash, tuple.gs);
                boost::hash_combine(hash, tuple.fs);
                return hash;
            }
        };
    };

    struct ProgramConfig {
        Pica::Regs regs;
        std::array<u32, Pica::Shader::MAX_PROGRAM_CODE_LENGTH> program_code;
        std::array<u32, Pica::Shader::MAX_SWIZZLE_DATA_LENGTH> swizzle_data;
    };

    ShaderTuple current;
    CompleteShaderConfig current_config;

    ProgrammableVertexShaders programmable_vertex_shaders;
    TrivialVertexShader trivial_vertex_shader;

    ProgrammableGeometryShaders programmable_geometry_shaders;
    FixedGeometryShaders fixed_geometry_shaders;

    FragmentShaders fragment_shaders;

    std::atomic<bool> is_running = true;
    std::mutex cache_mutex;
    std::unordered_map<ShaderTuple, OGLProgram, ShaderTuple::Hash> program_cache;
    OGLPipeline pipeline;

    std::thread disk_cache_handler;
    bool use_binary_shader_cache;

    std::string binary_cache_path;
    Common::LinearDiskCache<u64, u8> binary_cache;
    Common::LinearDiskCache<u64, u8> sharable_cache{false};
    BinaryCacheReader binary_reader;
};

ShaderProgramManager::ShaderProgramManager(bool use_binary_shader_cache, u64 program_id)
    : impl(std::make_unique<Impl>(use_binary_shader_cache, program_id)) {}

ShaderProgramManager::~ShaderProgramManager() = default;

bool ShaderProgramManager::UseProgrammableVertexShader(const GLShader::PicaVSConfig& config,
                                                       const Pica::Shader::ShaderSetup setup) {
    GLuint handle = impl->programmable_vertex_shaders.Get(config, setup);
    if (handle == 0)
        return false;
    impl->current.vs = handle;
    return true;
}

void ShaderProgramManager::UseTrivialVertexShader() {
    impl->current.vs = impl->trivial_vertex_shader.Get();
}

bool ShaderProgramManager::UseProgrammableGeometryShader(const GLShader::PicaGSConfig& config,
                                                         const Pica::Shader::ShaderSetup setup) {
    GLuint handle = impl->programmable_geometry_shaders.Get(config, setup);
    if (handle == 0)
        return false;
    impl->current.gs = handle;
    return true;
}

void ShaderProgramManager::UseFixedGeometryShader(const GLShader::PicaFixedGSConfig& config) {
    impl->current.gs = impl->fixed_geometry_shaders.Get(config);
}

void ShaderProgramManager::UseTrivialGeometryShader() {
    impl->current.gs = 0;
}

void ShaderProgramManager::UseFragmentShader(const GLShader::PicaFSConfig& config) {
    impl->current.fs = impl->fragment_shaders.Get(config);
}

void ShaderProgramManager::UploadCompleteShaderConfig(CompleteShaderConfig config) {
    impl->current_config = config;
}

void ShaderProgramManager::ApplyTo(OpenGLState& state) {
    OGLProgram& cached_program = impl->program_cache[impl->current];
    if (cached_program.handle == 0) {
        cached_program.Create({impl->current.vs, impl->current.gs, impl->current.fs});
        SetShaderUniformBlockBindings(cached_program.handle);
        SetShaderSamplerBindings(cached_program.handle);
        u64 hash = Common::ComputeStructHash64(impl->current_config);
        GLShader::ProgramBinary binary = cached_program.GetProgramBinary();
        // add the format to the front of the data vector so we can just write it all in one go
        u8* as_u8 = reinterpret_cast<u8*>(&binary.format);
        binary.data.insert(binary.data.begin(), as_u8, as_u8 + sizeof(binary.format));
        impl->binary_cache.Append(hash, binary.data.data(), binary.data.size());
    }
    state.draw.shader_program = cached_program.handle;
}
