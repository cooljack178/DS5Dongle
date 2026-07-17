//
// Created by awalol on 2026/5/15.
//

#include "state_mgr.h"

#include <cstring>

#include "config.h"
#include "usb.h"
#include "utils.h"

static constexpr SetStateData state_init_data = {
    // 0xfd
    .EnableRumbleEmulation = 1,
    .UseRumbleNotHaptics = 0,
    .AllowRightTriggerFFB = 1,
    .AllowLeftTriggerFFB = 1,
    .AllowHeadphoneVolume = 1,
    .AllowSpeakerVolume = 1,
    .AllowMicVolume = 1,
    .AllowAudioControl = 1,
    // 0xf7
    .AllowMuteLight = 1,
    .AllowAudioMute = 1,
    .AllowLedColor = 1,
    .ResetLights = 0,
    .AllowPlayerIndicators = 1,
    .AllowHapticLowPassFilter = 1,
    .AllowMotorPowerLevel = 1,
    .AllowAudioControl2 = 1,

    .VolumeMic = 0xff,

    // AudioControl 0x09
    .MicSelect = 1, // Internal Only
    .NoiseCancelEnable = 1,

    // MuteControl 0x0f
    .TouchPowerSave = 1,
    .MotionPowerSave = 1,
    .HapticPowerSave = 1,
    .AudioPowerSave = 1,

    // MotorPowerLevel
    // .TriggerMotorPowerReduction = 7,

    // AudioControl2
    .SpeakerCompPreGain = 1,
    .BeamformingEnable = 0,

    .AllowLightBrightnessChange = 1,
    .AllowColorLightFadeAnimation = 1,
    .EnableImprovedRumbleEmulation = 1,

    .LightFadeAnimation = LightFadeAnimation::FadeOut,
    .LightBrightness = LightBrightness::Mid,

    // RGB LED: R, G, B (Nijika Color!)✨
    .LedRed = 0xff,
    .LedGreen = 0xd7,
    .LedBlue = 0x00,
};

SetStateData state{};
static volatile bool hardware_mic_muted = false;

void state_init() {
    hardware_mic_muted = false;
    mute[1] = 0;
    state = state_init_data;
    state.VolumeSpeaker = get_config().speaker_volume;
    state.VolumeHeadphones = get_config().headset_volume;
    set_volume(get_config().speaker_volume, get_config().headset_volume);
    if (get_config().speaker_gain != 0) {
        set_gain(get_config().speaker_gain);
    }
    if (get_config().trigger_reduce != 0) {
        state.TriggerMotorPowerReduction = get_config().trigger_reduce;
    }
}

void state_set(uint8_t *data, const uint8_t size) {
    if (size > 63) {
        printf("[StateMgr] Warning: State Set over 63 bytes\n");
    }
    memcpy(data, &state, size);
}

void state_set_hardware_mic_muted(const bool muted) {
    hardware_mic_muted = muted;
    mute[1] = muted ? 1 : 0;
    state.AllowMuteLight = 1;
    state.AllowAudioMute = 1;
    state.MuteLightMode = muted ? MuteLight::On : MuteLight::Off;
    state.MicMute = muted ? 1 : 0;
}

bool state_hardware_mic_muted() {
    return hardware_mic_muted;
}

