/*
 * Copyright (C) 2018 Fiberdyne Systems
 * 
 * Author: James O'Shannessy <james.oshannessy@fiberdyne.com.au>
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


#include "hal-generic-utility.h"
#include "wrap-json.h"

#include <stdbool.h>


/*****************************************************************************
 * Definitions
 ****************************************************************************/
typedef enum  {
  Volume,
  Ramp,
  Switch,
  Bass,
  Mid,
  Treble,
  Fade,
  Balance
} halCtlsTypeT;

const char *halCtlsTypeLabels[] = {
  [Volume] = "Volume",
  [Ramp] = "Ramp",
  [Switch] = "Switch",
  [Bass] = "Bass",
  [Mid] = "Mid",
  [Treble] = "Treble",
  [Fade] = "Fade",
  [Balance] = "Balance"
};


/*****************************************************************************
 * Local Function Declarations
 ****************************************************************************/
PUBLIC STATIC json_object *getZoneMap(json_object *zonesJ, const char *zoneName);
PUBLIC STATIC halCtlsTagT getHalCtlsTag(const char *role, halCtlsTypeT ctlsType);
PUBLIC STATIC bool generateAlsaHalMapCtl(json_object *ctlJ,
                                         const char *ctlStream,
                                         halCtlsTypeT ctlType,
                                         alsaHalMapT *alsaHalMap);



/*****************************************************************************
 * Local Function Definitions
 ****************************************************************************/
/*
 * @brief Get the stream mapping according to the zone it belongs to
 */
PUBLIC STATIC json_object *getZoneMap(json_object *zonesJ, const char *zoneName)
{
  json_object *resultJ = NULL, *zoneMappingJ = NULL;
  
  resultJ = json_object_array_find(zonesJ, "uid", zoneName);
  if (resultJ)
  {
    wrap_json_unpack(resultJ, "{s?o}",
                     "mapping", &zoneMappingJ);
    return zoneMappingJ;
  }

  AFB_ApiError(NULL, "STREAM: Zone '%s' is not defined!", zoneName);
  return NULL;
}

/*
 * @brief Get the halCtlsTagT for a given audio role, and ctl type
 */
PUBLIC STATIC halCtlsTagT getHalCtlsTag(const char *role, halCtlsTypeT type)
{
  int i = 1;
  char ctlLabel[255];

  while (halCtlsLabels[i] != NULL)
  {
    strcpy(ctlLabel, halCtlsLabels[i]);
    const char *ctlRole = strtok(ctlLabel, "_");
    const char *ctlType = strtok(NULL, "_");
    if (!strcmp(ctlRole, "Master"))
      ctlType = strtok(NULL, "_");

    if (!strcmp(ctlRole, role))
    {
      if (!strcmp(ctlType, halCtlsTypeLabels[type]))
        return (halCtlsTagT)i;
    }

    i++;
  }

  return EndHalCrlTag;
}


/*****************************************************************************
 * Global Function Definitions
 ****************************************************************************/
/*
 * @brief Find a JSON object in a JSON array whose value matches
 * it's own value for a given key
 */
json_object *json_object_array_find(json_object *arrayJ,
                                    const char *key,
                                    const char *value)
{
  int idx = 0;
  int len = json_object_array_length(arrayJ);

  for (idx = 0; idx < len; idx++)
  {
    char *val = NULL;
    json_object *currJ = NULL;

    currJ = json_object_array_get_idx(arrayJ, idx);
    wrap_json_unpack(currJ, "{s?s}",
                     key, &val);
    if (val)
      if (strcmp(value, val) == 0)
        return currJ;
  }

  return NULL;
}

HAL_ERRCODE initialize_sound_card(json_object *cardpropsJ,
                                  json_object *streammapJ)
{
  json_object *cfgJ = NULL;
  json_object *cfgResultJ = NULL;
  json_object *resultJ = NULL;
  int result = -1;
  const char *message;

  AFB_ApiNotice(NULL, "Configuration OK, sending to HAL plugin");
  wrap_json_pack(&cfgJ, "{s:o,s:o}",
                 "cardprops", cardpropsJ,
                 "streammap", streammapJ);

  // printf("jobj from str:\n---\n%s\n---\n", json_object_to_json_string_ext(cfgJ, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));

  result = afb_service_call_sync("fd-dsp-hifi2", "initialize_sndcard", cfgJ, &cfgResultJ);
  AFB_NOTICE("Result Code: %d, result: %s", result, json_object_get_string(cfgResultJ));

  if (result)
  {
    // Error code was returned, return fail to afb init
    const char *strResult;
    const char *strStatus;
    int errCode = -1;
    wrap_json_unpack(cfgResultJ, "{s:{s:s,s:s}}", "request", "status", &strStatus, "info", &strResult);
    resultJ = json_tokener_parse(strResult);
    wrap_json_unpack(resultJ, "{s:i,s:s}", "errcode", &errCode, "message", &message);
    AFB_NOTICE("Status: %s, Message: %s, ErrCode: %d", strStatus, message, errCode);

    // Break out and fail here.
    return HAL_FAIL;
  }
  else
  {
    int errCode = -1;
    wrap_json_unpack(cfgResultJ, "{s:{s:i,s:s}}",
                     "response", "errcode", &errCode, "message", &message);
    AFB_NOTICE("Message: %s, ErrCode: %d", message, result);
  }

  // No error found, return success to afb init
  return HAL_OK;
}

