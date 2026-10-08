// Engine spike M0-06: public interface of the spike_core library.
//
// The tests (spike/engine/tests) and the spike_cli executable call exactly these functions.
// The header deliberately uses only std types so that callers need no JUCE/Tracktion types.
//
// STATUS: the functions in spike_common.cpp are STUBS written by the test-writer. They fail with
// kNotImplemented. The implementer replaces the bodies (and may add more files under src/).
//
// Conventions
//  - Every function reports failure through `ok == false` plus a human readable `error`.
//    Functions never throw to the caller and never abort the process.
//  - Every function may be called many times in one process (the tests do): each call must create and
//    destroy its own engine (no process-wide singleton state, TRACKTION_ENABLE_SINGLETONS=0).
//  - Calls happen on the message thread (the thread that owns the juce::ScopedJuceInitialiser_GUI).
#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace spike
{

/** Error text of every stub. Real failures must NOT use this text. */
inline constexpr const char* kNotImplemented = "not implemented";

//==============================================================================
// Fixed parameters of the spike (shared between implementation and tests)

inline constexpr double kRenderSampleRate = 48000.0;  // render-region output
inline constexpr int kRenderBitsPerSample = 24;       // render-region output
inline constexpr int kRecordNumInputs = 12;           // record-12
inline constexpr double kRecordSampleRate = 48000.0;  // record-12 hosted device
inline constexpr int kRecordBlockSize = 512;          // record-12 hosted device
inline constexpr double kRecordToleranceDb = -99.0;   // record-12 comparison limit

/** load-vst3: the test signal is a sine of this frequency and amplitude (peak), rendered in ONE block. */
inline constexpr double kVst3TestSampleRate = 48000.0;
inline constexpr int kVst3TestBlockFrames = 4800;  // 100 periods of 1 kHz, so the RMS is exact
inline constexpr double kVst3TestFrequencyHz = 1000.0;
inline constexpr double kVst3TestAmplitude = 0.5;  // RMS = -9.0309 dBFS

/** SpikeGain: single parameter with ID "gainDb", range -24..+12 dB, default -6.0 dB, no smoothing
    (the gain applies from the first sample of the first block). */
inline constexpr double kSpikeGainDefaultDb = -6.0;

/** record-12 input signal: channel `c` (0-based) is a sine of 0.25 * sin(2*pi * f * n / 48000), where
    f = recordInputFrequencyHz(c) and n is the frame index since the start of the recording. */
inline constexpr double recordInputFrequencyHz(int channel) noexcept
{
    return 200.0 + 110.0 * static_cast<double>(channel);
}
inline constexpr double kRecordInputAmplitude = 0.25;

//==============================================================================
struct ImportResult
{
    bool ok = false;
    std::string error = kNotImplemented;
    double sampleRate = 0.0;
    int numChannels = 0;
    std::int64_t lengthSamples = 0;
};

/** import <file>: opens a WAV or MP3 through the engine's import path and reports its properties. */
ImportResult importFile(const std::filesystem::path& file);

//==============================================================================
struct Loudness
{
    double integratedLufs = 0.0;  // ITU-R BS.1770-4 / EBU R128, gated
    double truePeakDbtp = 0.0;    // 4x oversampled
    double lra = 0.0;             // EBU Tech 3342, in LU
};

struct RenderResult
{
    bool ok = false;
    std::string error = kNotImplemented;
    std::filesystem::path outFile;
    double sampleRate = 0.0;  // must be kRenderSampleRate
    int numChannels = 0;
    int bitsPerSample = 0;  // must be kRenderBitsPerSample
    std::int64_t lengthSamples = 0;
    Loudness loudness;  // measured on the rendered file
};

/** render-region <file> --start --end --out: offline render of [startSeconds, endSeconds) of the source
    file (positions in seconds of the source) to a 48 kHz / 24 bit WAV, then loudness measurement.
    lengthSamples must be exactly round((end - start) * 48000). The output keeps the source channel count. */
RenderResult renderRegion(const std::filesystem::path& source, double startSeconds, double endSeconds,
                          const std::filesystem::path& outFile);

//==============================================================================
struct Record12Result
{
    bool ok = false;
    std::string error = kNotImplemented;
    int numInputs = 0;                    // must be kRecordNumInputs
    int numTracks = 0;                    // must be kRecordNumInputs
    std::vector<std::filesystem::path> files;  // files[c] = recording of input c, mono, 48 kHz, 24 bit or float
    std::int64_t lengthSamples = 0;       // per file; must equal round(seconds * 48000)
    int missingBlocks = -1;               // blocks fed to the device but not present in the recordings
    double worstDeviationDb = 0.0;        // max over channels/samples of 20*log10(|rec - ref|), ref = input signal
};

/** record-12 --seconds S [--out-dir D]: hosted audio device with 12 inputs (block size kRecordBlockSize),
    12 tracks, each armed on one mono input; feeds the documented input signals for `seconds` and records.
    The recordings are written into outDir (created if missing). The function compares them itself and
    reports the result; the tests additionally re-read the files and verify independently. */
Record12Result record12(double seconds, const std::filesystem::path& outDir);

//==============================================================================
struct Vst3Result
{
    bool ok = false;
    std::string error = kNotImplemented;
    int numScanned = 0;       // plugin descriptions found in the bundle (>= 1 on success)
    std::string pluginName;   // name from the plugin description ("SpikeGain")
    bool rendered = false;    // one block of kVst3TestBlockFrames frames was processed
    double inputRmsDb = 0.0;  // RMS of the test signal fed to the plugin
    double outputRmsDb = 0.0; // RMS of the plugin output
    double gainDb = 0.0;      // outputRmsDb - inputRmsDb
};

/** load-vst3 <bundle> [--gain-db G]: scans the VST3 bundle, loads the first plugin, optionally sets the
    parameter "gainDb", renders one block of the documented sine and reports the measured level change. */
Vst3Result loadVst3(const std::filesystem::path& bundle, std::optional<double> gainDb = std::nullopt);

//==============================================================================
struct DumpPcmResult
{
    bool ok = false;
    std::string error = kNotImplemented;
    double sampleRate = 0.0;
    int numChannels = 0;
    std::int64_t lengthSamples = 0;
};

/** dump-pcm <file> --out <raw>: decodes the file through the same import path as `import` and writes
    interleaved little-endian float32 samples at the native sample rate (used for MP3 platform comparison). */
DumpPcmResult dumpPcm(const std::filesystem::path& file, const std::filesystem::path& rawOut);

//==============================================================================
/** The CLI. `args` excludes the program name. Prints exactly one JSON object followed by '\n' to `out`
    (also on failure: {"ok":false,"error":"..."}); diagnostics go to `err`.

    Exit codes: 0 success, 1 operation failed (file missing, decode error, ...), 2 usage error
    (unknown command, missing/invalid option).

    Commands and JSON keys (all numbers are JSON numbers):
      import <file>
        {"ok":true,"sample_rate":44100,"channels":2,"length_samples":N}
      render-region <file> --start S --end E --out O
        {"ok":true,"out":"...","sample_rate":48000,"channels":2,"bits_per_sample":24,"length_samples":N,
         "integrated_lufs":x,"true_peak_dbtp":x,"lra":x}
      record-12 --seconds S [--out-dir D]
        {"ok":true,"inputs":12,"tracks":12,"files":[12 paths],"length_samples":N,"missing_blocks":0,
         "worst_deviation_db":x}
      load-vst3 <bundle> [--gain-db G]
        {"ok":true,"plugin":"SpikeGain","scanned":1,"rendered":true,"input_rms_db":x,"output_rms_db":x,"gain_db":x}
      dump-pcm <file> --out <raw>
        {"ok":true,"sample_rate":44100,"channels":2,"length_samples":N}
*/
int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace spike
