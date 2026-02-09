# Changelog

## 1.1.0

### Improvements
- **True Async Decoding**: All audio decoding and waveform calculation methods now run on dedicated background threads in C++, ensuring the main thread remains completely unblocked during heavy processing.

## 1.0.1

- Documentation updates

## 1.0.0

### Features
- **Waveform Algorithms**: Introduced support for multiple waveform calculation methods: `RMS`, `AbsMean`, and `LUFS`.
- **Time-based Waveform Data**: Added `getWaveformData` which accepts `millisecondsPerPoint` instead of a fixed point count, ideal for time-scaled visualizations.
- **Fixed Point Waveform Data**: Renamed the original point-count based method to `getWaveformDataByPoints` for clarity.

### Improvements
- **Example App**: Completely overhauled the example application with a polished UI, including a visualizer that supports the new algorithms and horizontal scrolling.


## v0.1.0 (Initial Release)

We are excited to announce the first release of `react-native-audio-data`! 🚀
This library enables high-performance audio data processing in React Native, powered by C++ and [Nitro Modules](https://nitro.margelo.com/).

### ✨ Key Features

- **High Performance**: Direct C++ execution via JSI for near-native speed.
- **Format Support**: Decodes **MP3**, **WAV**, and **FLAC** audio files.
- **Raw PCM Access**: Get full access to raw `Float32` audio samples for custom DSP or analysis.
- **Waveform Generation**: Efficiently calculate RMS waveform data for visualizing audio (e.g., players, editors).
- **Lightweight**: Built on `dr_libs`—no heavy dependencies like FFmpeg.
- **Cross-Platform**: Full support for **iOS** and **Android**.

### 📦 Installation

```sh
npm install react-native-audio-data react-native-nitro-modules
```

### 🚀 Usage

```typescript
import { getRawPcmData, getWaveformData } from 'react-native-audio-data';

// Get raw PCM data
const pcm = await getRawPcmData('file.mp3');

// Get waveform for visualization
const waveform = await getWaveformData('file.mp3', 100);
```

---

*Thank you for using `react-native-audio-data`!*
