import { useState, useRef, useEffect } from 'react';
import {
  Text,
  View,
  StyleSheet,
  Button,
  ScrollView,
  SafeAreaView,
  TextInput,
  TouchableOpacity,
} from 'react-native';

import { pick, types } from '@react-native-documents/picker';
import Video from 'react-native-video';
import {
  getRawPcmData,
  getWaveformDataByPoints,
  getWaveformData,
  type WaveformMethod,
} from 'react-native-audio-data';

const WaveformView = ({ data, progress }: { data: number[], progress: number }) => {
  if (!data || data.length === 0) {
    return (
      <View style={[styles.waveformContainer, styles.emptyContainer]}>
        <Text style={styles.emptyText}>Waveform will appear here</Text>
      </View>
    );
  }

  const maxVal = Math.max(...data, 0.00001);
  const scale = 1 / maxVal;

  return (
    <View style={styles.waveformContainer}>
      <Text style={styles.chartTitle}>
        WAVEFORM PREVIEW ({data.length} points)
      </Text>
      <Text style={styles.scrollHint}>↔ Scrollable</Text>
      <ScrollView
        horizontal
        style={styles.scrollView}
        contentContainerStyle={styles.barsContainer}
        showsHorizontalScrollIndicator={true}
      >
        {data.map((value, index) => {
          let heightPercent = value * scale * 100;
          heightPercent = Math.max(heightPercent, 2);

          // Determine if this bar is "played"
          // progress is 0..1. The index corresponding to progress is floor(progress * data.length)
          const isPlayed = index / data.length < progress;

          return (
            <View
              key={index}
              style={[
                styles.bar,
                {
                  height: `${heightPercent}%`,
                  opacity: isPlayed ? 1.0 : (0.5 + value * scale * 0.5),
                  backgroundColor: isPlayed ? '#00ff00' : '#00e5ff',
                },
              ]}
            />
          );
        })}
      </ScrollView>
    </View>
  );
};

const RadioButton = ({
  label,
  selected,
  onSelect,
}: {
  label: string;
  selected: boolean;
  onSelect: () => void;
}) => (
  <TouchableOpacity
    style={[styles.radioButton, selected && styles.radioButtonSelected]}
    onPress={onSelect}
  >
    <Text
      style={[
        styles.radioButtonText,
        selected && styles.radioButtonTextSelected,
      ]}
    >
      {label}
    </Text>
  </TouchableOpacity>
);

