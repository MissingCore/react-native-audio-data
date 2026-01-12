#pragma once

#include <vector>
#include <cmath>
#include <cstddef>
#include <limits>
#include <algorithm>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// True LUFS (BS.1770-style) using K-weighting + gating, then mapped to targetPoints bins.
// Assumptions:
// - `data` is interleaved float PCM in [-1, 1] (but doesn't require hard clamp).
// - `channels` is 1..N
// - `sampleRate` is common audio rates (44.1k, 48k, etc.)
// Output:
// - vector length == targetPoints
// - each element is LUFS for that bin (negative dB), or -inf-ish for silence.
//
// Notes:
// - Uses 400ms blocks with 75% overlap (100ms hop) per EBU R128 gating block definition.
// - Applies absolute gate at -70 LUFS and relative gate at (integrated - 10 LU).
// - For per-bin values, we compute block loudness, then average energies of blocks inside each bin with gating applied.

namespace margelo::nitro::audiodata {

  static inline double safe_log10(double x) {
    // avoid -inf; caller can still treat very small as silence if desired
    return std::log10(std::max(x, 1e-20));
  }

  static inline double power_to_lufs(double meanSquare) {
    // BS.1770 loudness: L = -0.691 + 10*log10(meanSquare)  (for mono after K-weighting)
    // For multi-channel, meanSquare should already include channel weights and sum.
    return -0.691 + 10.0 * safe_log10(meanSquare);
  }

