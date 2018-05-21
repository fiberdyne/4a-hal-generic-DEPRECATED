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

// Local function declarations
PUBLIC STATIC json_object *generateCardProperties(json_object *cardsJ);
PUBLIC STATIC json_object *generateStreamMap(json_object *streamsJ, json_object *zonesJ);
PUBLIC STATIC json_object *getMap(json_object *zonesJ, const char *zoneName);

PUBLIC STATIC HAL_ERRCODE validateZones(json_object *zonesJ);
PUBLIC STATIC HAL_ERRCODE validateStreams(json_object *streamsJ);
PUBLIC STATIC HAL_ERRCODE validateStreamControls(json_object *streamCtlsJ);


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
                *streamSourceJ = NULL, *streamCtlsJ = NULL;

    streamCurrJ = json_object_array_get_idx(streamsJ, streamsIdx);
    wrap_json_unpack(streamCurrJ, "{s?s,s?s,s?o,s?o,s?o}",
                     "uid", &streamUid, "role", &streamRole, "sink", &streamSinkJ, 
                     "source", &streamSourceJ, "ctls", &streamCtlsJ);

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
    else if (!getMap(_zonesJ, streamSinkZone))
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
      else if (!getMap(_zonesJ, streamSourceZone))
        return HAL_FAIL;
    }

    // If ctls are defined ...
    if (streamCtlsJ)
      validateStreamControls(streamCtlsJ);
  }

  return HAL_OK;
}

/*
 * @brief Parse and validate all stream CTL definitions
 */
HAL_ERRCODE validateStreamControls(json_object *streamsControlsJ)
{
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

    if (!_cardpropsJ || !_streammapJ)
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
PUBLIC STATIC json_object *getMap(json_object *zonesJ, const char *zoneName)
{
  int zonesIdx = 0;
  int zonesLength = json_object_array_length(zonesJ);
  json_object *zoneMappingJ = 0;

  //AFB_NOTICE("%s, %s", zoneName, json_object_get_string(zonesJ));

  // Loop through the known zones, and attempt to find the indicated zone
  for (zonesIdx = 0; zonesIdx < zonesLength; zonesIdx++)
  {
    char *zoneUid = "";
    char *zoneType = "";

    json_object *zoneCurrJ = json_object_array_get_idx(zonesJ, zonesIdx);
    wrap_json_unpack(zoneCurrJ, "{s:s,s:s,s:o}",
                     "uid", &zoneUid, "type", &zoneType, "mapping", &zoneMappingJ);

    //AFB_NOTICE("%s, %s", zoneUid, zoneType);
    if (strcmp(zoneUid, zoneName) == 0)
    {
      //AFB_NOTICE("%s",json_object_get_string(zoneMapping));
      return zoneMappingJ;
    }
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

    json_object *currStreamJ = 0;
    const char *streamName = NULL;
    const char *sourceProfile = NULL, *sinkProfile = NULL;
    const char *sourceZone = NULL, *sinkZone = NULL;
    int sinkChannels = 0, sourceChannels = 0;
    json_object *strmSource = 0, *strmSink = 0;

    currStreamJ = json_object_array_get_idx(streamsJ, idxStream);

    // Begin unpacking
    wrap_json_unpack(currStreamJ, "{s:s,s?o,s?o}",
                      "role", &streamName, "source", &strmSource, "sink", &strmSink);

    wrap_json_unpack(strmSource, "{s?s,s:s}",
                      "profile", &sourceProfile, "zone", &sourceZone);
    wrap_json_unpack(strmSink, "{s?s,s:s}",
                      "profile", &sinkProfile, "zone", &sinkZone);

    // Validate that the mapped zone exists, and get the zone's mapping
    json_object *sourceJ = NULL, *sinkJ = NULL;
    json_object *sourceZoneMapJ = NULL, *sinkZoneMapJ = NULL;
    if (sourceZone)
    {
      AFB_NOTICE("sourceZone: %s", sourceZone);
      sourceZoneMapJ = getMap(zonesJ, sourceZone);
      if (sourceZoneMapJ)
      {
        sourceChannels = json_object_array_length(sourceZoneMapJ);
        wrap_json_pack(&sourceJ, "{s:i,s:o}",
                        "channels", sourceChannels, "mapping", sourceZoneMapJ);
      }
    }
    if (sinkZone)
    {
      sinkZoneMapJ = getMap(zonesJ, sinkZone);
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
