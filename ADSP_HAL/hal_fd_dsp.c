#define _GNU_SOURCE
#include <string.h>
#include "dsphal_utility.h"
#include "hal-interface.h"
#include "wrap-json.h"
#include "filescan-utils.h"
#include <sys/mman.h>
#include <sys/stat.h>

#if 1
#define ALSA_CARD_NAME "xfalsa"
#else
#define ALSA_CARD_NAME "HDA Intel PCH"
#define ALSA_DEVICE_ID "ALC887-VD Analog"
#endif
#define PCM_MAX_CHANNELS 6

json_object *loadHalConfig(void);
static void initialize_sound_card_cb(void *closure, int status, struct json_object *j_response);
void initialize_sound_card(json_object *cfgStrJ);

#if 0
/* Default Values for MasterVolume Ramping */
STATIC halVolRampT volRampMaster= {
    .mode    = RAMP_VOL_NORMAL,
    .slave   = Master_Playback_Volume,
    .delay   = 100*1000, // ramping delay in us
    .stepDown=1,
    .stepUp  =1,
};

/* Soft Volume Ramping Value can be customize by Audio Role and hardware card */
STATIC halVolRampT volRampEmergency= {
    .slave   = Emergency_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 10,
};

STATIC halVolRampT volRampWarning= {
    .slave   = Warning_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 10,
};

