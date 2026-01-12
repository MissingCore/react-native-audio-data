#include "HybridAudioData.hpp"
#include <iostream>
#include <cctype>
#include <cmath>
#include <algorithm>

#include "dr_wav.h"
#include "dr_mp3.h"
#include "dr_flac.h"

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

    } catch (const std::exception& e) {
      promise->reject(std::make_exception_ptr(e));
    }
    
    return promise;
  }


  // Helper functions for waveform calculation
  // Helper functions for waveform calculation
  // Helper functions for waveform calculation
  std::vector<double> calculateRMS(const std::vector<float>& data, unsigned int channels, size_t targetPoints) {
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

          double sumFramePowers = 0.0;
          size_t count = 0;

          for (size_t j = startFrame; j < endFrame; ++j) {
              double framePower = 0.0;
              for (unsigned int c = 0; c < channels; ++c) {
                  float sample = data[j * channels + c];
                  framePower += sample * sample;
              }
              // Mean power of the frame across channels
              if (channels > 0) {
                  framePower /= channels;
              }
              sumFramePowers += framePower;
              count++;
          }

          if (count > 0) {
              double rms = std::sqrt(sumFramePowers / count);
              waveform.push_back(rms);
          } else {
              waveform.push_back(0.0);
          }
      }
      return waveform;
  }

  std::vector<double> calculateAbsMean(const std::vector<float>& data, unsigned int channels, size_t targetPoints) {
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

  std::vector<double> calculateLUFS(std::vector<float> data, unsigned int channels, unsigned int sampleRate, size_t targetPoints) {
        // Apply K-weighting in-place
        double fs = (double)sampleRate;

        // Filter coefficients
        double b0_1, b1_1, b2_1, a1_1, a2_1; // Pre-filter (Stage 1)
        double b0_2, b1_2, b2_2, a1_2, a2_2; // RLB (Stage 2)

        if (sampleRate == 48000) {
            // Use exact ITU-R BS.1770-4 coefficients for 48kHz
            // Stage 1 (High Shelf)
            b0_1 = 1.53512485958697;
            b1_1 = -2.69169618940638;
            b2_1 = 1.19839281085285;
            a1_1 = -1.69065929318241;
            a2_1 = 0.73248077421585;
            
            // Stage 2 (RLB High Pass)
            b0_2 = 1.0;
            b1_2 = -2.0;
            b2_2 = 1.0;
            a1_2 = -1.99004745483398;
            a2_2 = 0.99007225036621;
        } else {
            // Derive coefficients for other sample rates
            // Filter 1 (Pre-filter): High Shelf, +4dB at 1500Hz
            double G = 4.0;
            double f0 = 1500.0;
            double Q = 1.0/std::sqrt(2.0);
            double A = std::pow(10.0, G/40.0);
            double w0 = 2.0 * M_PI * f0 / fs;
            double alpha = std::sin(w0) / (2.0 * Q);
            double cos_w0 = std::cos(w0);
            
            double a0_1;
            b0_1 =    A * ( (A+1) + (A-1)*cos_w0 + 2*std::sqrt(A)*alpha );
            b1_1 = -2*A * ( (A-1) + (A+1)*cos_w0 );
            b2_1 =    A * ( (A+1) + (A-1)*cos_w0 - 2*std::sqrt(A)*alpha );
            a0_1 =          (A+1) - (A-1)*cos_w0 + 2*std::sqrt(A)*alpha;
            a1_1 =    2 * ( (A-1) - (A+1)*cos_w0 );
            a2_1 =          (A+1) - (A-1)*cos_w0 - 2*std::sqrt(A)*alpha;
            
            // Normalize
            b0_1 /= a0_1; b1_1 /= a0_1; b2_1 /= a0_1; a1_1 /= a0_1; a2_1 /= a0_1;
            
            // Filter 2 (RLB): High Pass (~38Hz)
            f0 = 38.0; 
            Q = 0.5; 
            w0 = 2.0 * M_PI * f0 / fs;
            alpha = std::sin(w0) / (2.0 * Q);
            cos_w0 = std::cos(w0);
            double a0;

            // Standard HP Biquad
            b0_2 = (1 + cos_w0) / 2;
            b1_2 = -(1 + cos_w0);
            b2_2 = (1 + cos_w0) / 2;
            a0   = 1 + alpha;
            a1_2 = -2 * cos_w0;
            a2_2 = 1 - alpha;
            
            // Normalize
            b0_2 /= a0; b1_2 /= a0; b2_2 /= a0; a1_2 /= a0; a2_2 /= a0;
        }
        
        size_t totalSamples = data.size();
        size_t numChannels = channels;

        // Apply filters
        std::vector<double> x1_1(numChannels, 0), x2_1(numChannels, 0), y1_1(numChannels, 0), y2_1(numChannels, 0);
        std::vector<double> x1_2(numChannels, 0), x2_2(numChannels, 0), y1_2(numChannels, 0), y2_2(numChannels, 0);

         for (size_t i = 0; i < totalSamples; i += numChannels) {
            for (size_t c = 0; c < numChannels; ++c) {
                if (i + c >= totalSamples) break;
                double x = data[i + c];
                
                // Stage 1
                double out1 = b0_1*x + b1_1*x1_1[c] + b2_1*x2_1[c] - a1_1*y1_1[c] - a2_1*y2_1[c];
                x2_1[c] = x1_1[c]; x1_1[c] = x;
                y2_1[c] = y1_1[c]; y1_1[c] = out1;
                
                // Stage 2
                double out2 = b0_2*out1 + b1_2*x1_2[c] + b2_2*x2_2[c] - a1_2*y1_2[c] - a2_2*y2_2[c];
                x2_2[c] = x1_2[c]; x1_2[c] = out1;
                y2_2[c] = y1_2[c]; y1_2[c] = out2;
                
                data[i+c] = (float)out2; 
            }
        }
        
        // After filtering, calculate RMS
        // LUFS includes a -0.691 dB offset (attenuation).
        // -0.691 dB in linear amplitude is 10^(-0.0691/20) ~ 0.9235
        // Wait, 10log10(x^2) -> 20log10(x).
        // 10*log10(ratio) = -0.691. ratio = 10^(-0.0691) = 0.8529 (Energy ratio)
        // Amplitude ratio = sqrt(0.8529) = 0.9235.
        
        std::vector<double> rmsWaveform = calculateRMS(data, channels, targetPoints);
        double lufsOffsetScale = 0.9235;

        for (size_t i = 0; i < rmsWaveform.size(); ++i) {
            rmsWaveform[i] *= lufsOffsetScale;
        }

        return rmsWaveform;
  }


  std::shared_ptr<Promise<std::vector<double>>> HybridAudioData::getWaveformData(const std::string& path, double targetPoints, std::optional<WaveformMethod> method) {
    auto promise = Promise<std::vector<double>>::create();

    try {
        auto audio = loadAudioData(path);
        
        std::vector<double> waveform;
        waveform.reserve(targetPoints);

        // Default to RMS if not specified
        WaveformMethod actualMethod = method.value_or(WaveformMethod::RMS);

        switch (actualMethod) {
            case WaveformMethod::ABSMEAN:
                waveform = calculateAbsMean(audio.data, audio.channels, targetPoints);
                break;
            case WaveformMethod::LUFS:
                waveform = calculateLUFS(audio.data, audio.channels, audio.sampleRate, targetPoints);
                break;
            case WaveformMethod::RMS:
            default:
                waveform = calculateRMS(audio.data, audio.channels, targetPoints);
                break;
        }
        
        promise->resolve(waveform);

    } catch (const std::exception& e) {
        promise->reject(std::make_exception_ptr(e));
    }

    return promise;
  }
}