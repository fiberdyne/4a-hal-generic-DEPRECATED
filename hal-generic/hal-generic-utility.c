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


/*****************************************************************************
 * Local Function Declarations
 ****************************************************************************/
PUBLIC STATIC json_object *getZoneMap(json_object *zonesJ, const char *zoneName);


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

PUBLIC json_object *generateCardProperties(json_object *cfgCardsJ)
{
  json_object *cfgCardPropsJ;
  wrap_json_pack(&cfgCardPropsJ, "{}");

  AFB_ApiNotice(NULL, "CARD: Generating 'cardprops'");

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
PUBLIC json_object *generateStreamMap(json_object *streamsJ,
                                             json_object *zonesJ)
{
  int streamsIdx = 0;
  int streamsLength = json_object_array_length(streamsJ);
  json_object *streamMapArrayJ = NULL;

  AFB_ApiNotice(NULL, "STREAM: Generating 'streammap'");

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