PUBLIC alsaHalMapT *generateAlsaHalMap(json_object *ctlsJ)
{
  int ctlsLength = json_object_array_length(ctlsJ);
  size_t ctlsAlloc = sizeof(alsaHalMapT) * ctlsLength * 8;
  int ctlsIdx = 0, halMapIdx = 0;
  alsaHalMapT *alsaHalMap = (alsaHalMapT *)malloc(ctlsAlloc);
  memset(alsaHalMap, 0, ctlsAlloc);

  if (!alsaHalMap)
  {
    AFB_ApiError(NULL, "HALMAP: Cannot allocate memory (sz: %ld)", ctlsAlloc);
    return NULL;
  }

  for (ctlsIdx = 0; ctlsIdx < ctlsLength; ctlsIdx++)
  {
    char *ctlUid = NULL, *ctlStream = NULL;
    json_object *ctlCurrJ = NULL, *ctlVolumeJ = NULL, *ctlVolrampJ = NULL,
                *ctlBassJ = NULL, *ctlMidJ = NULL, *ctlTrebleJ = NULL,
                *ctlFadeJ = NULL, *ctlBalanceJ = NULL;

    ctlCurrJ = json_object_array_get_idx(ctlsJ, ctlsIdx);
    wrap_json_unpack(ctlCurrJ, "{s:s,s:s,s:o,s?o,s?o,s?o,s?o,s?o,s?o}",
                     "uid", &ctlUid, "stream", &ctlStream,
                     "volume", &ctlVolumeJ, "volramp", &ctlVolrampJ,
                     "bass", &ctlBassJ, "mid", &ctlMidJ, "treble", &ctlTrebleJ,
                     "fade", &ctlFadeJ, "balance", &ctlBalanceJ);

    if (generateAlsaHalMapCtl(ctlVolumeJ, ctlStream, Volume, &alsaHalMap[halMapIdx]))
      halMapIdx++;
    if (ctlVolrampJ)
      if (generateAlsaHalMapCtl(ctlVolrampJ, ctlStream, Ramp, &alsaHalMap[halMapIdx]))
        halMapIdx++;
    if (ctlBassJ)
      if (generateAlsaHalMapCtl(ctlBassJ, ctlStream, Bass, &alsaHalMap[halMapIdx]))
        halMapIdx++;
    if (ctlMidJ)
      if (generateAlsaHalMapCtl(ctlMidJ, ctlStream, Mid, &alsaHalMap[halMapIdx]))
        halMapIdx++;
    if (ctlTrebleJ)
      if (generateAlsaHalMapCtl(ctlTrebleJ, ctlStream, Treble, &alsaHalMap[halMapIdx]))
        halMapIdx++;
    if (ctlFadeJ)
      if (generateAlsaHalMapCtl(ctlFadeJ, ctlStream, Fade, &alsaHalMap[halMapIdx]))
        halMapIdx++;
    if (ctlBalanceJ)
      if (generateAlsaHalMapCtl(ctlBalanceJ, ctlStream, Balance, &alsaHalMap[halMapIdx]))
        halMapIdx++;
  }

  AFB_ApiNotice(NULL, "ctlsIdx: %d, halMapIdx: %d:", ctlsIdx, halMapIdx);

  return alsaHalMap;
}

