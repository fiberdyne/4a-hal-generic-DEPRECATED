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
#include "hal-generic-utility.h"
#include "hal-generic-validate.h"
#include "ctl-config.h"


/*****************************************************************************
 * Definitions
 ****************************************************************************/
//#define ALSA_DEVICE_ID "ALC887-VD Analog"

#define PCM_Master_Bass 1000
#define PCM_Master_Mid 1001
#define PCM_Master_Treble 1002


/*****************************************************************************
 * Local Function Declarations
 ****************************************************************************/
STATIC int hal_generic_preinit();
STATIC int hal_generic_init();
STATIC void hal_generic_event_cb(const char *evtname, json_object *j_event);

STATIC int CardConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *cardsJ);
STATIC int StreamConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *streamsJ);
STATIC int ZoneConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *zonesJ);
STATIC int CtlConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *ctlsJ);


/*****************************************************************************
 * Local Variable Declarations
 ****************************************************************************/
static json_object *_cardsJ = NULL;     // Cards JSON section from conf file
static json_object *_streamsJ = NULL;   // Streams JSON section from conf file
static json_object *_zonesJ = NULL;     // Zones JSON section from conf file
static json_object *_ctlsJ = NULL;      // Ctls JSON section from conf file

static alsaHalSndCardT alsaHalSndCard;  // alsaHalSndCard for alsacore

/* API prefix should be unique for each snd card */
const struct afb_binding_v2 afbBindingV2 = {
  .api = "4a-hal-generic",
  .preinit = hal_generic_preinit,
  .init = hal_generic_init,
  .verbs = halServiceApi,
  .onevent = hal_generic_event_cb,
};

// Config Section definition (note: controls section index should match handle retrieval in HalConfigExec)
// NOTE: This order must NOT change!
static CtlSectionT ctrlSections[]= {
    {.key="cards"  , .loadCB= CardConfig},
    {.key="zones"  , .loadCB= ZoneConfig},
    {.key="streams", .loadCB= StreamConfig},
    {.key="ctls"   , .loadCB= CtlConfig},

    {.key=NULL}
};


/*****************************************************************************
 * Local Function Definitions (App Controller CB)
 ****************************************************************************/
/*
 * @brief 'cards' section callback for app controller
 */
STATIC int CardConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *cardsJ)
{
  HAL_ERRCODE err = HAL_OK;
  
  err = validateCards(cardsJ); // Cards OK?
  if (err == HAL_OK)
    _cardsJ = cardsJ;

  return (int)err;
}

/*
 * @brief 'streams' section callback for app controller
 *         Must come AFTER zone config!!
 */
STATIC int StreamConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *streamsJ)
{
  HAL_ERRCODE err = HAL_FAIL;

  if (_zonesJ) // zones OK?
  {
    err = validateStreams(streamsJ, _zonesJ); // streams OK?
    if (err == HAL_OK)
      _streamsJ = streamsJ;
  }

  return (int)err;
}

/*
 * @brief 'zones' section callback for app controller
 */
STATIC int ZoneConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *zonesJ)
{
  HAL_ERRCODE err = HAL_FAIL;

  err = validateZones(zonesJ);
  if (err == HAL_OK)
    _zonesJ = zonesJ;

  return (int)err;
}

/*
 * @brief 'ctls' section callback for app controller
 */
STATIC int CtlConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *ctlsJ)
{
  HAL_ERRCODE err = HAL_FAIL;

  if (_streamsJ) // streams OK?
  {
    err = validateCtls(ctlsJ, _streamsJ);
    if (err == HAL_OK)
      _ctlsJ = ctlsJ;
  }

  return (int)err;
}


/*****************************************************************************
 * Local Function Definitions
 ****************************************************************************/
/*
 * @brief AFB 'preinit' callback function
 */
STATIC int hal_generic_preinit()
{
  //const char *dirList= getenv("CONTROL_CONFIG_PATH");
  //if (!dirList) dirList = CONTROL_CONFIG_PATH;
  char *dirList = GetBindingDirPath(NULL);
  strcat(dirList, "/etc");

  const char *configPath = CtlConfigSearch(NULL, dirList, "config");
  if (!configPath)
  {
    AFB_ApiError(apiHandle, "CtlPreInit: No config-%s-* config found in %s ", GetBinderName(), dirList);
    goto OnErrorExit;
  }

  // load config file and create API
  CtlConfigT *ctrlConfig = CtlLoadMetaData(NULL, configPath);
  if (!ctrlConfig)
  {
    AFB_ApiError(apiHandle, "CtrlBindingDyn No valid control config file in:\n-- %s", configPath);
    goto OnErrorExit;
  }
  
  if (ctrlConfig->api)
  {
    int err = afb_daemon_rename_api(ctrlConfig->api);
    if (err)
    {
      AFB_ApiError(apiHandle, "Fail to rename api to:%s", ctrlConfig->api);
      goto OnErrorExit;
    }
  }

  int err = CtlLoadSections(NULL, ctrlConfig, ctrlSections);
  return err;

OnErrorExit:
  return -1;
}

/*
 * @brief AFB 'init' callback function
 */
STATIC int hal_generic_init()
{
  int err = 0;
  json_object *streammapJ = NULL, *cardpropsJ = NULL;

  AFB_NOTICE("Initializing 4a-hal-generic");
  
  // Check that hal plugin is present. If so, we can initialize it.
    err = afb_daemon_require_api("fd-dsp-hifi2", 1);
    if (err)
    {
        AFB_ERROR("Failed to access fd-dsp-hifi2 API");
        goto OnErrorExit;
    }

  // If the hal plugin is present, we need to configure our custom sound card to provide all the required interfaces.
  // This will load the configuration json file, and set up the sound card  
  if (!_cardsJ || !_streamsJ || !_ctlsJ || !_zonesJ)
  {
    AFB_ApiError(NULL, "Cannot initialize HAL plugin: JSON config is not valid!");
    goto OnErrorExit;
  }

  cardpropsJ = generateCardProperties(json_object_array_get_idx(_cardsJ, 0),
                                      "xfalsa");
  streammapJ = generateStreamMap(_streamsJ, _zonesJ, "xfalsa");

  AFB_NOTICE("Start Initialize of sound card");
  err = initialize_sound_card(cardpropsJ, streammapJ);
  if (err)
  {
    AFB_ApiError(NULL, "Initializing Sound Card failed!");
    goto OnErrorExit;
  }

    /* HAL sound card mapping info */
  alsaHalSndCard.name = "xfalsa"; /*  WARNING: name MUST match with 'aplay -l' */
  alsaHalSndCard.info = "HAL for FD-DSP sound card controlled by ADSP binding+plugin";
  alsaHalSndCard.ctls = generateAlsaHalMap(_ctlsJ);
  alsaHalSndCard.volumeCB = NULL; /* use default volume normalization function */

  err = halServiceInit(afbBindingV2.api, &alsaHalSndCard);
  if (err)
  {
    AFB_ApiError(NULL, "Cannot initialize ALSA soundcard.");
    goto OnErrorExit;
  }

  AFB_NOTICE(".. Initializing Complete!");
  return err;

OnErrorExit:
  AFB_ApiError(NULL, "hal_generic_init() - end");
  return err;
}

// This receive all event this binding subscribe to
STATIC void hal_generic_event_cb(const char *evtname, json_object *j_event)
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
