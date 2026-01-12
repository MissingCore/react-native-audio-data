import { useState } from 'react';
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
import {
  getRawPcmData,
  getWaveformData,
  type WaveformMethod,
} from 'react-native-audio-data';

const WaveformView = ({ data }: { data: number[] }) => {
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
          return (
            <View
              key={index}
              style={[
                styles.bar,
                {
                  height: `${heightPercent}%`,
                  opacity: 0.5 + value * scale * 0.5,
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

  const methods: WaveformMethod[] = ['RMS', 'LUFS', 'AbsMean'];

  const handlePickAndProcess = async () => {
    try {
      setLoading(true);
      setLog('Picking file...');
      setSelectedPath(null);
      setWaveformData([]);

      const targetPoints = parseInt(pointCount, 10) || 50;

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
        `Selected: ${file.name}\nProcessing ${targetPoints} points using ${method}...`
      );
      const points = await getWaveformData(file.uri, targetPoints, method);
      setWaveformData(points);

      const result = await getRawPcmData(file.uri);
      const { buffer, channels, sampleRate, totalPCMFrameCount } = result;

      setLog(
        (prev) =>
          prev +
          `\n\n✅ Success!` +
          `\nRequested Points: ${targetPoints}` +
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

        <WaveformView data={waveformData} />

        <View style={styles.settingsContainer}>
          <View style={styles.settingRow}>
            <Text style={styles.settingLabel}>Target Blocks:</Text>
            <TextInput
              style={styles.input}
              value={pointCount}
              onChangeText={setPointCount}
              keyboardType="numeric"
              maxLength={4}
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
    fontSize: 24,
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
});
