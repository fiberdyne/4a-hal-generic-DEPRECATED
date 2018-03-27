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

#define MAX_FILENAME_LEN 255
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

    initialize_sound_card(configJ);

#if 1
    AFB_NOTICE("Start halServiceInit section");
    err = halServiceInit(afbBindingV2.api, &alsaHalSndCard);
    if (err)
    {
        AFB_ERROR("Cannot initialize ALSA soundcard.");
        goto OnErrorExit;
    }
#endif

OnErrorExit:
    AFB_NOTICE("Initializing HAL-FDDSP done..");
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

    if (strncmp(evtname, "FDDSP/", 6) == 0)
    {
        AFB_NOTICE("fddsp_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));

        return;
    }

    AFB_NOTICE("fddsp_event_cb: UNHANDLED EVENT, evtname=%s, event=%s", evtname, json_object_get_string(j_event));
}

static void initialize_sound_card_cb(void *closure, int status, struct json_object *j_response)
{
    int err;
    json_object *j_obj;

    int cfgErrCode;
    const char *cfgErrMessage;

    AFB_NOTICE("initialize_sound_card_cb: closure=%p status=%d, res=%s", closure, status, json_object_to_json_string(j_response));

    if (json_object_object_get_ex(j_response, "response", &j_obj))
    {
        //wrap_json_optarray_for_all(j_obj, &unicens_apply_card_value, NULL);

        err = wrap_json_unpack(j_obj, "{s:i,s:s}", "errcode", &cfgErrCode, "message", &cfgErrMessage);

        if (err)
        {
            AFB_ERROR("Soundcard Init Fail: %s", cfgErrMessage);
            goto OnErrorExit;
        }
    }

    AFB_INFO("Soundcard has been initialized, now initializing the Hal");

OnErrorExit:
    return;
}

void initialize_sound_card(json_object *configJ)
{
    json_object *configurationStringJ = NULL;
    json_object *cfgResultJ = NULL;

    /*
    int err;
    err = wrap_json_pack(&j_query, "{s:s, s:i}", "devid", dev_id, "mode", 0);

    if (err) {
        AFB_ERROR("Failed to call wrap_json_pack");
        goto OnErrorExit;
    }
*/
    configurationStringJ = getConfigurationString(configJ);

    AFB_NOTICE("Configuration String recieved, sending to sound card");
    
    #if 0
    afb_service_call("fd-dsp-hifi2", "initialize_sndcard", configurationStringJ,
                     &initialize_sound_card_cb,
                     NULL);
    #else
    afb_service_call_sync("fd-dsp-hifi2", "initialize_sndcard", configurationStringJ, &cfgResultJ);
    AFB_NOTICE("result: %s", json_object_get_string(cfgResultJ));
    #endif

    return;
}

json_object *loadHalConfig(void)
{
    char filename[MAX_FILENAME_LEN];
    struct stat st;
    int fd;
    json_object *config = NULL;
    char *configJsonStr;
    char *bindingDirPath = GetBindingDirPath();
    
    AFB_NOTICE("Binding path: %s", bindingDirPath);
#if 0
    int index;
    const char *_fullpath = "";
    const char *_filename = "";
    json_object *_jsonObj = ScanForConfig(bindingDirPath, CTL_SCAN_RECURSIVE, "onload-", ".json");

    if(_jsonObj)
        AFB_NOTICE("config files found at: %s", json_object_get_string(_jsonObj));
    else
        AFB_ERROR("Config file was not found!");

    enum json_type jtype = json_object_get_type(_jsonObj);
    switch (jtype)
    {
    case json_type_array:
        //AFB_NOTICE("Json config object is an Array!");
        for (index = 0; index < json_object_array_length(_jsonObj); index++)
        {
            json_object *tmpJ = json_object_array_get_idx(_jsonObj, index);
            //AFB_NOTICE("Getting object: %s",json_object_get_string(tmpJ));

            if(tmpJ)
            {
                wrap_json_unpack(tmpJ, "{s:s,s:s}", "fullpath",&_fullpath, "filename",&_filename);
                //AFB_NOTICE("%s/%s",_fullpath,_filename);
                snprintf(filename, MAX_FILENAME_LEN, "%s/%s", _fullpath, _filename);
                AFB_NOTICE("fullpath: %s", filename);
            }
        }
        break;
    default:
        AFB_NOTICE("Json config object is NOT an Array");
    }
#else
    snprintf(filename, MAX_FILENAME_LEN, "%s/var/%s", bindingDirPath, "onload-config-0001.json");
    AFB_NOTICE("path: %s", filename);
#endif

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        perror("open");
    if (fstat(fd, &st) == -1)
        perror("fstat");
    configJsonStr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    config = json_tokener_parse(configJsonStr);

    return config;
}

/* API prefix should be unique for each snd card */
PUBLIC const struct afb_binding_v2 afbBindingV2 = {
    .api = "hal-fddsp",
    .init = fddsp_service_init,
    .verbs = halServiceApi,
    .onevent = fddsp_event_cb,
};