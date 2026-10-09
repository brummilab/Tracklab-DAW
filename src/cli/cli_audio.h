// Audio side of the CLI (M1-07): offline render of a project and loudness measurement of an audio file.
// Both throw CliFailure (exit code 1) on failure.
#pragma once

#include "core/command.h"

#include <juce_core/juce_core.h>
#include <tracktion_engine/tracktion_engine.h>

namespace tracklab::cli
{

/** Which measurements `measureLoudness` reports. */
struct Measurements
{
    bool loudness = false;
    bool truePeak = false;
    bool lra = false;
};

/** Renders the whole project (from 0 to the end of its last clip, no tail, no dither) to `destination` as a 48 kHz /
    24 bit stereo WAV. The file appears only if the render succeeded: it is rendered next to the destination under a
    temporary name and then moved. A project without any clip: failure "empty_project", nothing is written.
    Result: {"out","format","sample_rate","channels","bits_per_sample","length_samples"} of the file as written. */
core::Json renderProject(tracktion::Engine& engine, tracktion::Edit& edit, const juce::File& destination);

/** Measures `file` with Tracktion's LoudnessMeter (BS.1770-4 K-weighting and gating, EBU Tech 3342 LRA, 4x oversampled
    true peak), as the engine spike does. Result: only the requested keys of {"integrated_lufs","true_peak_dbtp","lra"}.
    A value the meter cannot give (e.g. the loudness of silence is -infinity) becomes JSON null. */
core::Json measureLoudness(tracktion::Engine& engine, const juce::File& file, const Measurements& wanted);

}  // namespace tracklab::cli
