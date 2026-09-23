# Buttmeister Small Speaker Destroyer

Buttmeister Small Speaker Destroyer is an auto EQ coach, stack analyzer, and active tone rack for dry tracks, vocals, effect sends, and sub bass/808 translation. Put it on a raw track or send, pick the mission, capture fingerprints into slots, approve the strong ones, then select multiple slots and run a stack destroy pass.

This repository contains editable Windows x64 VST3/standalone source and build scripts. Compiled plugins, audio, presets, and the JUCE framework are not included.

The **Destroyer Rack** can process audio directly. When enabled, Buttmeister uses its live fingerprint to drive high-pass cleanup, low shelf control, mud cuts, presence and air shaping, saturation drive, and output compensation. The rack defaults to bypass so old sessions stay safe.

## Missions

- **Full Track Auto EQ**: the original general-purpose tone rack.
- **Voice Forward**: vocal cleanup, intelligibility, air control, and gentle harmonic push.
- **Reverb Send Cleaner**: high-passes and scoops reverb returns so the dry source stays forward.
- **Delay Send Cleaner**: filters repeats and keeps feedback from crowding the mix.
- **Sub/808 Small Speaker**: trades some pure sub weight for harmonics that survive phones, laptops, and tiny speakers.

## Workflow

1. Pick a mission, genre, instrument, style, and intensity.
2. Click a slot number from 1 to 8.
3. Solo that sound or send and press **Destroy It**.
4. Tweak the sound, then press **Recheck Slot**.
5. When the dry signal feels right, press **Run Away** to approve and save the fingerprint.
6. Select multiple approved slots and press **Stack Destroy** while they play together.

## Tone Rack

1. Enable **Destroyer Rack** when you want the plugin to fix the raw tone instead of only coaching it.
2. Use **Destroy Amount** to decide how hard Buttmeister can push the EQ and drive decisions.
3. Use **Output Trim** for final level matching after the rack adds harmonics or bite.

The analysis and advice use local signal measurements and rules. They are not an AI service and do not require an account or API key.

## Build

Requirements: Windows x64, Visual Studio 2022 with the Desktop development with C++ workload and Windows SDK, CMake 3.22+ on PATH, and [JUCE 8.0.12](https://github.com/juce-framework/JUCE/tree/8.0.12).

From this repository in PowerShell:

```powershell
git clone --branch 8.0.12 --depth 1 https://github.com/juce-framework/JUCE.git external/JUCE
.\build_with_vs.ps1 -JuceDir "$PWD\external\JUCE"
```

An existing checkout can be supplied with `-JuceDir`. The script builds the Release VST3 and standalone application without installing them. Outputs are under `build-vs\HoneyBadgerHolyGrail_artefacts\Release`.

## Install

Copy the complete `Buttmeister Small Speaker Destroyer.vst3` bundle from the build output's `VST3` folder into a VST3 folder your DAW scans, such as `%CommonProgramFiles%\VST3` or `%LOCALAPPDATA%\Programs\Common\VST3`. Preserve the bundle's `Contents` folder, rescan, and load it as an audio effect.

## Source and validation

This is version 0.3.0. The source retains the original internal `HoneyBadgerHolyGrail` target and class names because the project was renamed during development. Its product name, bundle identifier, and plugin identifiers are the original Small Speaker Destroyer values. No audio behavior or saved-state identifiers were changed for this snapshot. No automated test suite was present in the recovered project.

## License and dependencies

No project-source license has been selected. Public availability does not itself grant a reuse or redistribution license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the external JUCE dependency and its original notice.
