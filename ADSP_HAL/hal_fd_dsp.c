#define _GNU_SOURCE
#include <string.h>
#include "hal-interface.h"
#include "wrap-json.h"

#define ALSA_CARD_NAME    "HDA Intel PCH"
#define ALSA_DEVICE_ID    "HDMI 0"
#define PCM_MAX_CHANNELS  6

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

static int master_volume = 80;
static json_bool master_switch;
static int pcm_volume[PCM_MAX_CHANNELS] = {100,100,100,100,100,100};

void fddsp_master_vol_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void* handle,  json_object *j_obj) {

    const char *j_str = json_object_to_json_string(j_obj);

    if (wrap_json_unpack(j_obj, "[i!]", &master_volume) == 0) {
        AFB_NOTICE("master_volume: %s, value=%d", j_str, master_volume);
        //wrap_volume_master(master_volume);
    }
    else {
        AFB_NOTICE("master_volume: INVALID STRING %s", j_str);
    }
}

void fddsp_master_switch_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void* handle,  json_object *j_obj) {

    const char *j_str = json_object_to_json_string(j_obj);

    if (wrap_json_unpack(j_obj, "[b!]", &master_switch) == 0) {
        AFB_NOTICE("master_switch: %s, value=%d", j_str, master_switch);
    }
    else {
        AFB_NOTICE("master_switch: INVALID STRING %s", j_str);
    }
}

void fddsp_pcm_vol_cb(halCtlsTagT tag, alsaHalCtlMapT *control, void* handle,  json_object *j_obj) {

    const char *j_str = json_object_to_json_string(j_obj);

    if (wrap_json_unpack(j_obj, "[iiiiii!]", &pcm_volume[0], &pcm_volume[1], &pcm_volume[2], &pcm_volume[3],
                                             &pcm_volume[4], &pcm_volume[5]) == 0) {
        AFB_NOTICE("pcm_vol: %s", j_str);
        //wrap_volume_pcm(pcm_volume, PCM_MAX_CHANNELS/*array size*/);
    }
    else {
        AFB_NOTICE("pcm_vol: INVALID STRING %s", j_str);
    }
}

/* declare ALSA mixer controls */
STATIC alsaHalMapT  alsaHalMap[]= {
  { .tag=Master_Playback_Volume, .cb={.callback=fddsp_master_vol_cb, .handle=&master_volume}, .info="Sets master playback volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER, .count=1, .minval=0, .maxval=100, .step=1, .value=80, .name="Master Playback Volume"}
  },
  /*{ .tag=Master_OnOff_Switch, .cb={.callback=unicens_master_switch_cb, .handle=&master_switch}, .info="Sets master playback switch",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_BOOLEAN, .count=1, .minval=0, .maxval=1, .step=1, .value=1, .name="Master Playback Switch"}
  },*/
  { .tag=PCM_Playback_Volume, .cb={.callback=fddsp_pcm_vol_cb, .handle=&pcm_volume}, .info="Sets PCM playback volume",
    .ctl={.numid=CTL_AUTO, .type=SND_CTL_ELEM_TYPE_INTEGER, .count=6, .minval=0, .maxval=100, .step=1, .value=100, .name="PCM Playback Volume"}
  },

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
  { .tag=EndHalCrlTag}  /* marker for end of the array */
} ;

/* HAL sound card mapping info */
STATIC alsaHalSndCardT alsaHalSndCard  = {
    .name  = ALSA_CARD_NAME,   /*  WARNING: name MUST match with 'aplay -l' */
    .info  = "HAL for FD-DSP sound card controlled by ADSP binding+plugin",
    .ctls  = alsaHalMap,
    .volumeCB = NULL,               /* use default volume normalization function */
};

/* initializes ALSA sound card, FDDSP API */
STATIC int fddsp_service_init() {
    int err = 0;
    AFB_NOTICE("Initializing HAL-FDDSP");

    err = halServiceInit(afbBindingV2.api, &alsaHalSndCard);
    if (err) {
        AFB_ERROR("Cannot initialize ALSA soundcard.");
        goto OnErrorExit;
    }

/*
    err= afb_daemon_require_api("FDDSP", 1);
    if (err) {
        AFB_ERROR("Failed to access FDDSP API");
        goto OnErrorExit;
    }
*/

OnErrorExit:
    AFB_NOTICE("Initializing HAL-FDDSP done..");
    return err;
}

// This receive all event this binding subscribe to
PUBLIC void fddsp_event_cb(const char *evtname, json_object *j_event) {

    if (strncmp(evtname, "alsacore/", 9) == 0) {
        halServiceEvent(evtname, j_event);
        return;
    }

    if (strncmp(evtname, "FDDSP/", 8) == 0) {
        AFB_NOTICE("fddsp_event_cb: evtname=%s, event=%s", evtname, json_object_get_string(j_event));
        
        return;
    }

    AFB_NOTICE("fddsp_event_cb: UNHANDLED EVENT, evtname=%s, event=%s", evtname, json_object_get_string(j_event));
}

/* API prefix should be unique for each snd card */
PUBLIC const struct afb_binding_v2 afbBindingV2 = {
    .api     = "hal-fddsp",
    .init    = fddsp_service_init,
    .verbs   = halServiceApi,
    .onevent = fddsp_event_cb,
};