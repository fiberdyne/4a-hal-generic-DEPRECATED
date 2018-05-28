/*
 * Copyright (C) 2018 Fiberdyne Systems
 * 
 * Author: Mark Farrugia <mark.farrugia@fiberdyne.com.au>
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


/*****************************************************************************
 * Included Files
 ****************************************************************************/
#include "hal-generic-validate.h"
#include "hal-generic-utility.h"
#include "wrap-json.h"

#include <string.h>


/*****************************************************************************
 * Local Function Definitions
 ****************************************************************************/
static char assert_j_errbuf[255];
#define ASSERT_J(j, errmsg, type, ...)                       \
  {                                                          \
    if (!json_object_is_type(j, type))                       \
    {                                                        \
      snprintf(assert_j_errbuf, 255, errmsg, ##__VA_ARGS__); \
      AFB_ApiError(NULL, assert_j_errbuf);                   \
      return HAL_FAIL;                                       \
    }                                                        \
  }

#define ASSERT_JARRAY(jarray, errmsg, ...)                   \
  ASSERT_J(jarray, errmsg, json_type_array, ##__VA_ARGS__)

#define ASSERT_JOBJECT(jobject, errmsg, ...)                 \
  ASSERT_J(jobject, errmsg, json_type_object, ##__VA_ARGS__)


/*****************************************************************************
 * Local Function Declarations
 ****************************************************************************/
PUBLIC STATIC HAL_ERRCODE validateCardSinkSource(json_object *arrayJ);
PUBLIC STATIC HAL_ERRCODE validateZoneIsSinkSource(json_object *zoneJ,
                                                   const char *zoneId,
                                                   SinkSourceT matchType);
PUBLIC STATIC HAL_ERRCODE validateZoneMapping(json_object *zoneMappingJ,
                                              json_object *cardsJ,
                                              SinkSourceT sinkSourceType);
PUBLIC STATIC HAL_ERRCODE validateCtl(json_object *ctlJ, const char *ctlType);


/*****************************************************************************
 * Local Function Definitions
 ****************************************************************************/
/*
 * @brief Parse and validate a CTL definition
 * @params ctlJ    : The ctl json_object
 *         ctlType : The ctl type for error reporting
 * @return HAL_OK if ctl is valid, HAL_FAIL otherwise 
 */
STATIC HAL_ERRCODE validateCtl(json_object *ctlJ, const char *ctlType)
{
  // Set to defaults
  int ctlValue = 50, ctlMinval = 0, ctlMaxval = 100;

  ASSERT_JOBJECT(ctlJ, "CTL: '%s' needs to be a JSON object!", ctlType);

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

/*
 * @brief Check that the zone is either a 'sink' or a 'source'
 * @param zoneJ  : The zone json_object
 * @param zoneId : The zone ID for debug
 * @param type   : The matchType to check for
 * @return HAL_OK if zone type matches matchType, HAL_FAIL otherwise
 */
STATIC HAL_ERRCODE validateZoneIsSinkSource(json_object *zoneJ,
                                            const char *zoneId,
                                            SinkSourceT matchType)
{
  char *zoneType = NULL;
  char *zoneTypeMatcher = (matchType == SINK) ? "sink" : "source";

  wrap_json_unpack(zoneJ, "{s?s}",
                  "type", &zoneType); 
  
  // Check that the zone type matches the intended type
  if (strcmp(zoneType, zoneTypeMatcher) != 0)
  {
    AFB_ApiError(NULL, "STREAM: Zone '%s' is not a '%s'!", zoneId, zoneTypeMatcher);
    return HAL_FAIL;
  }
  
  return HAL_OK;
}

/*
 * @brief Parse and validate all card channel SINK/SOURCE definitions
 */
STATIC HAL_ERRCODE validateCardSinkSource(json_object *arrayJ)
{
  int idx = 0, len = 0;
  
  len = json_object_array_length(arrayJ);

  // Loop through the known card channel sinks/sources
  for (idx = 0; idx < len; idx++)
  {
    json_object *currJ = NULL;
    char *type = NULL;
    int port = -1;

    currJ = json_object_array_get_idx(arrayJ, idx);
    wrap_json_unpack(currJ, "{s?s,s?i}",
                     "type", &type, "port", &port);

    if (!type || (port < 0))
    {
      AFB_ApiError(NULL, "CARD: Channel sink/source must contain 'type' and 'port' properties!");
      return HAL_FAIL; 
    }

    // Check 'type' against allowed enum types
    // TODO
  }
  
  return HAL_OK;
}

/*
 * @brief Check that the zone mapping only uses card-declared sinks or sources
 * @params zoneMappingJ   : A json_object containing an array of 'zone' mapping arrays
 *         cardsJ         : A json_object containing an array of 'card' objects
 *         sinkSourceType : An enum indicating the zone type (SINK or SOURCE)
 * @return HAL_OK if zone mapping is valid, HAL_FAIL otherwise
 */
STATIC HAL_ERRCODE validateZoneMapping(json_object *zoneMappingJ,
                                       json_object *cardsJ,
                                       SinkSourceT sinkSourceType)
{
  int zoneMappingIdx = 0;
  int zoneMappingLength = json_object_array_length(zoneMappingJ);

  for (zoneMappingIdx = 0; zoneMappingIdx < zoneMappingLength; zoneMappingIdx++)
  {
    json_object *zoneMappingCurrJ;
    int inputIdx = 0, inputLength = 0;

    // Get inner "input" array
    zoneMappingCurrJ = json_object_array_get_idx(zoneMappingJ, zoneMappingIdx);
    ASSERT_JARRAY(zoneMappingCurrJ, "ZONE: Zone mapping input must be an array!");

    // Loop the input indices
    inputLength = json_object_array_length(zoneMappingCurrJ);
    for (inputIdx = 0; inputIdx < inputLength; inputIdx++)
    {
      json_object *inputCurrJ = NULL;
      char *map = NULL;

      inputCurrJ = json_object_array_get_idx(zoneMappingCurrJ, inputIdx);
      wrap_json_unpack(inputCurrJ, "s", &map);
      if (!map)
      {
        AFB_ApiError(NULL, "ZONE: Mapping must contain at least one element!");
        return HAL_FAIL;
      }

      if (!getCardSinkSource(map, cardsJ, sinkSourceType))
      {
        AFB_ApiError(NULL, "ZONE: Map '%s' is not defined in any card!", map);
        return HAL_FAIL;
      }
    }
  }

  return HAL_OK;
}


/*****************************************************************************
 * Global Function Definitions
 ****************************************************************************/
/*
 * @brief Parse and validate all STREAM definitions
 */
HAL_ERRCODE validateStreams(json_object *streamsJ, json_object *zonesJ)
{
  int streamsIdx = 0, streamsLength = 0;
  
  // Check that streamsJ is an array
  ASSERT_JARRAY(streamsJ, "STREAM: Parent object must be an array!");

  // Loop through the known streams, and make sure all is well
  streamsLength = json_object_array_length(streamsJ);
  for (streamsIdx = 0; streamsIdx < streamsLength; streamsIdx++)
  {
    char *streamUid = NULL, *streamRole = NULL,
         *streamSinkZone = NULL, *streamSourceZone = NULL;
    json_object *streamCurrJ = NULL, *streamSinkJ = NULL,
                *streamSourceJ = NULL, *resultJ = NULL;

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

    ASSERT_JOBJECT(streamSinkJ, "STREAM: 'sink' must be a JSON object!");
    if (streamSourceJ)
      ASSERT_JOBJECT(streamSourceJ, "STREAM: 'source' must be a JSON object!");

    // Check that the streamRole is always valid
    // TODO check halCtlsTagT type for role

    // Check the stream sink contains the field 'zone'
    wrap_json_unpack(streamSinkJ, "{s?s}",
                     "zone", &streamSinkZone);
    if (!streamSinkZone)
    {
      AFB_ApiError(NULL, "STREAM: Sink must have field 'zone'!");
      return HAL_FAIL;
    }
    // Check that the zone exists
    resultJ = json_object_array_find(zonesJ, "uid", streamSinkZone);
    if (!resultJ)
    {
      AFB_ApiError(NULL, "STREAM: Zone '%s' is not defined!", streamSinkZone);
      return HAL_FAIL;
    }
    // Check that the zone is a sink
    if (validateZoneIsSinkSource(resultJ, streamSinkZone, SINK) == HAL_FAIL)
      return HAL_FAIL;

    // If stream source is defined, check it contains the field 'zone'
    if (streamSourceJ)
    {
      wrap_json_unpack(streamSourceJ, "{s?s}",
                      "zone", &streamSourceZone);
      if (!streamSourceZone)
      {
        AFB_ApiError(NULL, "STREAM: Source must have field 'zone'!");
        return HAL_FAIL;
      }
      // Check that the zone exists
      resultJ = json_object_array_find(zonesJ, "uid", streamSourceZone);
      if (!resultJ)
      {
        AFB_ApiError(NULL, "STREAM: Zone '%s' is not defined!", streamSourceZone);
        return HAL_FAIL;
      }
      // Check that the zone is a source
      if (validateZoneIsSinkSource(resultJ, streamSourceZone, SOURCE) == HAL_FAIL)
        return HAL_FAIL;
    }
  }

  AFB_ApiNotice(NULL, "STREAM: OK!");
  return HAL_OK;
}

/*
 * @brief Parse and validate all CTL definitions
 * @params ctlsJ    : A json_object containing an array of ctls
 *         streamsJ : A json_object containing an array of streams
 * @return HAL_OK if the ctls are valid, HAL_FAIL otherwise 
 */
HAL_ERRCODE validateCtls(json_object *ctlsJ, json_object *streamsJ)
{
  int ctlsIdx = 0, ctlsLength = 0;

  // Check that ctlsJ is an array
  ASSERT_JARRAY(ctlsJ, "CTL: Parent object must be an array!");

  // Loop through the ctls, and make sure all is well
  ctlsLength = json_object_array_length(ctlsJ);
  for (ctlsIdx = 0; ctlsIdx < ctlsLength; ctlsIdx++)
  {
    char *ctlUid = NULL, *ctlStream = NULL;
    json_object *ctlCurrJ = NULL, *ctlVolumeJ = NULL, *ctlVolrampJ = NULL,
                *ctlBassJ = NULL, *ctlMidJ = NULL, *ctlTrebleJ = NULL,
                *ctlFadeJ = NULL, *ctlBalanceJ = NULL;

    ctlCurrJ = json_object_array_get_idx(ctlsJ, ctlsIdx);
    wrap_json_unpack(ctlCurrJ, "{s?s,s?s,s?o,s?o,s?o,s?o,s?o,s?o,s?o}",
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
      if (!json_object_array_find(streamsJ, "role", ctlStream))
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
 * @brief Parse and validate all CARD definitions
 * @params cardsJ : A json_object containing an array of cards
 * @return HAL_OK if cards are valid, HAL_FAIL otherwise 
 */
HAL_ERRCODE validateCards(json_object *cardsJ)
{
  int cardsIdx = 0, cardsLength = 0;

  // Check that cardsJ is an array
  ASSERT_JARRAY(cardsJ, "CARD: Parent object must be an array!");

  // Loop through the known cards, and make sure all is well
  cardsLength = json_object_array_length(cardsJ);
  for (cardsIdx = 0; cardsIdx < cardsLength; cardsIdx++)
  {
    char *cardName = NULL, *cardApi = NULL;
    json_object *cardCurrJ = NULL, *cardChannelsJ = NULL,
                *cardSinksJ = NULL, *cardSourcesJ = NULL;

    cardCurrJ = json_object_array_get_idx(cardsJ, cardsIdx);
    wrap_json_unpack(cardCurrJ, "{s?s,s?s,s?o}",
                     "name", &cardName, "api", &cardApi,
                     "channels", &cardChannelsJ);

    // Check that all required fields exist
    if (!cardName || !cardApi || !cardChannelsJ)
    {
      AFB_ApiError(NULL, "CARD: Properties must include 'name', 'api', and channels'!");
      return HAL_FAIL; 
    }

    // Check the channels object
    wrap_json_unpack(cardChannelsJ, "{s?o,s?o}",
                     "sink", &cardSinksJ, "source", &cardSourcesJ);
    if (!cardSinksJ)
    {
      AFB_ApiError(NULL, "CARD: Channels must contain 'sink' property!");
      return HAL_FAIL; 
    }
    
    // Check the channels sink and source objects
    if (validateCardSinkSource(cardSinksJ) != HAL_OK)
      return HAL_FAIL;
    if (cardSourcesJ)
      if (validateCardSinkSource(cardSourcesJ) != HAL_OK)
        return HAL_FAIL;
  }

  AFB_ApiNotice(NULL, "CARD: OK!");
  return HAL_OK;
}

/*
 * @brief Parse and validate all ZONE definitions
 */
HAL_ERRCODE validateZones(json_object *zonesJ, json_object *cardsJ)
{
  int zonesIdx = 0, zonesLength = 0;

  // Check that zonesJ is an array
  ASSERT_JARRAY(zonesJ, "ZONE: Parent object must be an array!");
  
  // Loop through the known zones, and make sure all is well
  zonesLength = json_object_array_length(zonesJ);
  for (zonesIdx = 0; zonesIdx < zonesLength; zonesIdx++)
  {
    char *zoneUid = NULL, *zoneType = NULL;
    json_object *zoneMappingJ = NULL, *zoneCurrJ = NULL;
    SinkSourceT sinkSourceType;

    zoneCurrJ = json_object_array_get_idx(zonesJ, zonesIdx);
    wrap_json_unpack(zoneCurrJ, "{s?s,s?s,s?o}",
                     "uid", &zoneUid, "type", &zoneType, "mapping", &zoneMappingJ);

    // Check that all required fields exist
    if (!zoneUid || !zoneType || !zoneMappingJ)
    {
      AFB_ApiError(NULL, "ZONE: Properties must include 'uid', 'type', and 'mapping'!");
      return HAL_FAIL; 
    }

    // Check that the zoneType is always "sink" or "source"
    if (strcmp(zoneType, "sink") == 0)
      sinkSourceType = SINK;
    else if (strcmp(zoneType, "source") == 0)
      sinkSourceType = SOURCE;
    else
    {
      AFB_ApiError(NULL, "ZONE: Zone type '%s' is not allowed. Must be 'sink' or 'source'!", zoneType);
      return HAL_FAIL;
    }

    // Check that zoneMappingJ is an array
    ASSERT_JARRAY(zoneMappingJ, "ZONE: Zone mapping object must be an array!");

    // Check that the mapping has at least one element
    if (json_object_array_length(zoneMappingJ) <= 0)
    {
      AFB_ApiError(NULL, "ZONE: Zone map must have at least one element!");
      return HAL_FAIL;
    }

    // Check that each mapping entry corresponds to a card
    if (validateZoneMapping(zoneMappingJ, cardsJ, sinkSourceType) == HAL_FAIL)
      return HAL_FAIL;
  }

  AFB_ApiNotice(NULL, "ZONE: OK!");
  return HAL_OK;
}
