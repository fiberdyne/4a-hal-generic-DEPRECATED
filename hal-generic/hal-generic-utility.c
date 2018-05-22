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
#include "hal-interface.h"
#include "wrap-json.h"

static json_object *_zonesJ = NULL;     // Zones JSON section from conf file
static json_object *_cardpropsJ = NULL; // Cardprops JSON config
static json_object *_streammapJ = NULL; // Streammap JSON config
static json_object *_ctlsJ = NULL;      // Ctls JSON config

// Local function declarations
PUBLIC STATIC json_object *generateCardProperties(json_object *cardsJ);
PUBLIC STATIC json_object *generateStreamMap(json_object *streamsJ, json_object *zonesJ);
PUBLIC STATIC json_object *getZoneMap(json_object *zonesJ, const char *zoneName);

PUBLIC STATIC HAL_ERRCODE validateZones(json_object *zonesJ);
PUBLIC STATIC HAL_ERRCODE validateStreams(json_object *streamsJ);
PUBLIC STATIC HAL_ERRCODE validateCtls(json_object *ctlsJ);
PUBLIC STATIC HAL_ERRCODE validateCtl(json_object *ctlJ, const char *ctlType);

PUBLIC STATIC json_object *json_object_array_find(json_object *arrayJ,
                                                  const char *key,
                                                  const char *value);


/////////////////////////////////////////////////////////////////////////
//  APP CONTROLLER SECTION CALLBACKS
/////////////////////////////////////////////////////////////////////////
/*
 * @brief 'cards' section callback for app controller
 */
int CardConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *cardsJ)
{
  int err = 0;
  
  AFB_NOTICE("Card Config");
  _cardpropsJ = generateCardProperties(cardsJ);

  return err;
}

/*
 * @brief 'streams' section callback for app controller
 *         Must come AFTER zone config!!
 */
int StreamConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *streamsJ)
{
  HAL_ERRCODE err = HAL_OK;

  if (_zonesJ) // zones OK?
  {
    err = validateStreams(streamsJ); // streams OK?
    if (err == HAL_OK)
      _streammapJ = generateStreamMap(streamsJ, _zonesJ);
  }

  return (int)err;
}

/*
 * @brief 'zones' section callback for app controller
 */
int ZoneConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *zonesJ)
{
  HAL_ERRCODE err = HAL_OK;

  err = validateZones(zonesJ);
  if (err == HAL_OK)
    _zonesJ = zonesJ;

  return (int)err;
}

/*
 * @brief 'ctls' section callback for app controller
 */
int CtlConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *ctlsJ)
{
  HAL_ERRCODE err = HAL_OK;

  err = validateCtls(ctlsJ);
  if (err == HAL_OK)
    _ctlsJ = ctlsJ;

  return (int)err;
}

/*
 * @brief Find a JSON object in a JSON array whose value matches
 * it's own value for a given key
 */
