#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float silenceDb = -96.0f;

float gainToDb(float value)
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 0.000001f), silenceDb);
}

float paramOrDefault(juce::AudioProcessorValueTreeState& state, const char* id, float fallback)
{
    if (auto* value = state.getRawParameterValue(id))
        return value->load();
    return fallback;
}

float toneIntensityScale(const juce::String& intensity)
{
    if (intensity.equalsIgnoreCase("Subtle"))
        return 0.8f;
    if (intensity.equalsIgnoreCase("Extreme"))
        return 2.35f;
    return 1.35f;
}

float lerpTone(float current, float target)
{
    return current + (target - current) * 0.12f;
}
}

HoneyBadgerHolyGrailAudioProcessor::HoneyBadgerHolyGrailAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::StringArray HoneyBadgerHolyGrailAudioProcessor::genres()
{
    return { "Trap", "Drill", "EDM", "Dubstep", "House", "Techno", "Pop", "R&B", "Hip Hop", "Cinematic" };
}

juce::StringArray HoneyBadgerHolyGrailAudioProcessor::missions()
{
    return { "Full Track Auto EQ", "Voice Forward", "Reverb Send Cleaner", "Delay Send Cleaner", "Sub/808 Small Speaker" };
}

juce::StringArray HoneyBadgerHolyGrailAudioProcessor::instruments()
{
    return { "Kick", "808", "Bass", "Lead", "Pad", "Pluck", "Snare", "Clap", "Hats", "Vocal", "Rap Vocal", "Reverb Send", "Delay Send", "Piano", "Guitar", "Strings", "FX" };
}

juce::StringArray HoneyBadgerHolyGrailAudioProcessor::styles()
{
    return { "Raw Dry", "Dark", "Bright", "Aggressive", "Clean", "Gritty", "Punchy", "Wide", "Modern", "Vintage" };
}

juce::StringArray HoneyBadgerHolyGrailAudioProcessor::intensities()
{
    return { "Subtle", "Balanced", "Extreme" };
}

juce::AudioProcessorValueTreeState::ParameterLayout HoneyBadgerHolyGrailAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>("mission", "Mission", missions(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("genre", "Genre", genres(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("instrument", "Instrument", instruments(), 3));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("style", "Style", styles(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("intensity", "Intensity", intensities(), 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoTone", "Destroyer Rack", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("toneAmount", "Destroyer Amount",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputTrim", "Output Trim",
        juce::NormalisableRange<float>(-12.0f, 6.0f, 0.1f), 0.0f));
    return { params.begin(), params.end() };
}

void HoneyBadgerHolyGrailAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampleRateHz = sampleRate;
    lowFollower = 0.0f;
    midFollower = 0.0f;
    highFollower = 0.0f;
    blocksSinceSnapshot = 0;
    toneRackSmoothed = {};

    for (auto* bank : { &toneHighPass, &toneLowShelf, &toneMudPeak, &tonePresencePeak, &toneAirShelf })
        for (auto& filter : *bank)
            filter.reset();

    std::lock_guard<std::mutex> lock(dataMutex);
    audioStats.reset();
    latestSnapshot.reset();
}

bool HoneyBadgerHolyGrailAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void HoneyBadgerHolyGrailAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(2, buffer.getNumChannels());

    RunningStats block;
    block.previousMono = audioStats.previousMono;

    const auto lowCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 160.0f / static_cast<float>(sampleRateHz));
    const auto midCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 1200.0f / static_cast<float>(sampleRateHz));
    const auto highCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 5200.0f / static_cast<float>(sampleRateHz));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto left = buffer.getReadPointer(0)[sample];
        const auto right = numChannels > 1 ? buffer.getReadPointer(1)[sample] : left;
        const auto mono = (left + right) * 0.5f;
        const auto side = (left - right) * 0.5f;
        const auto absMono = std::abs(mono);

        lowFollower = lowFollower * lowCoeff + mono * (1.0f - lowCoeff);
        midFollower = midFollower * midCoeff + mono * (1.0f - midCoeff);
        highFollower = highFollower * highCoeff + mono * (1.0f - highCoeff);

        const auto low = lowFollower;
        const auto mid = midFollower - lowFollower;
        const auto high = mono - highFollower;
        const auto derivative = std::abs(mono - block.previousMono);
        block.previousMono = mono;

        block.sumSquares += mono * mono;
        block.sumAbs += absMono;
        block.sumDerivative += derivative;
        block.sumLow += low * low;
        block.sumMid += mid * mid;
        block.sumHigh += high * high;
        block.sumSide += side * side;
        block.sumMidStereo += mono * mono;
        block.peak = juce::jmax(block.peak, absMono);
        ++block.samples;
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        audioStats.sumSquares += block.sumSquares;
        audioStats.sumAbs += block.sumAbs;
        audioStats.sumDerivative += block.sumDerivative;
        audioStats.sumLow += block.sumLow;
        audioStats.sumMid += block.sumMid;
        audioStats.sumHigh += block.sumHigh;
        audioStats.sumSide += block.sumSide;
        audioStats.sumMidStereo += block.sumMidStereo;
        audioStats.peak = juce::jmax(audioStats.peak, block.peak);
        audioStats.previousMono = block.previousMono;
        audioStats.samples += block.samples;

        if (++blocksSinceSnapshot > 260)
        {
            latestSnapshot = audioStats;
            audioStats.reset();
            audioStats.previousMono = block.previousMono;
            blocksSinceSnapshot = 0;
        }
    }

    auto inputFingerprint = makeFingerprintFromStats(block);
    auto toneTarget = makeToneRackTarget(inputFingerprint);
    applyToneRack(buffer, toneTarget);
}

