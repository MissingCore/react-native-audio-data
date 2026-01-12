# Changelog

## 1.0.0 (2026-01-12)

### Features
- **Waveform Algorithms**: Introduced support for multiple waveform calculation methods: `RMS`, `AbsMean`, and `LUFS`.
- **Time-based Waveform Data**: Added `getWaveformData` which accepts `millisecondsPerPoint` instead of a fixed point count, ideal for time-scaled visualizations.
- **Fixed Point Waveform Data**: Renamed the original point-count based method to `getWaveformDataByPoints` for clarity.

### Improvements
- **Example App**: Completely overhauled the example application with a polished UI, including a visualizer that supports the new algorithms and horizontal scrolling.