STATIC json_object *json_object_array_find(json_object *arrayJ,
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

/*
 * @brief Parse and validate all STREAM definitions
 */
HAL_ERRCODE validateStreams(json_object *streamsJ)
{
  int streamsIdx = 0;
  int streamsLength = json_object_array_length(streamsJ);

  // Loop through the known streams, and make sure all is well
  for (streamsIdx = 0; streamsIdx < streamsLength; streamsIdx++)
  {
    char *streamUid = NULL, *streamRole = NULL,
         *streamSinkZone = NULL, *streamSourceZone = NULL;
    json_object *streamCurrJ = NULL, *streamSinkJ = NULL,
                *streamSourceJ = NULL;

    streamCurrJ = json_object_array_get_idx(streamsJ, streamsIdx);
    wrap_json_unpack(streamCurrJ, "{s?s,s?s,s?o,s?o,s?o}",
                     "uid", &streamUid, "role", &streamRole,
                     "sink", &streamSinkJ, "source", &streamSourceJ);

    // Check that all required fields exist
    if (!streamUid || !streamRole || !streamSinkJ)
    {
      AFB_ApiError(NULL, "STREAM: Properties must include 'uid', 'role', and 'sink'!");
      return HAL_FAIL; 
    }

    // Check that the streamRole is always valid
    // TODO

    // Check the stream sink contains the field 'zone', and that the zone exists!
    wrap_json_unpack(streamSinkJ, "{s?s}",
                     "zone", &streamSinkZone);
    if (!streamSinkZone)
    {
      AFB_ApiError(NULL, "STREAM: Sink must have field 'zone'!");
      return HAL_FAIL;
    }
    else if (!getZoneMap(_zonesJ, streamSinkZone))
      return HAL_FAIL;

    // If stream source is defined, check it contains the field 'zone', and that the zone exists!
    if (streamSourceJ)
    {
      wrap_json_unpack(streamSinkJ, "{s?s}",
                      "zone", &streamSourceZone);
      if (!streamSourceZone)
      {
        AFB_ApiError(NULL, "STREAM: Source must have field 'zone'!");
        return HAL_FAIL;
      }
      else if (!getZoneMap(_zonesJ, streamSourceZone))
        return HAL_FAIL;
    }
  }

  return HAL_OK;
}

/*
 * @brief Parse and validate all CTL definitions
 */
HAL_ERRCODE validateCtls(json_object *ctlsJ)
{
  int ctlsIdx = 0;
  int ctlsLength = json_object_array_length(ctlsJ);

  // Loop through the ctls, and make sure all is well
  for (ctlsIdx = 0; ctlsIdx < ctlsLength; ctlsIdx++)
  {
    char *ctlUid = NULL, *ctlStream = NULL;
    json_object *ctlCurrJ = NULL, *ctlVolumeJ = NULL, *ctlVolrampJ = NULL,
                *ctlBassJ = NULL, *ctlMidJ = NULL, *ctlTrebleJ = NULL,
                *ctlFadeJ = NULL, *ctlBalanceJ = NULL;

    ctlCurrJ = json_object_array_get_idx(ctlsJ, ctlsIdx);
    wrap_json_unpack(ctlCurrJ, "{s?s,s?s,s?o,s?o,s?o}",
                     "uid", &ctlUid, "stream", &ctlStream,
                     "volume", &ctlVolumeJ, "volramp", &ctlVolrampJ,
                     "bass", &ctlBassJ, "mid", &ctlMidJ, "treble", &ctlTrebleJ,
                     "fade", &ctlFadeJ, "balance", &ctlBalanceJ);

    // Check that all required fields exist
    if (!ctlStream || !ctlVolumeJ)
    {
      AFB_ApiError(NULL, "CTL: Properties must include 'stream' and 'volume'!");
      return HAL_FAIL; 
    }

    // Check that the ctlStream points to a valid stream
    if (strcmp(ctlStream, "Master") != 0) // Master does not need to be defined
    {
      if (!json_object_array_find(_streammapJ, "stream", ctlStream))
      {
        AFB_ApiError(NULL, "CTL: Stream '%s' is not defined!", ctlStream);
        return HAL_FAIL;
      }
    }

    // Check that control 'value', 'minval', 'maxval' values are sane
    if (validateCtl(ctlVolumeJ, "volume") != HAL_OK)
      return HAL_FAIL;
    if (ctlVolrampJ)
      if (validateCtl(ctlVolrampJ, "volramp") != HAL_OK)
        return HAL_FAIL;
    if (ctlBassJ)
      if (validateCtl(ctlBassJ, "bass") != HAL_OK)
        return HAL_FAIL;
    if (ctlMidJ)
      if (validateCtl(ctlMidJ, "mid") != HAL_OK)
        return HAL_FAIL;
    if (ctlTrebleJ)
      if (validateCtl(ctlTrebleJ, "treble") != HAL_OK)
        return HAL_FAIL;
    if (ctlFadeJ)
      if (validateCtl(ctlFadeJ, "fade") != HAL_OK)
        return HAL_FAIL;
    if (ctlBalanceJ)
      if (validateCtl(ctlBalanceJ, "balance") != HAL_OK)
        return HAL_FAIL;
  }

  AFB_ApiNotice(NULL, "CTL: OK!");
  return HAL_OK;
}

/*
 * @brief Parse and validate a CTL definition
 */
HAL_ERRCODE validateCtl(json_object *ctlJ, const char *ctlType)
{
  // Set to defaults
  int ctlValue = 50, ctlMinval = 0, ctlMaxval = 100;

  wrap_json_unpack(ctlJ, "{s?i,s?i,s?i}",
                   "value", &ctlValue, "minval", &ctlMinval,
                   "maxval", &ctlMaxval);

  if (ctlValue > ctlMaxval)
  {
    AFB_ApiError(NULL, "CTL: '%s': 'value' '%i' is greater than it's maxval! (%d)",
                 ctlType, ctlValue, ctlMaxval);
    return HAL_FAIL;
  }

  if (ctlValue < ctlMinval)
  {
    AFB_ApiError(NULL, "CTL: '%s': 'value' '%i' is less than it's minval! (%d)",
                 ctlType, ctlValue, ctlMinval);
    return HAL_FAIL;
  }

  return HAL_OK;
}
  return HAL_OK;
}

/*
 * @brief Parse and validate all ZONE definitions
 */