PUBLIC STATIC bool generateAlsaHalMapCtl(json_object *ctlJ,
                                         const char *ctlStream,
                                         halCtlsTypeT ctlType,
                                         alsaHalMapT *alsaHalMap)
{
  // Get halCtlsTagT for ctlType
  halCtlsTagT tag = getHalCtlsTag(ctlStream, ctlType);
  if (tag == EndHalCrlTag)
  {
    AFB_ApiError(NULL, "HALMAP: Cannot get HAL ctl tag for role: %s, type: %s",
                 ctlStream, halCtlsTypeLabels[ctlType]);
    return false;
  }

  // Set to defaults
  char *ctlName = NULL, *ctlCb = NULL;
  int ctlValue = 50, ctlMinval = 0, ctlMaxval = 100,
      ctlCount = 1, ctlStep = 1;

  // Unpack the control
  wrap_json_unpack(ctlJ, "{s:s,s?s,s?i,s?i,s?i}",
                   "name", &ctlName, "cb", &ctlCb, "value", &ctlValue,
                   "minval", &ctlMinval, "maxval", &ctlMaxval,
                   "count", &ctlStep, "step", &ctlStep);

  // Set up the halmap
  alsaHalMap->tag = tag;
  alsaHalMap->ctl.name = ctlName;
  alsaHalMap->ctl.numid = CTL_AUTO;
  alsaHalMap->ctl.type = SND_CTL_ELEM_TYPE_INTEGER;
  alsaHalMap->ctl.value = ctlValue;
  alsaHalMap->ctl.minval = ctlMinval;
  alsaHalMap->ctl.maxval = ctlMaxval;

  if (ctlType == Volume)
    alsaHalMap->ctl.count = ctlCount;

  if (ctlType == Ramp)
  {
    alsaHalMap->ctl.step = ctlStep;
    alsaHalMap->cb.callback = volumeRamp;
    halVolRampT halVolRamp = {
      .mode = RAMP_VOL_SMOOTH,
      .slave = (halCtlsTagT)(tag - 1),
      .delay = 100 * 1000,
      .stepDown = 1,
      .stepUp = 1,
    };
    alsaHalMap->cb.handle = &halVolRamp;
  }

  alsaHalMap->ctl.enums = NULL;

  return true;
}

/*
 * @brief Generate a 'cardprops' object from a 'card' config
 * @param cardsJ : A json_object containing an a card object
 * @return A json_object containing a 'cardprops' object
 */
PUBLIC json_object *generateCardProperties(json_object *cardJ,
                                           const char *cardname)
{
  AFB_ApiNotice(NULL, "CARD: Generating 'cardprops' for card: %s", cardname);

  json_object *cardChannelsJ = NULL;

  wrap_json_unpack(cardJ, "{s:o}", "channels", &cardChannelsJ);
    
  return cardChannelsJ;
}

/*
 * @brief Generate a 'streammap' object from an array of streams and zones
 * @param streamsJ : A json_object containing an array of streams
 * @param zonesJ   : A json_object containing an array of zones
 * @return A json_object containing a 'steammap' object
 */
PUBLIC json_object *generateStreamMap(json_object *streamsJ,
                                      json_object *zonesJ,
                                      const char *cardname)
{
  int streamsIdx = 0;
  int streamsLength = json_object_array_length(streamsJ);
  json_object *streamMapArrayJ = NULL;

  AFB_ApiNotice(NULL, "STREAM: Generating 'streammap' for card: %s:", cardname);

  wrap_json_pack(&streamMapArrayJ, "[]");

  for (streamsIdx = 0; streamsIdx < streamsLength; streamsIdx++)
  {
    json_object *streamMapJ = NULL, *sourceJ = NULL, *sinkJ = NULL,
                *sourceZoneMapJ = NULL, *sinkZoneMapJ = NULL,
                *streamSourceJ = NULL, *streamSinkJ = NULL, *currStreamJ = NULL;
    const char *streamName = NULL, *sourceProfile = NULL, *sinkProfile = NULL,
               *sourceZone = NULL, *sinkZone = NULL;
    int sinkChannels = 0, sourceChannels = 0;

    // Begin unpacking
    currStreamJ = json_object_array_get_idx(streamsJ, streamsIdx);
    wrap_json_unpack(currStreamJ, "{s:s,s?o,s?o}",
                     "role", &streamName, "source", &streamSourceJ,
                     "sink", &streamSinkJ);
    if (streamSourceJ)
      wrap_json_unpack(streamSourceJ, "{s?s,s:s}",
                       "profile", &sourceProfile, "zone", &sourceZone);
    if (streamSinkJ)
      wrap_json_unpack(streamSinkJ, "{s?s,s:s}",
                       "profile", &sinkProfile, "zone", &sinkZone);

    // Validate that the mapped zone exists, and get the zone's mapping
    if (sourceZone)
    {
      sourceZoneMapJ = getZoneMap(zonesJ, sourceZone);
      if (sourceZoneMapJ)
      {
        sourceChannels = json_object_array_length(sourceZoneMapJ);
        wrap_json_pack(&sourceJ, "{s:i,s:o}",
                       "channels", sourceChannels, "mapping", sourceZoneMapJ);
      }
    }
    if (sinkZone)
    {
      sinkZoneMapJ = getZoneMap(zonesJ, sinkZone);
      if (sinkZoneMapJ)
      {
        sinkChannels = json_object_array_length(sinkZoneMapJ);
        wrap_json_pack(&sinkJ, "{s:i,s:o}",
                       "channels", sinkChannels, "mapping", sinkZoneMapJ);
      }
    }
    
    wrap_json_pack(&streamMapJ, "{s:s,s:o*,s:o*}",
                   "stream", streamName, "source", sourceJ, "sink", sinkJ);

    json_object_array_add(streamMapArrayJ, streamMapJ);
  }

  return streamMapArrayJ;
}