  inline std::vector<double> calculateLUFS(std::vector<float> data,
                                    unsigned int channels,
                                    unsigned int sampleRate,
                                    size_t targetPoints) {
    std::vector<double> out;
    if (targetPoints == 0) return out;
    out.assign(targetPoints, -std::numeric_limits<double>::infinity());

    if (channels == 0 || sampleRate == 0 || data.empty()) {
      return out;
    }

    const size_t totalSamples = data.size();
    const size_t totalFrames = totalSamples / channels;
    if (totalFrames == 0) return out;

    // ----------------------------
    // 1) K-weighting filter (biquad cascade per channel)
    // ----------------------------
    // For production, you likely want coefficient tables for 44.1k and 48k.
    // Here we provide exact 48k coefficients (common) and a widely-used 44.1k set.
    // For other sample rates, we fall back to a bilinear-designed approximation (good enough for UI bars).
    struct Biquad {
      double b0{}, b1{}, b2{}, a1{}, a2{};
    };

    auto make_biquad_48k = []() -> std::pair<Biquad, Biquad> {
      // ITU-R BS.1770-4 K-weighting @ 48k (widely used reference coefficients)
      Biquad shelf{ 1.53512485958697, -2.69169618940638, 1.19839281085285,
                    -1.69065929318241, 0.73248077421585 };
      Biquad hp{ 1.0, -2.0, 1.0,
                -1.99004745483398, 0.99007225036621 };
      return {shelf, hp};
    };

    auto make_biquad_44100 = []() -> std::pair<Biquad, Biquad> {
      // Commonly used BS.1770 K-weighting @ 44.1k coefficient set (biquad cascade)
      // Note: there are multiple published rounded variants; this set is widely used in open implementations.
      Biquad shelf{ 1.53084123005034, -2.65097999515473, 1.16907907992159,
                    -1.66365511325602, 0.71259542807323 };
      Biquad hp{ 1.0, -2.0, 1.0,
                -1.98916967362979, 0.98919903578704 };
      return {shelf, hp};
    };

    auto design_fallback = [&](double fs) -> std::pair<Biquad, Biquad> {
      // Approximate design (not strict BS.1770 for arbitrary fs, but reasonable for UI)
      // Stage1: high-shelf +4dB @ 1500Hz, Q=1/sqrt(2)
      // Stage2: high-pass around 38Hz
      auto design_high_shelf = [&](double f0, double G, double Q) -> Biquad {
        const double A = std::pow(10.0, G/40.0);
        const double w0 = 2.0 * M_PI * f0 / fs;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double cosw0 = std::cos(w0);
        double b0 =    A * ( (A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alpha );
        double b1 = -2*A * ( (A-1) + (A+1)*cosw0 );
        double b2 =    A * ( (A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alpha );
        double a0 =          (A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alpha;
        double a1 =    2 * ( (A-1) - (A+1)*cosw0 );
        double a2 =          (A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alpha;
        b0/=a0; b1/=a0; b2/=a0; a1/=a0; a2/=a0;
        return {b0,b1,b2,a1,a2};
      };

      auto design_high_pass = [&](double f0, double Q) -> Biquad {
        const double w0 = 2.0 * M_PI * f0 / fs;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double cosw0 = std::cos(w0);
        double b0 = (1 + cosw0) / 2;
        double b1 = -(1 + cosw0);
        double b2 = (1 + cosw0) / 2;
        double a0 = 1 + alpha;
        double a1 = -2 * cosw0;
        double a2 = 1 - alpha;
        b0/=a0; b1/=a0; b2/=a0; a1/=a0; a2/=a0;
        return {b0,b1,b2,a1,a2};
      };

      Biquad shelf = design_high_shelf(1500.0, 4.0, 1.0/std::sqrt(2.0));
      Biquad hp    = design_high_pass(38.0, 0.5);
      return {shelf, hp};
    };

    Biquad bq1, bq2;
    if (sampleRate == 48000) {
      auto [s, h] = make_biquad_48k();
      bq1 = s; bq2 = h;
    } else if (sampleRate == 44100) {
      auto [s, h] = make_biquad_44100();
      bq1 = s; bq2 = h;
    } else {
      auto [s, h] = design_fallback((double)sampleRate);
      bq1 = s; bq2 = h;
    }

    // Filter state per channel per biquad: x1,x2,y1,y2
    struct State { double x1=0, x2=0, y1=0, y2=0; };
    std::vector<State> s1(channels), s2(channels);

    auto process_biquad = [](double x, const Biquad& bq, State& st) -> double {
      const double y = bq.b0*x + bq.b1*st.x1 + bq.b2*st.x2 - bq.a1*st.y1 - bq.a2*st.y2;
      st.x2 = st.x1; st.x1 = x;
      st.y2 = st.y1; st.y1 = y;
      return y;
    };

    for (size_t f = 0; f < totalFrames; ++f) {
      for (unsigned int c = 0; c < channels; ++c) {
        const size_t idx = f * channels + c;
        double x = data[idx];
        x = process_biquad(x, bq1, s1[c]);
        x = process_biquad(x, bq2, s2[c]);
        data[idx] = (float)x;
      }
    }

    // ----------------------------
    // 2) Block energies (400ms blocks, 75% overlap => 100ms hop)
    // ----------------------------
    const size_t blockFrames = (size_t)std::llround(0.400 * sampleRate);
    const size_t hopFrames   = (size_t)std::llround(0.100 * sampleRate); // 75% overlap
    if (blockFrames == 0 || hopFrames == 0 || totalFrames < blockFrames) {
      // Not enough audio to form one block: fall back to a single-bin measurement
      // Compute mean square across all frames, with simple channel weighting (mono/stereo)
      // For UI, we'll just compute unweighted sum over channels / channels.
      double sum = 0.0;
      size_t n = 0;
      for (size_t f = 0; f < totalFrames; ++f) {
        double framePower = 0.0;
        for (unsigned int c = 0; c < channels; ++c) {
          const double v = data[f*channels + c];
          framePower += v*v;
        }
        framePower /= (double)channels;
        sum += framePower;
        ++n;
      }
      double ms = (n>0) ? (sum / (double)n) : 0.0;
      double lufs = power_to_lufs(ms);
      std::fill(out.begin(), out.end(), lufs);
      return out;
    }

    // Channel weights: for typical PCM stereo/mono, weights are 1.0.
    // (BS.1770 defines weights for surround channels; you can extend if needed.)
    std::vector<double> chW(channels, 1.0);

    struct BlockInfo {
      size_t startFrame;
      double meanSquare; // weighted sum mean square over block
      double lufs;       // per-block loudness before gating
    };
    std::vector<BlockInfo> blocks;

    for (size_t start = 0; start + blockFrames <= totalFrames; start += hopFrames) {
      double sumSquaresWeighted = 0.0;

      for (size_t f = start; f < start + blockFrames; ++f) {
        for (unsigned int c = 0; c < channels; ++c) {
          const double v = data[f*channels + c];
          sumSquaresWeighted += chW[c] * v * v;
        }
      }

      // mean square across time (and implicitly across channels via summation; weights are applied)
      const double ms = sumSquaresWeighted / (double)blockFrames;
      const double lufs = power_to_lufs(ms);
      blocks.push_back({start, ms, lufs});
    }

    if (blocks.empty()) return out;

    // ----------------------------
    // 3) Gating for integrated loudness threshold
    // ----------------------------
    // Absolute gate: -70 LUFS
    const double absGateLUFS = -70.0;

    // Compute preliminary integrated loudness using only blocks above absolute gate.
    std::vector<double> ms_abs;
    ms_abs.reserve(blocks.size());
    for (const auto& b : blocks) {
      if (b.lufs > absGateLUFS) ms_abs.push_back(b.meanSquare);
    }

    if (ms_abs.empty()) {
      // Everything is below absolute gate => silence
      return out;
    }

    auto mean_of = [](const std::vector<double>& v) -> double {
      double s = 0.0;
      for (double x : v) s += x;
      return s / (double)v.size();
    };

    const double ms_pre = mean_of(ms_abs);
    const double lufs_pre = power_to_lufs(ms_pre);

    // Relative gate: (preliminary integrated - 10 LU)
    const double relGateLUFS = lufs_pre - 10.0;

    // Final gated set
    std::vector<double> ms_gated;
    ms_gated.reserve(blocks.size());
    for (const auto& b : blocks) {
      if (b.lufs > absGateLUFS && b.lufs > relGateLUFS) {
        ms_gated.push_back(b.meanSquare);
      }
    }

    if (ms_gated.empty()) {
      // Extremely dynamic material can end up here; fall back to abs-gated
      ms_gated = std::move(ms_abs);
    }

    // We'll use the gated rule to decide whether a block is "counted" for per-bin aggregation too.
    auto block_is_gated_in = [&](const BlockInfo& b) -> bool {
      return (b.lufs > absGateLUFS && b.lufs > relGateLUFS);
    };

    // ----------------------------
    // 4) Map blocks to targetPoints bins and compute per-bin LUFS
    // ----------------------------
    // We aggregate in energy domain then convert to LUFS: stable and correct-ish.
    // Bin boundaries in frames:
    for (size_t i = 0; i < targetPoints; ++i) {
      const size_t binStart = (i * totalFrames) / targetPoints;
      const size_t binEnd   = ((i + 1) * totalFrames) / targetPoints;

      double sumMs = 0.0;
      size_t cnt = 0;

      // A block belongs to a bin if its center frame is within [binStart, binEnd)
      // (using center reduces boundary artifacts).
      for (const auto& b : blocks) {
        const size_t center = b.startFrame + blockFrames/2;
        if (center < binStart) continue;
        if (center >= binEnd) break; // blocks are in chronological order

        if (block_is_gated_in(b)) {
          sumMs += b.meanSquare;
          ++cnt;
        }
      }

      if (cnt == 0) {
        // If no gated blocks fall into bin, we can either:
        // - return -inf (silence) OR
        // - fall back to ungated blocks inside the bin
        // For UI bars, fallback is often nicer:
        double sumMs2 = 0.0;
        size_t cnt2 = 0;
        for (const auto& b : blocks) {
          const size_t center = b.startFrame + blockFrames/2;
          if (center < binStart) continue;
          if (center >= binEnd) break;
          if (b.lufs > absGateLUFS) {
            sumMs2 += b.meanSquare;
            ++cnt2;
          }
        }
        if (cnt2 == 0) {
          out[i] = -std::numeric_limits<double>::infinity();
        } else {
          out[i] = power_to_lufs(sumMs2 / (double)cnt2);
        }
      } else {
        out[i] = power_to_lufs(sumMs / (double)cnt);
      }
    }

    // Convert dB to linear amplitude (0..1) to match other methods
    for (double& val : out) {
      if (val == -std::numeric_limits<double>::infinity()) {
        val = 0.0;
      } else {
        val = std::pow(10.0, val / 20.0);
      }
    }

    return out;
  }
}
