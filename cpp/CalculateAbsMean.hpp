#pragma once

#include <vector>
#include <cmath>
#include <cstddef>

namespace margelo::nitro::audiodata {

  inline std::vector<double> calculateAbsMean(const std::vector<float>& data, unsigned int channels, size_t targetPoints) {
      std::vector<double> waveform;
      if (targetPoints == 0) return waveform;
      waveform.reserve(targetPoints);

      size_t totalSamples = data.size();
      size_t totalFrames = totalSamples / channels;

      for (size_t i = 0; i < targetPoints; ++i) {
          size_t startFrame = (i * totalFrames) / targetPoints;
          size_t endFrame = ((i + 1) * totalFrames) / targetPoints;
          if (endFrame > totalFrames) endFrame = totalFrames; 
          if (startFrame >= totalFrames) break;

          double accumulator = 0.0;
          size_t count = 0;

          for (size_t j = startFrame; j < endFrame; ++j) {
              double frameSum = 0.0;
              for (unsigned int c = 0; c < channels; ++c) {
                  frameSum += std::abs(data[j * channels + c]);
              }
              if (channels > 0) {
                  frameSum /= channels;
              }
              accumulator += frameSum;
              count++;
          }

          if (count > 0) {
              waveform.push_back(accumulator / count);
          } else {
              waveform.push_back(0.0);
          }
      }
      return waveform;
  }

}
