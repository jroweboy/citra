// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/pica.h"
#include "video_core/rasterizer_interface.h"

class RasterizerVulkan : public VideoCore::RasterizerInterface {
public:

    //RasterizerVulkan();
    //~RasterizerVulkan() override;

    void InitObjects() override;
    void Reset() override;
    void AddTriangle(const Pica::Shader::OutputVertex& v0,
        const Pica::Shader::OutputVertex& v1,
        const Pica::Shader::OutputVertex& v2) override;
    void DrawTriangles() override;
    void FlushFramebuffer() override;
    void NotifyPicaRegisterChanged(u32 id) override;
    void FlushRegion(PAddr addr, u32 size) override;
    void InvalidateRegion(PAddr addr, u32 size) override;
    bool SupportsRendererDelegate() override;
    void SetRendererDelegate(void * renderDelegate) override;
private:
};