float HoneyBadgerHolyGrailAudioProcessor::readParam(juce::AudioProcessorValueTreeState& state, const char* id)
{
    return state.getRawParameterValue(id)->load();
}

juce::String HoneyBadgerHolyGrailAudioProcessor::currentGenre() const
{
    return genres()[static_cast<int>(readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "genre"))];
}

juce::String HoneyBadgerHolyGrailAudioProcessor::currentMission() const
{
    return missions()[static_cast<int>(readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "mission"))];
}

juce::String HoneyBadgerHolyGrailAudioProcessor::currentInstrument() const
{
    return instruments()[static_cast<int>(readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "instrument"))];
}

juce::String HoneyBadgerHolyGrailAudioProcessor::currentStyle() const
{
    return styles()[static_cast<int>(readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "style"))];
}

juce::String HoneyBadgerHolyGrailAudioProcessor::currentIntensity() const
{
    return intensities()[static_cast<int>(readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "intensity"))];
}

HoneyBadgerHolyGrailAudioProcessor::Fingerprint HoneyBadgerHolyGrailAudioProcessor::makeFingerprintFromStats(const RunningStats& stats) const
{
    Fingerprint fp;
    fp.captured = stats.samples > 0;
    fp.mission = currentMission();
    fp.genre = currentGenre();
    fp.instrument = currentInstrument();
    fp.style = currentStyle();
    fp.intensity = currentIntensity();

    const auto samples = juce::jmax(1, stats.samples);
    const auto rms = std::sqrt(stats.sumSquares / static_cast<double>(samples));
    const auto low = std::sqrt(stats.sumLow / static_cast<double>(samples));
    const auto mid = std::sqrt(stats.sumMid / static_cast<double>(samples));
    const auto high = std::sqrt(stats.sumHigh / static_cast<double>(samples));
    const auto side = std::sqrt(stats.sumSide / static_cast<double>(samples));
    const auto midStereo = std::sqrt(stats.sumMidStereo / static_cast<double>(samples));
    const auto total = low + mid + high + 0.000001;

    fp.peakDb = gainToDb(stats.peak);
    fp.rmsDb = gainToDb(static_cast<float>(rms));
    fp.crestDb = fp.peakDb - fp.rmsDb;
    fp.lowBuild = static_cast<float>(low / total);
    fp.midDensity = static_cast<float>(mid / total);
    fp.brightness = static_cast<float>(high / total);
    fp.harshness = fp.brightness * 0.7f + static_cast<float>(mid / total) * 0.3f;
    fp.width = static_cast<float>(side / (midStereo + side + 0.000001));
    fp.transient = static_cast<float>((stats.sumDerivative / static_cast<double>(samples)) / (rms + 0.000001));
    return fp;
}

HoneyBadgerHolyGrailAudioProcessor::Fingerprint HoneyBadgerHolyGrailAudioProcessor::getLatestFingerprint() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return makeFingerprintFromStats(latestSnapshot.samples > 0 ? latestSnapshot : audioStats);
}

