// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include "audio_core/sink.h"

namespace AudioCore {

class SDL2Sink final : public Sink {
public:
    SDL2Sink();
    ~SDL2Sink() override;

    unsigned int GetNativeSampleRate() const override;

    void EnqueueSamples(const s16* samples, size_t sample_count) override;

    size_t SamplesInQueue() const override;
	
    std::vector<std::string>* GetDeviceMap() override;
    void SetDevice(int _device_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    int device_id;
    std::vector<std::string> device_map;
};

} // namespace AudioCore
