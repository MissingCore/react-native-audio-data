#include "HybridAudioData.hpp"
#include <iostream>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <thread>

#include "dr_wav.h"
#include "dr_mp3.h"
#include "dr_flac.h"
#include "CalculateAbsMean.hpp"
#include "CalculateRMS.hpp"
#include "CalculateLUFS.hpp"

namespace margelo::nitro::audiodata {

  inline bool isFormat(const std::string& path, const std::string& format) {
    auto len = path.size();
    auto fLen = format.size();
    if (len < fLen + 1) return false;
    for (size_t i = len - fLen, j = 0; i < len; ++i, ++j) {
      auto a = static_cast<unsigned char>(path[i]);
      auto b = static_cast<unsigned char>(format[j]);
      if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
  }

  struct AudioDataStruct {
    std::vector<float> data;
    unsigned int channels;
    unsigned int sampleRate;
    uint64_t totalPCMFrameCount;
  };

  AudioDataStruct loadAudioData(const std::string& path) {
    float* pSampleData = nullptr;
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    uint64_t totalPCMFrameCount = 0;

    if (isFormat(path, ".wav")) {
      drwav_uint64 frames;
      pSampleData = drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sampleRate, &frames, NULL);
      totalPCMFrameCount = frames;
    } 
    else if (isFormat(path, ".mp3")) {
      drmp3_config config;
      drmp3_uint64 frames;
      pSampleData = drmp3_open_file_and_read_pcm_frames_f32(path.c_str(), &config, &frames, NULL);
      channels = config.channels;
      sampleRate = config.sampleRate;
      totalPCMFrameCount = frames;
    } 
    else if (isFormat(path, ".flac")) {
      drflac_uint64 frames;
      pSampleData = drflac_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sampleRate, &frames, NULL);
      totalPCMFrameCount = frames;
    } 
    else {
      throw std::invalid_argument("Unsupported audio format");
    }

    if (pSampleData == NULL) {
      throw std::runtime_error("Failed to decode audio data");
    }

    size_t totalSamples = totalPCMFrameCount * channels;
    std::vector<float> data(pSampleData, pSampleData + totalSamples);

    // Free memory using the correct function
    if (isFormat(path, ".wav")) {
      drwav_free(pSampleData, NULL);
    } else if (isFormat(path, ".mp3")) {
      drmp3_free(pSampleData, NULL);
    } else if (isFormat(path, ".flac")) {
      drflac_free(pSampleData, NULL);
    }

    return { data, channels, sampleRate, totalPCMFrameCount };
  }

  std::shared_ptr<Promise<AudioDataResult>> HybridAudioData::getRawPcmData(const std::string& path) {
    auto promise = Promise<AudioDataResult>::create();

    std::thread t([promise, path] {
      try {
        auto audio = loadAudioData(path);

        size_t byteSize = audio.data.size() * sizeof(float);
        const uint8_t* uInt8Data = reinterpret_cast<const uint8_t*>(audio.data.data());
        auto buffer = ArrayBuffer::copy(uInt8Data, byteSize);
        
        AudioDataResult result;
        result.buffer = buffer;
        result.channels = static_cast<double>(audio.channels);
        result.sampleRate = static_cast<double>(audio.sampleRate);
        result.totalPCMFrameCount = static_cast<double>(audio.totalPCMFrameCount);
        
        promise->resolve(result);
      }
      catch (const std::exception& e) {
        promise->reject(std::make_exception_ptr(e));
      }
    });
 
    t.detach();
    
    return promise;
  }

  std::vector<double> calculateWaveform(const AudioDataStruct& audio, double targetPoints, WaveformMethod method) {
    switch (method) {
      case WaveformMethod::ABSMEAN:
        return calculateAbsMean(audio.data, audio.channels, targetPoints);
      case WaveformMethod::LUFS:
        return calculateLUFS(audio.data, audio.channels, audio.sampleRate, targetPoints);
      case WaveformMethod::RMS:
      default:
        return calculateRMS(audio.data, audio.channels, targetPoints);
    }
  }

  std::shared_ptr<Promise<std::vector<double>>> HybridAudioData::getWaveformDataByPoints(const std::string& path, double targetPoints, std::optional<WaveformMethod> method) {
    auto promise = Promise<std::vector<double>>::create();

    std::thread t([promise, path, targetPoints, method] {
      try {
        auto audio = loadAudioData(path);
        
        // Default to RMS if not specified
        WaveformMethod actualMethod = method.value_or(WaveformMethod::RMS);
        auto waveform = calculateWaveform(audio, targetPoints, actualMethod);
        
        promise->resolve(waveform);
      } catch (const std::exception& e) {
        promise->reject(std::make_exception_ptr(e));
      } catch (...) {
        promise->reject(std::make_exception_ptr(std::runtime_error("Unknown error in getWaveformDataByPoints")));
      }
    });

    t.detach();
    
    return promise;
  }

  std::shared_ptr<Promise<std::vector<double>>> HybridAudioData::getWaveformData(const std::string& path, double millisecondsPerPoint, std::optional<WaveformMethod> method) {
    auto promise = Promise<std::vector<double>>::create();

    std::thread t([promise, path, millisecondsPerPoint, method] {
      try {
        auto audio = loadAudioData(path);
        
        if (millisecondsPerPoint <= 0) {
           throw std::invalid_argument("millisecondsPerPoint must be greater than 0");
        }

        // Calculate targetPoints based on millisecondsPerPoint
        // Total Duration (ms) = (totalPCMFrameCount / sampleRate) * 1000
        // Target Points = Total Duration / millisecondsPerPoint
        double durationMs = (static_cast<double>(audio.totalPCMFrameCount) / audio.sampleRate) * 1000.0;
        double targetPoints = durationMs / millisecondsPerPoint;

        // Ensure at least 1 point if file is short but valid
        if (targetPoints < 1.0 && audio.totalPCMFrameCount > 0) {
            targetPoints = 1.0;
        }

        // Default to RMS if not specified
        WaveformMethod actualMethod = method.value_or(WaveformMethod::RMS);
        auto waveform = calculateWaveform(audio, targetPoints, actualMethod);
        
        promise->resolve(waveform);

      } catch (const std::exception& e) {
        promise->reject(std::make_exception_ptr(e));
      } catch (...) {
        promise->reject(std::make_exception_ptr(std::runtime_error("Unknown error in getWaveformData")));
      }
    });

    t.detach();

    return promise;
  }
}