void HoneyBadgerHolyGrailAudioProcessor::setActiveSlot(int slotIndex)
{
    activeSlot.store(juce::jlimit(0, 7, slotIndex));
}

HoneyBadgerHolyGrailAudioProcessor::Fingerprint HoneyBadgerHolyGrailAudioProcessor::getSlot(int slotIndex) const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return slots[static_cast<size_t>(juce::jlimit(0, 7, slotIndex))];
}

juce::String HoneyBadgerHolyGrailAudioProcessor::getLastMessage() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return lastMessage;
}

juce::String HoneyBadgerHolyGrailAudioProcessor::getToneRackReadout() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return lastToneRackReadout;
}

juce::String HoneyBadgerHolyGrailAudioProcessor::classify(float value, float low, float high,
                                                          const juce::String& lowText,
                                                          const juce::String& goodText,
                                                          const juce::String& highText)
{
    if (value < low)
        return lowText;
    if (value > high)
        return highText;
    return goodText;
}

juce::String HoneyBadgerHolyGrailAudioProcessor::makeSlotAdvice(const Fingerprint& now, const Fingerprint* previous, bool recheck) const
{
    juce::StringArray fixes;
    const auto instrument = now.instrument.toLowerCase();
    const auto mission = now.mission.toLowerCase();

    if (now.rmsDb < -36.0f)
        return "Buttmeister hears almost nothing. Solo the sound or send, play a real section, then hit Destroy It again.";

    if (now.peakDb > -1.0f)
        fixes.add("Back the gain down 2-4 dB. Clipping is not confidence.");
    else if (now.peakDb < -12.0f)
        fixes.add("Bring the level up. Buttmeister cannot judge a whisper pretending to be a weapon.");

    if (now.crestDb < 5.0f && (instrument.contains("kick") || instrument.contains("snare") || instrument.contains("clap")))
        fixes.add("Restore transient punch. Ease off compression or clipping until the hit has teeth again.");

    if (now.lowBuild > 0.48f && ! instrument.contains("808") && ! instrument.contains("bass") && ! instrument.contains("kick"))
        fixes.add("High-pass the junk. This sound is dragging low-end mud it has no right to own.");

    if (now.lowBuild > 0.58f && (instrument.contains("808") || instrument.contains("bass")))
        fixes.add("Tighten the sub lane. Keep the weight, but add upper harmonics so it survives small speakers.");

    if (mission.contains("voice") || instrument.contains("vocal"))
    {
        if (now.lowBuild > 0.34f)
            fixes.add("Clean the vocal lows. Leave chest, lose the blanket below the voice.");
        if (now.brightness < 0.18f)
            fixes.add("Push intelligibility around 2-5 kHz. Make the words step forward before adding level.");
        if (now.brightness > 0.46f)
            fixes.add("De-ess or soften the air band. A loud S is not charisma.");
    }

    if (mission.contains("reverb") || instrument.contains("reverb send"))
    {
        if (now.lowBuild > 0.30f)
            fixes.add("High-pass the reverb return harder. The room should not carry sub weight.");
        if (now.midDensity > 0.48f)
            fixes.add("Scoop the reverb body around 300-700 Hz so the dry sound stays in front.");
    }

    if (mission.contains("delay") || instrument.contains("delay send"))
    {
        if (now.lowBuild > 0.32f)
            fixes.add("Filter delay lows before feedback piles up.");
        if (now.brightness > 0.45f)
            fixes.add("Darken the repeats so they tuck behind the source instead of poking holes in it.");
    }

    if (mission.contains("small speaker") || instrument.contains("808"))
    {
        if (now.brightness < 0.20f)
            fixes.add("Add harmonics above the sub. The phone speaker needs a shadow of the 808 to grab.");
        if (now.lowBuild > 0.64f)
            fixes.add("Do not keep boosting the fundamental. Trade a little sub for saturation and upper bass.");
    }

    if (now.midDensity > 0.54f)
        fixes.add("Cut a little 250-600 Hz. The belly is too soft and it is trying to eat the whole room.");

    if (now.brightness < 0.12f && (instrument.contains("lead") || instrument.contains("pluck") || instrument.contains("vocal")))
        fixes.add("Add controlled bite around 2-5 kHz. Do not turn it up; give it teeth.");
    else if (now.brightness > 0.42f)
        fixes.add("Tame the top. If the sound hurts solo, the stack will punish you.");

    if (now.width > 0.42f && now.lowBuild > 0.36f)
        fixes.add("Narrow the low end. Wide mud is still mud, just wearing sunglasses.");

    if (fixes.isEmpty())
        fixes.add("This dry signal is holding shape. Press Run Away if it still feels right in context.");

    juce::String message = recheck ? "Better. Buttmeister checked the new bite.\n\n" : "Buttmeister says:\n\n";
    message << now.mission << " / " << now.instrument << " / " << now.genre << " / " << now.style << " / " << now.intensity << "\n";
    message << "Peak " << juce::String(now.peakDb, 1) << " dB, RMS " << juce::String(now.rmsDb, 1)
            << " dB, Rip " << juce::String(juce::jlimit(0.0f, 100.0f, (1.0f - std::abs(now.midDensity - 0.38f)) * 70.0f + now.transient * 12.0f), 0) << "%.\n\n";

    if (previous != nullptr && previous->captured)
    {
        if (now.midDensity < previous->midDensity - 0.05f)
            message << "The mud is down. Now we are biting.\n";
        if (now.brightness > previous->brightness + 0.04f)
            message << "The top end woke up. Keep it controlled.\n";
        if (now.width < previous->width - 0.05f)
            message << "The center got stronger. Good.\n";
    }

    message << "\nDo this next:\n";
    for (int i = 0; i < fixes.size(); ++i)
        message << juce::String(i + 1) << ". " << fixes[i] << "\n";
    message << "\nRecheck it. Make it harder to tear apart.";
    return message;
}

