/*
 * Copyright (C) 2018 Fiberdyne Systems
 * 
 * Author: James OShannessy <james.oshannessy@fiberdyne.com.au>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <string.h>
#include "hal-generic-utility.h"
#include "hal-interface.h"
#include "wrap-json.h"

#define ALSA_CARD_NAME "xfalsa"
//#define ALSA_DEVICE_ID "ALC887-VD Analog"

/* Default Values for MasterVolume Ramping */
STATIC halVolRampT volRampMaster = {
    .mode = RAMP_VOL_NORMAL,
    .slave = Master_Playback_Volume,
    .delay = 100 * 1000,
    .stepDown = 3,
    .stepUp = 2,
};

STATIC halVolRampT volRampPhone = {
    .mode = RAMP_VOL_NORMAL,
    .slave = Phone_Playback_Volume,
    .delay = 100 * 1000,
    .stepDown = 3,
    .stepUp = 2,
};

STATIC halVolRampT volRampNavigation = {
    .mode = RAMP_VOL_NORMAL,
    .slave = Navigation_Playback_Volume,
    .delay = 100 * 1000,
    .stepDown = 3,
    .stepUp = 2,
};

STATIC halVolRampT volRampRadio = {
    .mode = RAMP_VOL_SMOOTH,
    .slave = Radio_Playback_Volume,
    .delay = 100 * 1000,
    .stepDown = 1,
    .stepUp = 1,
};

STATIC halVolRampT volRampMultimedia = {
    .mode = RAMP_VOL_SMOOTH,
    .slave = Multimedia_Playback_Volume,
    .delay = 100 * 1000,
    .stepDown = 1,
    .stepUp = 1,
}
;

#define PCM_Master_Bass 1000
#define PCM_Master_Mid 1001
#define PCM_Master_Treble 1002

/* declare ALSA mixer controls */
STATIC alsaHalMapT alsaHalMap[] = {
    {.tag = Master_Playback_Volume, .ctl = {.name = "Master Playback Volume", .value = 100}},
    {.tag = PCM_Master_Bass, .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Master Bass Playback Volume", .minval = 0, .maxval = 100, .step = 1, .value = 50}},
    {.tag = PCM_Master_Mid, .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Master Midrange Playback Volume", .minval = 0, .maxval = 100, .step = 1, .value = 50}},
    {.tag = PCM_Master_Treble, .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Master Treble Playback Volume", .minval = 0, .maxval = 100, .step = 1, .value = 50}},

    {.tag = Navigation_Playback_Volume, .ctl = {.name = "Navigation Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 1, .minval = 183, .maxval = 255, .value = 50}},
    {.tag = Phone_Playback_Volume, .ctl = {.name = "Phone Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 1, .minval = 183, .maxval = 255, .value = 50}},
    {.tag = Radio_Playback_Volume, .ctl = {.name = "Radio Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 2, .minval = 183, .maxval = 255, .value = 50}},
    {.tag = Multimedia_Playback_Volume, .ctl = {.name = "Multimedia Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 2, .minval = 183, .maxval = 255, .value = 50}},

    {.tag = Navigation_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampNavigation}, .info = "RampUp Navigation Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Navigation_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},
    {.tag = Phone_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampPhone}, .info = "RampUp Phone Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Phone_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},
    {.tag = Radio_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampRadio}, .info = "RampUp Radio Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Radio_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},
    {.tag = Multimedia_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampMultimedia}, .info = "RampUp Multimedia Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Multimedia_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},

    {.tag = EndHalCrlTag} /* marker for end of the array */
};

/* HAL sound card mapping info */
STATIC alsaHalSndCardT alsaHalSndCard = {
    .name = ALSA_CARD_NAME, /*  WARNING: name MUST match with 'aplay -l' */
    .info = "HAL for FD-DSP sound card controlled by ADSP binding+plugin",
    .ctls = alsaHalMap,
    .volumeCB = NULL, /* use default volume normalization function */
};

/* initializes ALSA sound card using hal plugin API */
STATIC int hal_generic_init()
{
    int err = 0;
    json_object *configJ;

    AFB_NOTICE("Initializing 4a-hal-generic");

    // Get the config file.
    configJ = loadHalConfig();

    // Check that hal plugin is present. If so, we can initialize it.
    err = afb_daemon_require_api("fd-dsp-hifi2", 1);
    if (err)
    {
        AFB_ERROR("Failed to access fd-dsp-hifi2 API");
        goto OnErrorExit;
    }

    // If the hal plugin is present, we need to configure our custom sound card to provide all the required interfaces.
    // This will load the configuration json file, and set up the sound card
    AFB_NOTICE("Start Initialize of sound card");
    err = initialize_sound_card(configJ);
    if (err)
    {
        AFB_ERROR("Initializing Sound Card failed!");
        goto OnErrorExit;
    }

    AFB_NOTICE("Start halServiceInit section");
    err = halServiceInit(afbBindingV2.api, &alsaHalSndCard);
    if (err)
    {
        AFB_ERROR("Cannot initialize ALSA soundcard.");
        goto OnErrorExit;
    }

    AFB_NOTICE(".. Initializing Complete!");
OnErrorExit:
    AFB_NOTICE("hal_generic_init() - end");
    return err;
}

// This receive all event this binding subscribe to
PUBLIC void hal_generic_event_cb(const char *evtname, json_object *j_event)
{
    AFB_NOTICE("hal_generic_event_cb: Event Received (%s)", evtname);
    if (strncmp(evtname, "alsacore/", 9) == 0)
    {
        halServiceEvent(evtname, j_event);
        return;
    }

    if (strncmp(evtname, "fd-dsp-hifi2/", 13) == 0)
    {
        AFB_NOTICE("hal_generic_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));

        return;
    }

    if (strncmp(evtname, "hal-fddsp/", 6) == 0)
    {
        AFB_NOTICE("hal_generic_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));

        return;
    }

    AFB_NOTICE("hal_generic_event_cb: UNHANDLED EVENT, evtname=%s, event=%s", evtname, json_object_get_string(j_event));
}

/* API prefix should be unique for each snd card */
PUBLIC const struct afb_binding_v2 afbBindingV2 = {
    .api = "4a-hal-generic",
    .init = hal_generic_init,
    .verbs = halServiceApi,
    .onevent = hal_generic_event_cb,
};