STATIC halVolRampT volRampCustomHigh= {
    .slave   = CustomHigh_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampPhone= {
    .slave   = Phone_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampNavigation= {
    .slave   = Navigation_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampCustomMedium= {
    .slave   = CustomMedium_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampVideo= {
    .slave   = Video_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampStreaming= {
    .slave   = Streaming_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampMultimedia= {
    .slave   = Multimedia_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampRadio= {
    .slave   = Radio_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampCustomLow= {
    .slave   = CustomLow_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};

STATIC halVolRampT volRampFallback= {
    .slave   = Fallback_Playback_Volume,
    .delay   = 50*1000, // ramping delay in us
    .stepDown= 2,
    .stepUp  = 4,
};
#endif

static int master_volume = 80;
static json_bool master_switch;
static int pcm_volume[PCM_MAX_CHANNELS] = {100, 100, 100, 100, 100, 100};
static int pcm_volume2[10] = {50, 50, 50, 50, 50, 50, 50, 50, 50, 50};

void fddsp_master_vol_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void *handle, json_object *j_obj)
{

    const char *j_str = json_object_to_json_string(j_obj);
    AFB_NOTICE("fddsp_master_vol_cb: Calling Master Volume Cb");
    if (wrap_json_unpack(j_obj, "[i!]", &master_volume) == 0)
    {
        AFB_NOTICE("master_volume: %s, value=%d", j_str, master_volume);
        //wrap_volume_master(master_volume);
    }
    else
    {
        AFB_NOTICE("master_volume: INVALID STRING %s", j_str);
    }
}

void fddsp_master_switch_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void *handle, json_object *j_obj)
{

    const char *j_str = json_object_to_json_string(j_obj);
    AFB_NOTICE("fddsp_master_switch_cb: Calling Master Switch Cb");
    if (wrap_json_unpack(j_obj, "[b!]", &master_switch) == 0)
    {
        AFB_NOTICE("master_switch: %s, value=%d", j_str, master_switch);
    }
    else
    {
        AFB_NOTICE("master_switch: INVALID STRING %s", j_str);
    }
}

void fddsp_pcm_vol_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void *handle, json_object *j_obj)
{

    const char *j_str = json_object_to_json_string(j_obj);
    AFB_NOTICE("fddsp_pcm_vol_cb: Calling PCM Volume Cb");
    if (wrap_json_unpack(j_obj, "[ii!]", &pcm_volume[0], &pcm_volume[1]) == 0)
    {
        AFB_NOTICE("pcm_vol: %s", j_str);
        //wrap_volume_pcm(pcm_volume, PCM_MAX_CHANNELS/*array size*/);
    }
    else
    {
        AFB_NOTICE("pcm_vol: INVALID STRING %s", j_str);
    }
}

void fddsp_pcm_vol_cb2(halCtlsTagT tag, alsaHalCtlMapT *control, void *handle, json_object *j_obj)
{

    const char *j_str = json_object_to_json_string(j_obj);
    AFB_NOTICE("fddsp_pcm_vol_cb: Calling PCM Volume Cb");
    if (wrap_json_unpack(j_obj, "[iiiiiiiiii!]", &pcm_volume2[0], &pcm_volume2[1],&pcm_volume2[2],&pcm_volume2[3],&pcm_volume2[4],&pcm_volume2[5],&pcm_volume2[6],&pcm_volume2[7],&pcm_volume2[8],&pcm_volume2[9] ) == 0)
    {
        AFB_NOTICE("pcm_vol: %s", j_str);
        //wrap_volume_pcm(pcm_volume, PCM_MAX_CHANNELS/*array size*/);
    }
    else
    {
        AFB_NOTICE("pcm_vol: INVALID STRING %s", j_str);
    }
}

/* declare ALSA mixer controls */
STATIC alsaHalMapT alsaHalMap[] = {

    {.tag = Master_Playback_Volume, .cb = {.callback = fddsp_master_vol_cb, .handle = &master_volume}, .info = "Sets master playback volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 1, .minval = 0, .maxval = 100, .step = 1, .value = 80, .name = "Master Playback Volume"}},
    /*
  { .tag=Master_OnOff_Switch, .cb={.callback=fddsp_master_switch_cb, .handle=&master_switch}, .info="Sets master playback switch",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER, .count=1, .minval=0, .maxval=1, .step=1, .value=1, .name="Master Playback Switch"}
  },
  */
    {.tag = PCM_Playback_Volume, .cb = {.callback = fddsp_pcm_vol_cb, .handle = &pcm_volume}, .info = "Sets PCM playback volume", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 6, .minval = 0, .maxval = 100, .step = 1, .value = 100, .name = "PCM Playback Volume"}},
    {.tag = PCM_Playback_Volume, .cb = {.callback = fddsp_pcm_vol_cb2, .handle = &pcm_volume2}, .info = "Sets PCM playback volume 2", .ctl = {.numid = CTL_AUTO, .type = SND_CTL_ELEM_TYPE_INTEGER, .count = 4, .minval = 0, .maxval = 100, .step = 1, .value = 100, .name = "PCM Playback Volume 2"}},
#if 0
  // Sound card does not have hardware volume ramping
  { .tag=Master_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampMaster}, .info="RampUp Master Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER, .name="Master_Ramp", .count=1, .minval=0, .maxval=100, .step=1, .value=80 }
  },

  // Implement Rampup Volume for Virtual Channels (0-100)
  { .tag=Emergency_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampEmergency}, .info="RampUp Emergency Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Emergency_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Warning_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampWarning}, .info="RampUp Warning Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Warning_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=CustomHigh_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampCustomHigh}, .info="RampUp CustomHigh Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="CustomHigh_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Phone_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampPhone}, .info="RampUp Phone Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Phone_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Navigation_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampNavigation}, .info="RampUp Navigation Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Navigation_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=CustomMedium_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampCustomMedium}, .info="RampUp CustomMedium Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="CustomMedium_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Video_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampVideo}, .info="RampUp Video Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Video_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Streaming_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampStreaming}, .info="RampUp Streaming Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Streaming_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Multimedia_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampMultimedia}, .info="RampUp Multimedia Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Multimedia_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Radio_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampRadio}, .info="RampUp Radio Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Radio_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=CustomLow_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampCustomLow}, .info="RampUp CustomLow Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="CustomLow_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },
  { .tag=Fallback_Playback_Ramp, .cb={.callback=volumeRamp, .handle=&volRampFallback}, .info="RampUp Fallback Volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.name="Fallback_Ramp", .minval=0, .maxval=100, .step=1, .value=80 }
  },

  // Bind with existing ones created by ALSA configuration (and linked to softvol) [0-255]
  { .tag=Emergency_Playback_Volume      ,
     .ctl={.name="Emergency_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Warning_Playback_Volume      ,
     .ctl={.name="Warning_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=CustomHigh_Playback_Volume      ,
     .ctl={.name="CustomHigh_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Phone_Playback_Volume      ,
     .ctl={.name="Phone_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Navigation_Playback_Volume      ,
     .ctl={.name="Navigation_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=CustomMedium_Playback_Volume      ,
     .ctl={.name="CustomMedium_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Video_Playback_Volume      ,
     .ctl={.name="Video_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Streaming_Playback_Volume      ,
     .ctl={.name="Streaming_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Multimedia_Playback_Volume      ,
     .ctl={.name="Multimedia_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Radio_Playback_Volume      ,
     .ctl={.name="Radio_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=CustomLow_Playback_Volume      ,
     .ctl={.name="CustomLow_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
  { .tag=Fallback_Playback_Volume      ,
     .ctl={.name="Fallback_Volume",.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER,.count=2, .maxval=255, .value=204 }
  },
#endif
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