juce::String HoneyBadgerHolyGrailAudioProcessor::captureSlot(bool recheck)
{
    const auto index = activeSlot.load();
    RunningStats snapshot;
    Fingerprint previous;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        snapshot = latestSnapshot.samples > 0 ? latestSnapshot : audioStats;
        previous = slots[static_cast<size_t>(index)];
    }

    auto fp = makeFingerprintFromStats(snapshot);
    fp.verdict = makeSlotAdvice(fp, previous.captured ? &previous : nullptr, recheck);

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        slots[static_cast<size_t>(index)] = fp;
        lastMessage = "Slot " + juce::String(index + 1) + " captured.\n\n" + fp.verdict;
        audioStats.reset();
        latestSnapshot.reset();
    }

    return getLastMessage();
}

juce::String HoneyBadgerHolyGrailAudioProcessor::approveSlot()
{
    const auto index = activeSlot.load();
    std::lock_guard<std::mutex> lock(dataMutex);
    auto& slot = slots[static_cast<size_t>(index)];

    if (! slot.captured)
    {
        lastMessage = "Slot " + juce::String(index + 1) + " has no fingerprint. Destroy first, run later.";
        return lastMessage;
    }

    slot.approved = true;
    lastMessage = "There it is.\n\nSlot " + juce::String(index + 1) + " is approved: " + slot.instrument
        + ". Buttmeister has nothing left to fight in this signal.\n\nYou may now run away.";
    return lastMessage;
}

juce::String HoneyBadgerHolyGrailAudioProcessor::makeStackAdvice(const Fingerprint& stack, const std::vector<Fingerprint>& selected) const
{
    juce::StringArray fixes;
    juce::StringArray names;
    float approvedLow = 0.0f;
    float approvedMid = 0.0f;
    float approvedBright = 0.0f;
    float widest = 0.0f;

    for (const auto& slot : selected)
    {
        names.add(slot.instrument);
        approvedLow += slot.lowBuild;
        approvedMid += slot.midDensity;
        approvedBright += slot.brightness;
        widest = juce::jmax(widest, slot.width);
    }

    const auto count = juce::jmax(1.0f, static_cast<float>(selected.size()));
    approvedLow /= count;
    approvedMid /= count;
    approvedBright /= count;

    if (stack.peakDb > -1.0f)
        fixes.add("Pull the selected stack down 2-3 dB. Headroom is not optional.");
    if (stack.lowBuild > approvedLow + 0.10f)
        fixes.add("The low end is stacking too hard. Pick who owns the sub and move the other sound into harmonics.");
    if (stack.midDensity > approvedMid + 0.10f)
        fixes.add("The body bands are crowding. Cut the support sound around 250-700 Hz instead of making the hero louder.");
    if (stack.brightness < approvedBright - 0.08f)
        fixes.add("Something is hiding the bite. Clear space around the lead/vocal presence before boosting it.");
    if (stack.width > widest + 0.08f && stack.lowBuild > 0.32f)
        fixes.add("The stack got wider than the approved sounds. Narrow lows and low-mids before this thing tears.");

    if (fixes.isEmpty())
        fixes.add("The stack is holding. Nothing is folding, hiding, or ripping apart. Run away.");

    juce::String message = "Stack Destroy: " + names.joinIntoString(" + ") + "\n\n";
    message << "Peak " << juce::String(stack.peakDb, 1) << " dB, RMS " << juce::String(stack.rmsDb, 1)
            << " dB, width " << juce::String(stack.width * 100.0f, 0) << "%.\n\n";
    message << "Buttmeister says:\n";
    if (fixes.size() > 1)
        message << "These sounds are pulling on each other. We fix the relationship, not the ego.\n\n";
    else
        message << "This stack is dense. It is not ripping.\n\n";

    message << "Do this next:\n";
    for (int i = 0; i < fixes.size(); ++i)
        message << juce::String(i + 1) << ". " << fixes[i] << "\n";
    message << "\nRecheck the stack when you move the pieces.";
    return message;
}

