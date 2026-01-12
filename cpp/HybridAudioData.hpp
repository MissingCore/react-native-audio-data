#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>

#include <NitroModules/Promise.hpp>
#include <NitroModules/ArrayBuffer.hpp>

#include "HybridAudioDataSpec.hpp"
#include "WaveformMethod.hpp"

namespace margelo::nitro::audiodata {

  class HybridAudioData : public HybridAudioDataSpec {
  public:
    HybridAudioData() : HybridObject(TAG) {}
    virtual ~HybridAudioData() {}

    std::shared_ptr<Promise<AudioDataResult>> getRawPcmData(const std::string& path) override;

    std::shared_ptr<Promise<std::vector<double>>> getWaveformDataByPoints(const std::string& path, double targetPoints, std::optional<WaveformMethod> method) override;
  };

}