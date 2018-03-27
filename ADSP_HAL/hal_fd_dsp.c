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
#include "dsphal_utility.h"
#include "hal-interface.h"
#include "wrap-json.h"



#define ALSA_CARD_NAME "xfalsa"
//#define ALSA_DEVICE_ID "ALC887-VD Analog"

/* Default Values for MasterVolume Ramping */
STATIC halVolRampT volRampMaster = {
    .mode = RAMP_VOL_NORMAL,
    .slave = Master_Playback_Volume,
    .delay = 100 * 1000, // ramping delay in us
    .stepDown = 1,
    .stepUp = 1,
};

STATIC halVolRampT volRampPhone = {
    .slave = Phone_Playback_Volume,
    .delay = 50 * 1000, // ramping delay in us
    .stepDown = 2,
    .stepUp = 4,
};

STATIC halVolRampT volRampNavigation = {
    .slave = Navigation_Playback_Volume,
    .delay = 50 * 1000, // ramping delay in us
    .stepDown = 2,
    .stepUp = 4,
};

STATIC halVolRampT volRampRadio = {
    .slave = Radio_Playback_Volume,
    .delay = 50 * 1000, // ramping delay in us
    .stepDown = 2,
    .stepUp = 4,
};

/* declare ALSA mixer controls */
STATIC alsaHalMapT alsaHalMap[] = {
    {.tag = Master_Playback_Volume, .ctl = {.name = "Master Playback Volume", .value = 100}},

    {.tag = Navigation_Playback_Volume, .ctl = {.name = "Navigation Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 1, .minval = 183, .maxval = 255, .value = 50}},
    {.tag = Phone_Playback_Volume, .ctl = {.name = "Phone Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 1, .minval = 183, .maxval = 255, .value = 50}},
    {.tag = Radio_Playback_Volume, .ctl = {.name = "Radio Playback Volume", .numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 2, .minval = 183, .maxval = 255, .value = 50}},

    {.tag = Navigation_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampNavigation}, .info = "RampUp Navigation Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Navigation_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},
    {.tag = Phone_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampPhone}, .info = "RampUp Phone Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Phone_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},
    {.tag = Radio_Playback_Ramp, .cb = {.callback = volumeRamp, .handle = &volRampRadio}, .info = "RampUp Radio Volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .name = "Radio_Ramp", .minval = 0, .maxval = 100, .step = 1, .value = 80}},

    {.tag = EndHalCrlTag} /* marker for end of the array */
};

/* HAL sound card mapping info */
STATIC alsaHalSndCardT alsaHalSndCard = {
    .name = ALSA_CARD_NAME, /*  WARNING: name MUST match with 'aplay -l' */
    .info = "HAL for FD-DSP sound card controlled by ADSP binding+plugin",
    .ctls = alsaHalMap,
    .volumeCB = NULL, /* use default volume normalization function */
};

/* initializes ALSA sound card, FDDSP API */
STATIC int fddsp_service_init()
{
    int err = 0;
    json_object *configJ;

    AFB_NOTICE("Initializing HAL-FDDSP");

    // Get the config file.
    configJ = loadHalConfig();

    // Check that our plugin is present. If so, we can initialize it.
    err = afb_daemon_require_api("fd-dsp-hifi2", 1);
    if (err)
    {
        AFB_ERROR("Failed to access fd-dsp-hifi2 API");
        goto OnErrorExit;
    }

    AFB_NOTICE("Start Initialize of sound card");
    err = initialize_sound_card(configJ);
    if(err)
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
    AFB_NOTICE("fddsp_service_init() - end");
    return err;
}

// This receive all event this binding subscribe to
PUBLIC void fddsp_event_cb(const char *evtname, json_object *j_event)
{
    AFB_NOTICE("fddsp_event_cb: Event Received (%s)", evtname);
    if (strncmp(evtname, "alsacore/", 9) == 0)
    {
        halServiceEvent(evtname, j_event);
        return;
    }

    if (strncmp(evtname, "fd-dsp-hifi2/", 13) == 0)
    {
        AFB_NOTICE("fddsp_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));

        return;
    }

    if (strncmp(evtname, "hal-fddsp/", 6) == 0)
    {
        AFB_NOTICE("fddsp_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));

        return;
    }

    AFB_NOTICE("fddsp_event_cb: UNHANDLED EVENT, evtname=%s, event=%s", evtname, json_object_get_string(j_event));
}

/* API prefix should be unique for each snd card */
PUBLIC const struct afb_binding_v2 afbBindingV2 = {
    .api = "hal-fddsp",
    .init = fddsp_service_init,
    .verbs = halServiceApi,
    .onevent = fddsp_event_cb,
};