HoneyBadgerHolyGrailAudioProcessor::ToneRackState HoneyBadgerHolyGrailAudioProcessor::makeToneRackTarget(const Fingerprint& input) const
{
    ToneRackState target;
    target.active = readParam(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "autoTone") > 0.5f;
    target.amount = juce::jlimit(0.0f, 2.0f,
        paramOrDefault(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "toneAmount", 1.0f));
    target.outputDb = paramOrDefault(const_cast<juce::AudioProcessorValueTreeState&>(parameters), "outputTrim", 0.0f);

    if (! target.active || input.rmsDb < -70.0f)
        return target;

    const auto instrument = input.instrument.toLowerCase();
    const auto mission = input.mission.toLowerCase();
    const auto style = input.style.toLowerCase();
    const auto genre = input.genre.toLowerCase();
    const auto intent = toneIntensityScale(input.intensity) * target.amount;
    const auto isLowInstrument = instrument.contains("808") || instrument.contains("bass") || instrument.contains("kick");
    const auto isVocalMission = mission.contains("voice") || instrument.contains("vocal");
    const auto isReverbSend = mission.contains("reverb") || instrument.contains("reverb send");
    const auto isDelaySend = mission.contains("delay") || instrument.contains("delay send");
    const auto isSmallSpeakerSub = mission.contains("small speaker") || instrument.contains("808");
    const auto isTransient = instrument.contains("kick") || instrument.contains("snare") || instrument.contains("clap");
    const auto isLeadLike = instrument.contains("lead") || instrument.contains("pluck") || instrument.contains("vocal")
        || instrument.contains("guitar") || instrument.contains("piano");

    if (isReverbSend)
    {
        target.highPassHz = juce::jlimit(170.0f, 420.0f, 190.0f + input.lowBuild * 360.0f);
        target.lowShelfDb = juce::jlimit(-14.0f, -3.0f, -5.0f - input.lowBuild * 16.0f * intent);
        target.mudDb = juce::jlimit(-16.0f, -2.0f, (0.22f - input.midDensity) * 40.0f * intent - 3.0f);
        target.presenceDb = juce::jlimit(-3.0f, 3.5f, (0.30f - input.brightness) * 7.0f * intent);
        target.airDb = juce::jlimit(-7.0f, 5.0f, (0.38f - input.brightness) * 11.0f * intent);
        target.driveDb = juce::jlimit(0.0f, 5.0f, 1.4f * intent);
        target.outputDb -= 1.5f * intent;
        return target;
    }

    if (isDelaySend)
    {
        target.highPassHz = juce::jlimit(120.0f, 340.0f, 145.0f + input.lowBuild * 260.0f);
        target.lowShelfDb = juce::jlimit(-12.0f, -2.0f, -3.5f - input.lowBuild * 12.0f * intent);
        target.mudDb = juce::jlimit(-13.0f, -1.0f, (0.28f - input.midDensity) * 32.0f * intent - 2.0f);
        target.presenceDb = juce::jlimit(-2.0f, 5.0f, (0.25f - input.brightness) * 10.0f * intent);
        target.airDb = input.brightness > 0.42f ? -4.0f * intent : 1.6f * intent;
        target.driveDb = juce::jlimit(0.0f, 7.0f, 2.0f * intent);
        target.outputDb -= 1.2f * intent;
        return target;
    }

    if (isVocalMission)
    {
        target.highPassHz = juce::jlimit(70.0f, 180.0f, 78.0f + input.lowBuild * 170.0f);
        target.lowShelfDb = juce::jlimit(-10.0f, 1.0f, (0.20f - input.lowBuild) * 18.0f * intent);
        target.mudDb = juce::jlimit(-12.0f, 0.0f, (0.30f - input.midDensity) * 31.0f * intent);
        target.presenceDb = juce::jlimit(0.0f, 8.5f, (0.27f - input.brightness) * 18.0f * intent + 1.2f * intent);
        target.airDb = input.brightness > 0.43f
            ? juce::jlimit(-8.0f, 0.0f, (0.39f - input.brightness) * 24.0f * intent)
            : juce::jlimit(0.0f, 6.5f, (0.34f - input.brightness) * 12.0f * intent);
        target.driveDb = juce::jlimit(0.0f, 6.5f, 2.2f * intent);
        target.outputDb -= juce::jlimit(0.0f, 4.5f, target.presenceDb * 0.18f + target.driveDb * 0.22f);
        return target;
    }

    target.highPassHz = isLowInstrument ? 24.0f : 88.0f;
    if (! isLowInstrument && input.lowBuild > 0.42f)
        target.highPassHz += (input.lowBuild - 0.42f) * 520.0f;
    if (instrument.contains("hats") || instrument.contains("fx"))
        target.highPassHz = juce::jmax(target.highPassHz, 220.0f);

    if (isLowInstrument)
    {
        target.lowShelfDb = juce::jlimit(-8.0f, 5.0f, (0.50f - input.lowBuild) * 18.0f * intent);
        target.presenceDb = juce::jlimit(0.0f, 11.0f, (0.26f - input.brightness) * 22.0f * intent);
        target.driveDb = juce::jlimit(0.0f, 18.0f, (0.64f - input.brightness) * 19.0f * intent);
        if (isSmallSpeakerSub)
        {
            target.lowShelfDb = juce::jlimit(-6.0f, 2.0f, (0.46f - input.lowBuild) * 14.0f * intent);
            target.presenceDb = juce::jlimit(3.0f, 13.5f, (0.38f - input.brightness) * 23.0f * intent + 2.0f);
            target.driveDb = juce::jlimit(4.0f, 22.0f, (0.72f - input.brightness) * 21.0f * intent + 2.5f);
        }
    }
    else
    {
        target.lowShelfDb = juce::jlimit(-12.0f, 3.0f, (0.28f - input.lowBuild) * 21.0f * intent);
        target.presenceDb = isLeadLike ? juce::jlimit(0.0f, 12.0f, (0.26f - input.brightness) * 24.0f * intent) : 0.0f;
        target.driveDb = style.contains("gritty") || style.contains("aggressive") || genre.contains("dubstep")
            ? 7.5f * intent : 3.5f * intent;
    }

    target.mudDb = juce::jlimit(-14.0f, 0.0f, (0.34f - input.midDensity) * 34.0f * intent);
    target.airDb = juce::jlimit(-10.0f, 8.0f, (0.34f - input.brightness) * 18.0f * intent);

    if (input.brightness > 0.43f)
        target.airDb = juce::jlimit(-12.0f, 0.0f, (0.38f - input.brightness) * 30.0f * intent);
    if (isTransient)
        target.presenceDb += juce::jlimit(0.0f, 5.0f, (0.16f - input.transient) * 8.0f * intent);
    if (style.contains("clean"))
        target.driveDb *= 0.35f;
    if (style.contains("bright"))
        target.airDb += 1.2f * intent;
    if (style.contains("dark"))
        target.airDb -= 1.5f * intent;

    target.driveDb = juce::jlimit(0.0f, 24.0f, target.driveDb);
    target.outputDb -= juce::jlimit(0.0f, 9.0f, target.driveDb * 0.32f + juce::jmax(0.0f, target.presenceDb) * 0.16f);
    return target;
}