HAL_ERRCODE validateZones(json_object *zonesJ)
{
  int zonesIdx = 0;
  int zonesLength = json_object_array_length(zonesJ);

  // Loop through the known zones, and make sure all is well
  for (zonesIdx = 0; zonesIdx < zonesLength; zonesIdx++)
  {
    char *zoneUid = NULL;
    char *zoneType = NULL;
    json_object *zoneMappingJ = NULL;
    json_object *zoneCurrJ = NULL;

    zoneCurrJ = json_object_array_get_idx(zonesJ, zonesIdx);
    wrap_json_unpack(zoneCurrJ, "{s?s,s?s,s?o}",
                     "uid", &zoneUid, "type", &zoneType, "mapping", &zoneMappingJ);

    // Check that all required fields exist
    if (!zoneUid || !zoneType || !zoneMappingJ)
    {
      AFB_ApiError(NULL, "ZONE: Zone properties must include 'uid', 'type', and 'mapping'!");
      return HAL_FAIL; 
    }

    // Check that the zoneType is always "sink" or "source"
    if ((strcmp(zoneType, "sink") != 0) && (strcmp(zoneType, "source") != 0))
    {
      AFB_ApiError(NULL, "ZONE: Zone type '%s' is not allowed. Must be 'sink' or 'source'!", zoneType);
      return HAL_FAIL;
    }

    // Check that the mapping has at least one element
    if (json_object_array_length(zoneMappingJ) <= 0)
    {
      AFB_ApiError(NULL, "ZONE: Zone map must have at least one element!");
      return HAL_FAIL;
    }
  }

  return HAL_OK;
}

HAL_ERRCODE initialize_sound_card()
{
    json_object *cfgJ = NULL;
    json_object *cfgResultJ = NULL;
    json_object *resultJ = NULL;
    int result = -1;
    const char *message;

    if (!_cardpropsJ || !_streammapJ || !_ctlsJ)
    {
      AFB_ApiError(NULL, "Cannot initialize HAL plugin: cardprops or streammap is NULL!");
      return HAL_FAIL;
    }

    AFB_ApiNotice(NULL, "Configuration OK, sending to HAL plugin");
    wrap_json_pack(&cfgJ, "{s:o,s:o}",
                   "cardprops", _cardpropsJ,
                   "streammap", _streammapJ);

    printf("jobj from str:\n---\n%s\n---\n", json_object_to_json_string_ext(cfgJ, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));

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
    } else {
        int errCode = -1;
        wrap_json_unpack(cfgResultJ, "{s:{s:i,s:s}}", "response", "errcode", &errCode, "message", &message);
        AFB_NOTICE("Message: %s, ErrCode: %d", message, result);
    }

    // No error found, return success to afb init
    return HAL_OK;
}

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

PUBLIC STATIC json_object *generateCardProperties(json_object *cfgCardsJ)
{
  AFB_NOTICE("Generating Card Properties");
  json_object *cfgCardPropsJ;
  wrap_json_pack(&cfgCardPropsJ, "{}");
  json_object *currCardJ = json_object_array_get_idx(cfgCardsJ, 0);
  if (currCardJ)
  {
    json_object *cardChannels = 0;

    wrap_json_unpack(currCardJ, "{s:o}", "channels", &cardChannels);
    if (cardChannels)
    {
      json_object *cardSource = 0, *cardSink = 0;
      // Abstract sink and source from the card channel definition
      wrap_json_unpack(cardChannels, "{s:o,s:o}", "sink", &cardSink, "source", &cardSource);

      // Wrap into our configuration object

      wrap_json_pack(&cfgCardPropsJ, "{s:o*,s:o*}", "sink", cardSink, "source", cardSource);
    }
  }
  return cfgCardPropsJ;
}

//
// Construct the stream map by looping over each stream and extracting the mapping.
//
PUBLIC STATIC json_object *generateStreamMap(json_object *streamsJ, json_object *zonesJ)
{
  int idxStream;
  json_object *streamMapArrayJ;
  wrap_json_pack(&streamMapArrayJ, "[]");

  for (idxStream = 0; idxStream < json_object_array_length(streamsJ); idxStream++)
  {
    json_object *streamMapJ = 0;

    const char *streamName = NULL,
               *sourceProfile = NULL, *sinkProfile = NULL,
               *sourceZone = NULL, *sinkZone = NULL;
    int sinkChannels = 0, sourceChannels = 0;
    json_object *streamSource = NULL, *streamSink = NULL,
                *currStreamJ = NULL;

    currStreamJ = json_object_array_get_idx(streamsJ, idxStream);

    // Begin unpacking
    wrap_json_unpack(currStreamJ, "{s:s,s?o,s?o}",
                     "role", &streamName, "source", &streamSource,
                     "sink", &streamSink);

    wrap_json_unpack(streamSource, "{s?s,s:s}",
                     "profile", &sourceProfile, "zone", &sourceZone);
    wrap_json_unpack(streamSink, "{s?s,s:s}",
                     "profile", &sinkProfile, "zone", &sinkZone);

    // Validate that the mapped zone exists, and get the zone's mapping
    json_object *sourceJ = NULL, *sinkJ = NULL;
    json_object *sourceZoneMapJ = NULL, *sinkZoneMapJ = NULL;
    if (sourceZone)
    {
      AFB_NOTICE("sourceZone: %s", sourceZone);
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