void state_update(const uint8_t *data, const uint8_t size) {
    if (size < 47) {
        printf(
            "[StateMgr] Error: SetStateData needs %u bytes, got %u\n",
            static_cast<unsigned>(sizeof(SetStateData)),
            size
        );
        return;
    }

    SetStateData update{};
    memcpy(&update, data, sizeof(SetStateData));

    state.EnableRumbleEmulation = update.EnableRumbleEmulation;
    state.UseRumbleNotHaptics = update.UseRumbleNotHaptics;
    state.EnableImprovedRumbleEmulation = update.EnableImprovedRumbleEmulation;
    state.UseRumbleNotHaptics2 = update.UseRumbleNotHaptics2;
    if (state.UseRumbleNotHaptics || state.UseRumbleNotHaptics2) {
        state.RumbleEmulationLeft = update.RumbleEmulationLeft;
        state.RumbleEmulationRight = update.RumbleEmulationRight;
    }else {
        state.RumbleEmulationLeft = state.RumbleEmulationRight = 0;
    }

    if (update.AllowHeadphoneVolume) {
        get_config().headset_volume = update.VolumeHeadphones;
        state.VolumeHeadphones = update.VolumeHeadphones;
    }
    if (update.AllowSpeakerVolume) {
        get_config().speaker_volume = update.VolumeSpeaker;
        state.VolumeSpeaker = update.VolumeSpeaker;
    }
    if (update.AllowMicVolume) {
        state.VolumeMic = update.VolumeMic;
    }

    if (update.AllowAudioControl) {
        state.MicSelect = update.MicSelect;
        state.EchoCancelEnable = update.EchoCancelEnable;
        state.NoiseCancelEnable = update.NoiseCancelEnable;
        state.OutputPathSelect = update.OutputPathSelect;
        state.InputPathSelect = update.InputPathSelect;
    }

    if (update.AllowMuteLight) {
        state.MuteLightMode = update.MuteLightMode;
    }

    if (update.AllowAudioMute) {
        state.TouchPowerSave = update.TouchPowerSave;
        state.MotionPowerSave = update.MotionPowerSave;
        state.HapticPowerSave = update.HapticPowerSave;
        state.AudioPowerSave = update.AudioPowerSave;
        state.MicMute = update.MicMute;
        state.SpeakerMute = update.SpeakerMute;
        state.HeadphoneMute = update.HeadphoneMute;
        state.HapticMute = update.HapticMute;
    }

    // The physical controller button is authoritative. Host output reports
    // must not clear the controller-side mute or its orange indicator.
    if (hardware_mic_muted) {
        state.AllowMuteLight = 1;
        state.AllowAudioMute = 1;
        state.MuteLightMode = MuteLight::On;
        state.MicMute = 1;
    }

    if (update.AllowRightTriggerFFB) {
        memcpy(state.RightTriggerFFB, update.RightTriggerFFB, sizeof(state.RightTriggerFFB));
    }
    if (update.AllowLeftTriggerFFB) {
        memcpy(state.LeftTriggerFFB, update.LeftTriggerFFB, sizeof(state.LeftTriggerFFB));
    }

    if (update.AllowMotorPowerLevel) {
        state.RumbleMotorPowerReduction = update.RumbleMotorPowerReduction;
        if (get_config().trigger_reduce == 0) {
            state.TriggerMotorPowerReduction = update.TriggerMotorPowerReduction;
        }
    }

    if (update.AllowAudioControl2) {
        if (get_config().speaker_gain == 0) {
            state.SpeakerCompPreGain = update.SpeakerCompPreGain;
        }
        state.BeamformingEnable = update.BeamformingEnable;
        state.UnkAudioControl2 = update.UnkAudioControl2;
    }

    if (update.AllowHapticLowPassFilter) {
        state.HapticLowPassFilter = update.HapticLowPassFilter;
        state.UNKBIT = update.UNKBIT;
    }

    if (update.AllowColorLightFadeAnimation) {
        state.LightFadeAnimation = update.LightFadeAnimation;
    }
    if (update.AllowLightBrightnessChange) {
        state.LightBrightness = update.LightBrightness;
    }

    if (update.AllowPlayerIndicators) {
        state.PlayerLight1 = update.PlayerLight1;
        state.PlayerLight2 = update.PlayerLight2;
        state.PlayerLight3 = update.PlayerLight3;
        state.PlayerLight4 = update.PlayerLight4;
        state.PlayerLight5 = update.PlayerLight5;
        state.PlayerLightFade = update.PlayerLightFade;
        state.PlayerLightUNK = update.PlayerLightUNK;
    }

    if (update.AllowLedColor) {
        state.LedRed = update.LedRed;
        state.LedGreen = update.LedGreen;
        state.LedBlue = update.LedBlue;
    }
}

// for usbaudio SET_CUR cmd
void set_volume(const uint8_t value) {
    // printf("[StateMgr] SetVolume: %u\n",value);
    state.VolumeSpeaker = value;
    state.VolumeHeadphones = value;
    get_config().speaker_volume = value;
    get_config().headset_volume = value;
}

void set_volume(const uint8_t speaker, const uint8_t headset) {
    state.VolumeSpeaker = speaker;
    state.VolumeHeadphones = headset;
}

void set_gain(const uint8_t value) {
    state.SpeakerCompPreGain = value;
}

void set_trigger_reduce(const uint8_t value) {
    state.TriggerMotorPowerReduction = value;
}