void HoneyBadgerHolyGrailAudioProcessor::applyToneRack(juce::AudioBuffer<float>& buffer, const ToneRackState& target)
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (! target.active || target.amount <= 0.001f)
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        lastToneRackReadout = "Tone Rack bypassed.";
        return;
    }

    toneRackSmoothed.active = target.active;
    toneRackSmoothed.amount = lerpTone(toneRackSmoothed.amount, target.amount);
    toneRackSmoothed.highPassHz = lerpTone(toneRackSmoothed.highPassHz, target.highPassHz);
    toneRackSmoothed.lowShelfDb = lerpTone(toneRackSmoothed.lowShelfDb, target.lowShelfDb);
    toneRackSmoothed.mudDb = lerpTone(toneRackSmoothed.mudDb, target.mudDb);
    toneRackSmoothed.presenceDb = lerpTone(toneRackSmoothed.presenceDb, target.presenceDb);
    toneRackSmoothed.airDb = lerpTone(toneRackSmoothed.airDb, target.airDb);
    toneRackSmoothed.driveDb = lerpTone(toneRackSmoothed.driveDb, target.driveDb);
    toneRackSmoothed.outputDb = lerpTone(toneRackSmoothed.outputDb, target.outputDb);

    const auto sr = juce::jmax(1000.0, sampleRateHz);
    const auto hp = juce::jlimit(20.0f, 900.0f, toneRackSmoothed.highPassHz);
    const auto lowGain = juce::Decibels::decibelsToGain(toneRackSmoothed.lowShelfDb);
    const auto mudGain = juce::Decibels::decibelsToGain(toneRackSmoothed.mudDb);
    const auto presenceGain = juce::Decibels::decibelsToGain(toneRackSmoothed.presenceDb);
    const auto airGain = juce::Decibels::decibelsToGain(toneRackSmoothed.airDb);
    const auto driveGain = juce::Decibels::decibelsToGain(toneRackSmoothed.driveDb);
    const auto outputGain = juce::Decibels::decibelsToGain(toneRackSmoothed.outputDb);

    const auto hpCoeffs = juce::IIRCoefficients::makeHighPass(sr, hp, 0.707f);
    const auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(sr, 115.0f, 0.72f, lowGain);
    const auto mudCoeffs = juce::IIRCoefficients::makePeakFilter(sr, 420.0f, 1.05f, mudGain);
    const auto presenceCoeffs = juce::IIRCoefficients::makePeakFilter(sr, 3300.0f, 0.82f, presenceGain);
    const auto airCoeffs = juce::IIRCoefficients::makeHighShelf(sr, 9200.0f, 0.65f, airGain);

    const auto channels = juce::jmin(2, buffer.getNumChannels());
    for (int channel = 0; channel < channels; ++channel)
    {
        toneHighPass[static_cast<size_t>(channel)].setCoefficients(hpCoeffs);
        toneLowShelf[static_cast<size_t>(channel)].setCoefficients(lowCoeffs);
        toneMudPeak[static_cast<size_t>(channel)].setCoefficients(mudCoeffs);
        tonePresencePeak[static_cast<size_t>(channel)].setCoefficients(presenceCoeffs);
        toneAirShelf[static_cast<size_t>(channel)].setCoefficients(airCoeffs);

        auto* samples = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto x = samples[sample];
            x = toneHighPass[static_cast<size_t>(channel)].processSingleSampleRaw(x);
            x = toneLowShelf[static_cast<size_t>(channel)].processSingleSampleRaw(x);
            x = toneMudPeak[static_cast<size_t>(channel)].processSingleSampleRaw(x);
            x = tonePresencePeak[static_cast<size_t>(channel)].processSingleSampleRaw(x);
            x = toneAirShelf[static_cast<size_t>(channel)].processSingleSampleRaw(x);
            x = std::tanh(x * driveGain) / std::tanh(driveGain);
            samples[sample] = x * outputGain;
        }
    }

    for (int channel = channels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    std::lock_guard<std::mutex> lock(dataMutex);
    lastToneRackReadout = "Destroyer: HP " + juce::String(hp, 0) + " Hz | low "
        + juce::String(toneRackSmoothed.lowShelfDb, 1) + " dB | mud "
        + juce::String(toneRackSmoothed.mudDb, 1) + " dB | bite "
        + juce::String(toneRackSmoothed.presenceDb, 1) + " dB | air "
        + juce::String(toneRackSmoothed.airDb, 1) + " dB | drive "
        + juce::String(toneRackSmoothed.driveDb, 1) + " dB";
}

