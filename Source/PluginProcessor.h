#pragma once

#include <JuceHeader.h>

class HoneyBadgerHolyGrailAudioProcessor final : public juce::AudioProcessor
{
public:
    struct Fingerprint
    {
        bool captured = false;
        bool approved = false;
        juce::String mission;
        juce::String genre;
        juce::String instrument;
        juce::String style;
        juce::String intensity;
        float peakDb = -100.0f;
        float rmsDb = -100.0f;
        float crestDb = 0.0f;
        float brightness = 0.0f;
        float transient = 0.0f;
        float width = 0.0f;
        float lowBuild = 0.0f;
        float midDensity = 0.0f;
        float harshness = 0.0f;
        juce::String verdict;
    };

    struct ToneRackState
    {
        bool active = false;
        float amount = 0.0f;
        float highPassHz = 20.0f;
        float lowShelfDb = 0.0f;
        float mudDb = 0.0f;
        float presenceDb = 0.0f;
        float airDb = 0.0f;
        float driveDb = 0.0f;
        float outputDb = 0.0f;
    };

    HoneyBadgerHolyGrailAudioProcessor();
    ~HoneyBadgerHolyGrailAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    void setActiveSlot(int slotIndex);
    int getActiveSlot() const noexcept { return activeSlot.load(); }
    Fingerprint getSlot(int slotIndex) const;
    Fingerprint getLatestFingerprint() const;
    juce::String getLastMessage() const;
    juce::String getToneRackReadout() const;
    juce::String captureSlot(bool recheck);
    juce::String approveSlot();
    juce::String analyzeStack(const std::array<bool, 8>& selectedSlots);

    static juce::StringArray genres();
    static juce::StringArray missions();
    static juce::StringArray instruments();
    static juce::StringArray styles();
    static juce::StringArray intensities();

private:
    struct RunningStats
    {
        double sumSquares = 0.0;
        double sumAbs = 0.0;
        double sumDerivative = 0.0;
        double sumLow = 0.0;
        double sumMid = 0.0;
        double sumHigh = 0.0;
        double sumSide = 0.0;
        double sumMidStereo = 0.0;
        float peak = 0.0f;
        float previousMono = 0.0f;
        int samples = 0;

        void reset()
        {
            sumSquares = 0.0;
            sumAbs = 0.0;
            sumDerivative = 0.0;
            sumLow = 0.0;
            sumMid = 0.0;
            sumHigh = 0.0;
            sumSide = 0.0;
            sumMidStereo = 0.0;
            peak = 0.0f;
            samples = 0;
        }
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float readParam(juce::AudioProcessorValueTreeState& state, const char* id);
    static juce::String classify(float value, float low, float high, const juce::String& lowText,
                                 const juce::String& goodText, const juce::String& highText);

    Fingerprint makeFingerprintFromStats(const RunningStats& stats) const;
    juce::String makeSlotAdvice(const Fingerprint& now, const Fingerprint* previous, bool recheck) const;
    juce::String makeStackAdvice(const Fingerprint& stack, const std::vector<Fingerprint>& selected) const;
    ToneRackState makeToneRackTarget(const Fingerprint& input) const;
    void applyToneRack(juce::AudioBuffer<float>& buffer, const ToneRackState& target);
    juce::String currentGenre() const;
    juce::String currentMission() const;
    juce::String currentInstrument() const;
    juce::String currentStyle() const;
    juce::String currentIntensity() const;
    void writeSlotToXml(juce::XmlElement& parent, int slotIndex) const;
    void readSlotFromXml(const juce::XmlElement& element);

    juce::AudioProcessorValueTreeState parameters;

    mutable std::mutex dataMutex;
    RunningStats audioStats;
    RunningStats latestSnapshot;
    std::array<Fingerprint, 8> slots;
    juce::String lastMessage = "Pick a mission, solo the sound or send, and hit Destroy It. Buttmeister is awake.";
    juce::String lastToneRackReadout = "Tone Rack bypassed.";
    std::atomic<int> activeSlot { 0 };
    double sampleRateHz = 44100.0;
    float lowFollower = 0.0f;
    float midFollower = 0.0f;
    float highFollower = 0.0f;
    int blocksSinceSnapshot = 0;
    ToneRackState toneRackSmoothed;
    std::array<juce::IIRFilter, 2> toneHighPass;
    std::array<juce::IIRFilter, 2> toneLowShelf;
    std::array<juce::IIRFilter, 2> toneMudPeak;
    std::array<juce::IIRFilter, 2> tonePresencePeak;
    std::array<juce::IIRFilter, 2> toneAirShelf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoneyBadgerHolyGrailAudioProcessor)
};