export default function App() {
  const [log, setLog] = useState<string>('Waiting for action...');
  const [loading, setLoading] = useState(false);
  const [selectedPath, setSelectedPath] = useState<string | null>(null);
  const [waveformData, setWaveformData] = useState<number[]>([]);

  const [pointCount, setPointCount] = useState<string>('50');
  const [method, setMethod] = useState<WaveformMethod>('RMS');
  const [mode, setMode] = useState<'points' | 'ms'>('points');

  // Video Player State
  const [paused, setPaused] = useState(true);
  const [progress, setProgress] = useState(0);
  const [duration, setDuration] = useState(0);

  // Animation Refs
  const lastUpdate = useRef<{ time: number; timestamp: number } | null>(null);
  const animationFrame = useRef<number | null>(null);

  const methods: WaveformMethod[] = ['RMS', 'LUFS', 'AbsMean'];

  const startAnimationLoop = () => {
    if (animationFrame.current) cancelAnimationFrame(animationFrame.current);

    const loop = () => {
      if (lastUpdate.current && duration > 0) {
        const now = Date.now();
        const elapsed = (now - lastUpdate.current.timestamp) / 1000;
        const estimatedTime = lastUpdate.current.time + elapsed;
        const newProgress = Math.min(estimatedTime / duration, 1.0);
        setProgress(newProgress);
      }
      animationFrame.current = requestAnimationFrame(loop);
    };
    animationFrame.current = requestAnimationFrame(loop);
  };

  const stopAnimationLoop = () => {
    if (animationFrame.current) {
      cancelAnimationFrame(animationFrame.current);
      animationFrame.current = null;
    }
  };

  // Cleanup on unmount
  useEffect(() => {
    return () => stopAnimationLoop();
  }, []);

  const togglePlayback = () => {
    const nextPaused = !paused;
    setPaused((prev) => !prev);

    if (nextPaused) {
      stopAnimationLoop();
    } else {
      // Reset last update if starting from scratch or re-playing
      if (progress >= 1) {
        lastUpdate.current = { time: 0, timestamp: Date.now() };
        setProgress(0);
      }
      startAnimationLoop();
    }
  };

  const handlePickAndProcess = async () => {
    try {
      setLoading(true);
      setLog('Picking file...');
      setSelectedPath(null);
      setWaveformData([]);

      // Reset player state
      setPaused(true);
      setProgress(0);
      setDuration(0);
      stopAnimationLoop();
      lastUpdate.current = null;

      const results = await pick({
        type: [types.audio],
        allowMultiSelection: false,
      });

      const file = results[0];
      if (!file) {
        setLog('No file selected');
        return;
      }

      setSelectedPath(file.uri);

      setLog(
        `Selected: ${file.name}\nProcessing using ${method}...`
      );

      let points: number[] = [];
      const val = parseInt(pointCount, 10) || 50;

      if (mode === 'points') {
        points = await getWaveformDataByPoints(file.uri, val, method);
      } else {
        points = await getWaveformData(file.uri, val, method);
      }

      setWaveformData(points);

      const result = await getRawPcmData(file.uri);
      const { buffer, channels, sampleRate, totalPCMFrameCount } = result;

      setLog(
        (prev) =>
          prev +
          `\n\n✅ Success!` +
          `\nMode: ${mode === 'points' ? 'Target Points' : 'Ms Per Point'}` +
          `\nInput Value: ${val}` +
          `\nMethod: ${method}` +
          `\nActual Points: ${points.length}` +
          `\nBuffer ByteLength: ${buffer.byteLength}` +
          `\nChannels: ${channels}` +
          `\nSampleRate: ${sampleRate}` +
          `\nTotal Frames: ${totalPCMFrameCount}`
      );
    } catch (err) {
      if (
        typeof err === 'object' &&
        err !== null &&
        'code' in err &&
        (err as any).code === 'DOCUMENT_PICKER_CANCELED'
      ) {
        setLog('User cancelled');
      } else {
        console.error(err);
        setLog(`❌ Error: ${err instanceof Error ? err.message : String(err)}`);
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.content}>
        <Text style={styles.header}>React Native Audio Data Demo</Text>

        {selectedPath && (
          <View style={styles.pathContainer}>
            <Text style={styles.pathLabel}>Current File Path:</Text>
            <Text
              style={styles.pathText}
              selectable
              numberOfLines={1}
              ellipsizeMode="middle"
            >
              {selectedPath}
            </Text>
          </View>
        )}

        <WaveformView data={waveformData} progress={progress} />

        {selectedPath && (
          <View style={styles.playerContainer}>
            <Video
              source={{ uri: selectedPath }}
              paused={paused}
              onLoad={(data) => {
                setDuration(data.duration);
                console.log('Video loaded', data.duration);
              }}
              onProgress={(data) => {
                // Sync authoritative time
                lastUpdate.current = { time: data.currentTime, timestamp: Date.now() };

                // If paused, we want exact sync. If playing, the loop handles it.
                if (paused && duration > 0) {
                  setProgress(data.currentTime / duration);
                }
              }}
              onEnd={() => {
                setPaused(true);
                setProgress(1);
                stopAnimationLoop();
                console.log('Video ended');
              }}
              onError={(e) => console.log('Video error', e)}
              audioOnly={true}
              repeat={false}
              ignoreSilentSwitch="ignore"
              progressUpdateInterval={250}
            />
            <Button
              title={paused ? "Play Audio" : "Pause"}
              onPress={togglePlayback}
            />
            <Text style={styles.progressText}>
              {(progress * duration).toFixed(1)}s / {duration.toFixed(1)}s
            </Text>
          </View>
        )}

        <View style={styles.settingsContainer}>
          <View style={styles.settingRow}>
            <Text style={styles.settingLabel}>Mode:</Text>
            <View style={styles.radioGroup}>
              <RadioButton
                label="Points"
                selected={mode === 'points'}
                onSelect={() => setMode('points')}
              />
              <RadioButton
                label="Ms/Point"
                selected={mode === 'ms'}
                onSelect={() => setMode('ms')}
              />
            </View>
          </View>
          <View style={styles.separator} />
          <View style={styles.settingRow}>
            <Text style={styles.settingLabel}>{mode === 'points' ? 'Target Points:' : 'Ms Per Point:'}</Text>
            <TextInput
              style={styles.input}
              value={pointCount}
              onChangeText={setPointCount}
              keyboardType="numeric"
              maxLength={5}
              placeholder="50"
            />
          </View>
          <View style={styles.separator} />
          <View style={styles.settingRow}>
            <Text style={styles.settingLabel}>Method:</Text>
            <View style={styles.radioGroup}>
              {methods.map((m) => (
                <RadioButton
                  key={m}
                  label={m}
                  selected={method === m}
                  onSelect={() => setMethod(m)}
                />
              ))}
            </View>
          </View>
        </View>

        <View style={styles.buttonContainer}>
          <Button
            title={loading ? 'Processing...' : 'Pick Audio & Analyze'}
            onPress={handlePickAndProcess}
            disabled={loading}
          />
        </View>

        <Text style={styles.label}>Log Output:</Text>
        <ScrollView style={styles.logBox}>
          <Text style={styles.logText}>{log}</Text>
        </ScrollView>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  content: {
    flex: 1,
    padding: 20,
    alignItems: 'stretch',
  },
  header: {
    fontSize: 20,
    fontWeight: 'bold',
    textAlign: 'center',
    marginBottom: 20,
    color: '#333',
  },
  pathContainer: {
    backgroundColor: '#e3e3e3',
    padding: 12,
    borderRadius: 8,
    marginBottom: 10,
    borderWidth: 1,
    borderColor: '#d0d0d0',
  },
  pathLabel: {
    fontSize: 10,
    fontWeight: 'bold',
    color: '#666',
    marginBottom: 2,
    textTransform: 'uppercase',
  },
  pathText: {
    fontSize: 12,
    color: '#333',
    fontFamily: 'monospace',
  },
  waveformContainer: {
    height: 120,
    backgroundColor: '#1a1a1a',
    borderRadius: 8,
    paddingVertical: 10,
    marginBottom: 20,
    justifyContent: 'center',
    borderWidth: 1,
    borderColor: '#333',
  },
  scrollView: {
    marginTop: 15,
    flex: 1,
  },
  emptyContainer: {
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#e0e0e0',
    borderStyle: 'dashed',
    borderColor: '#aaa',
  },
  emptyText: {
    color: '#888',
    fontSize: 14,
    fontStyle: 'italic',
  },
  chartTitle: {
    position: 'absolute',
    top: 4,
    left: 8,
    color: '#555',
    fontSize: 9,
    fontWeight: 'bold',
    zIndex: 10,
  },
  scrollHint: {
    position: 'absolute',
    top: 8,
    right: 10,
    color: '#ffffff',
    fontSize: 11,
    fontWeight: 'bold',
    backgroundColor: 'rgba(0,0,0,0.6)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 12,
    overflow: 'hidden',
    zIndex: 10,
  },
  barsContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    minWidth: '100%',
    paddingHorizontal: 10,
  },
  bar: {
    width: 4,
    backgroundColor: '#00e5ff',
    marginHorizontal: 1,
    borderRadius: 2,
    minHeight: 2,
  },
  settingsContainer: {
    marginBottom: 15,
    backgroundColor: '#fff',
    padding: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#ddd',
  },
  settingRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: 5,
  },
  separator: {
    height: 1,
    backgroundColor: '#eee',
    marginVertical: 5,
  },
  settingLabel: {
    fontSize: 16,
    color: '#333',
    fontWeight: '500',
  },
  input: {
    borderWidth: 1,
    borderColor: '#ccc',
    borderRadius: 4,
    paddingHorizontal: 10,
    paddingVertical: 4,
    width: 60,
    textAlign: 'center',
    fontSize: 16,
    color: '#000',
  },
  radioGroup: {
    flexDirection: 'row',
    gap: 8,
  },
  radioButton: {
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 16,
    borderWidth: 1,
    borderColor: '#999',
    backgroundColor: '#f0f0f0',
  },
  radioButtonSelected: {
    backgroundColor: '#007aff',
    borderColor: '#007aff',
  },
  radioButtonText: {
    fontSize: 12,
    color: '#333',
  },
  radioButtonTextSelected: {
    color: '#fff',
    fontWeight: 'bold',
  },
  buttonContainer: {
    marginBottom: 20,
  },
  label: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 10,
    color: '#555',
  },
  logBox: {
    flex: 1,
    backgroundColor: '#1e1e1e',
    padding: 15,
    borderRadius: 8,
  },
  logText: {
    color: '#00ff00',
    fontFamily: 'monospace',
    fontSize: 12,
  },
  playerContainer: {
    backgroundColor: '#fff',
    borderRadius: 8,
    padding: 10,
    marginBottom: 15,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    borderWidth: 1,
    borderColor: '#ddd',
  },
  progressText: {
    fontSize: 14,
    color: '#555',
    fontWeight: 'bold',
  },
});