juce::String HoneyBadgerHolyGrailAudioProcessor::analyzeStack(const std::array<bool, 8>& selectedSlots)
{
    RunningStats snapshot;
    std::vector<Fingerprint> selected;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        snapshot = latestSnapshot.samples > 0 ? latestSnapshot : audioStats;
        for (size_t i = 0; i < slots.size(); ++i)
            if (selectedSlots[i] && slots[i].captured)
                selected.push_back(slots[i]);
    }

    if (selected.size() < 2)
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        lastMessage = "Click at least two captured slot numbers before Stack Destroy. Buttmeister only judges what you point at.";
        return lastMessage;
    }

    auto stack = makeFingerprintFromStats(snapshot);
    auto message = makeStackAdvice(stack, selected);

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        lastMessage = message;
        audioStats.reset();
        latestSnapshot.reset();
    }

    return getLastMessage();
}

void HoneyBadgerHolyGrailAudioProcessor::writeSlotToXml(juce::XmlElement& parent, int slotIndex) const
{
    const auto& slot = slots[static_cast<size_t>(slotIndex)];
    auto* child = parent.createNewChildElement("SLOT");
    child->setAttribute("index", slotIndex);
    child->setAttribute("captured", slot.captured);
    child->setAttribute("approved", slot.approved);
    child->setAttribute("mission", slot.mission);
    child->setAttribute("genre", slot.genre);
    child->setAttribute("instrument", slot.instrument);
    child->setAttribute("style", slot.style);
    child->setAttribute("intensity", slot.intensity);
    child->setAttribute("peakDb", slot.peakDb);
    child->setAttribute("rmsDb", slot.rmsDb);
    child->setAttribute("crestDb", slot.crestDb);
    child->setAttribute("brightness", slot.brightness);
    child->setAttribute("transient", slot.transient);
    child->setAttribute("width", slot.width);
    child->setAttribute("lowBuild", slot.lowBuild);
    child->setAttribute("midDensity", slot.midDensity);
    child->setAttribute("harshness", slot.harshness);
}

