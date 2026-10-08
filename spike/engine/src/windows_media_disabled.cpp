// Windows only: placeholder definitions for juce::WindowsMediaAudioFormat.
//
// The spike builds with JUCE_USE_WINDOWS_MEDIA_FORMAT=0 so that MP3 is decoded by JUCE's own MP3AudioFormat on
// every platform (identical samples on Windows and Linux). JUCE then compiles no WindowsMediaAudioFormat code,
// but its header still declares the class on Windows, and Tracktion's AudioFileFormatManager registers it under
// `#elif JUCE_WINDOWS` without checking the flag. Without these definitions the link would fail.
//
// The placeholder handles no file extension, so the format manager never picks it, and it opens nothing.
// [VERIFIZIEREN] Only the Windows CI can confirm that the link error exists and that this resolves it.
#include <juce_audio_formats/juce_audio_formats.h>

#if JUCE_WINDOWS && !JUCE_USE_WINDOWS_MEDIA_FORMAT

namespace juce
{

WindowsMediaAudioFormat::WindowsMediaAudioFormat() : AudioFormat("Windows Media (disabled)", StringArray()) {}

WindowsMediaAudioFormat::~WindowsMediaAudioFormat() = default;

Array<int> WindowsMediaAudioFormat::getPossibleSampleRates()
{
    return {};
}

Array<int> WindowsMediaAudioFormat::getPossibleBitDepths()
{
    return {};
}

bool WindowsMediaAudioFormat::canDoStereo()
{
    return false;
}

bool WindowsMediaAudioFormat::canDoMono()
{
    return false;
}

bool WindowsMediaAudioFormat::isCompressed()
{
    return true;
}

AudioFormatReader* WindowsMediaAudioFormat::createReaderFor(InputStream* sourceStream, bool deleteStreamIfOpeningFails)
{
    if (deleteStreamIfOpeningFails)
        delete sourceStream;
    return nullptr;
}

std::unique_ptr<AudioFormatWriter> WindowsMediaAudioFormat::createWriterFor(std::unique_ptr<OutputStream>&,
                                                                            const AudioFormatWriterOptions&)
{
    return nullptr;
}

}  // namespace juce

#endif