void HoneyBadgerHolyGrailAudioProcessor::readSlotFromXml(const juce::XmlElement& element)
{
    const auto index = element.getIntAttribute("index", -1);
    if (! juce::isPositiveAndBelow(index, 8))
        return;

    auto& slot = slots[static_cast<size_t>(index)];
    slot.captured = element.getBoolAttribute("captured", false);
    slot.approved = element.getBoolAttribute("approved", false);
    slot.mission = element.getStringAttribute("mission");
    slot.genre = element.getStringAttribute("genre");
    slot.instrument = element.getStringAttribute("instrument");
    slot.style = element.getStringAttribute("style");
    slot.intensity = element.getStringAttribute("intensity");
    slot.peakDb = static_cast<float>(element.getDoubleAttribute("peakDb", -100.0));
    slot.rmsDb = static_cast<float>(element.getDoubleAttribute("rmsDb", -100.0));
    slot.crestDb = static_cast<float>(element.getDoubleAttribute("crestDb", 0.0));
    slot.brightness = static_cast<float>(element.getDoubleAttribute("brightness", 0.0));
    slot.transient = static_cast<float>(element.getDoubleAttribute("transient", 0.0));
    slot.width = static_cast<float>(element.getDoubleAttribute("width", 0.0));
    slot.lowBuild = static_cast<float>(element.getDoubleAttribute("lowBuild", 0.0));
    slot.midDensity = static_cast<float>(element.getDoubleAttribute("midDensity", 0.0));
    slot.harshness = static_cast<float>(element.getDoubleAttribute("harshness", 0.0));
}

void HoneyBadgerHolyGrailAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto root = std::make_unique<juce::XmlElement>("BUTTMEISTER_SMALL_SPEAKER_DESTROYER");
    if (auto state = parameters.copyState(); auto paramsXml = state.createXml())
        root->addChildElement(paramsXml.release());

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        for (int i = 0; i < 8; ++i)
            writeSlotToXml(*root, i);
    }

    copyXmlToBinary(*root, destData);
}

void HoneyBadgerHolyGrailAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (auto* paramsXml = xmlState->getChildByName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*paramsXml));

        std::lock_guard<std::mutex> lock(dataMutex);
        slots = {};
        for (auto* child : xmlState->getChildIterator())
            if (child->hasTagName("SLOT"))
                readSlotFromXml(*child);
    }
}

juce::AudioProcessorEditor* HoneyBadgerHolyGrailAudioProcessor::createEditor()
{
    return new HoneyBadgerHolyGrailAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoneyBadgerHolyGrailAudioProcessor();
}
