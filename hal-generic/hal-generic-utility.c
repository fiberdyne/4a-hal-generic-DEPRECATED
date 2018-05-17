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
#include "filescan-utils.h"

#include <sys/mman.h>
#include <sys/stat.h>

#define MAX_FILENAME_LEN 255

json_object *loadHalConfig(void)
{
    char filename[MAX_FILENAME_LEN];
    struct stat st;
    int fd;
    json_object *config = NULL;
    char *configJsonStr;
    char *bindingDirPath = GetBindingDirPath(NULL);

    AFB_NOTICE("Binding path: %s", bindingDirPath);
    snprintf(filename, MAX_FILENAME_LEN, "%s/etc/%s", bindingDirPath, "onload-config-0001.json");
    AFB_NOTICE("path: %s", filename);

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

HAL_ERRCODE initialize_sound_card(json_object *configJ)
{
    json_object *configurationStringJ = NULL;
    json_object *cfgResultJ = NULL;
    json_object *resultJ = NULL;
    int result = -1;
    const char *message;

    configurationStringJ = getConfigurationString(configJ);

    AFB_NOTICE("Configuration String recieved, sending to sound card");

    result = afb_service_call_sync("fd-dsp-hifi2", "initialize_sndcard", configurationStringJ, &cfgResultJ);
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

PUBLIC json_object *getMap(json_object *cfgZones, const char *zoneName)
{
    int idxZone;
    json_object *zoneMapping = 0;

    //AFB_NOTICE("%s, %s", zoneName, json_object_get_string(cfgZones));

    for (idxZone = 0; idxZone < json_object_array_length(cfgZones); idxZone++)
    {
        char *zoneUid = "";
        char *zoneType = "";

        json_object *currZone = json_object_array_get_idx(cfgZones, idxZone);
        wrap_json_unpack(currZone, "{s:s,s:s,s:o}",
                         "uid", &zoneUid, "type", &zoneType, "mapping", &zoneMapping);

        //AFB_NOTICE("%s, %s", zoneUid, zoneType);
        if (strcmp(zoneUid, zoneName) == 0)
        {
            //AFB_NOTICE("%s",json_object_get_string(zoneMapping));
            return zoneMapping;
        }
    }

    wrap_json_pack(&zoneMapping, "[]");
    return zoneMapping;
}

PUBLIC json_object *generateCardProperties(json_object *cfgCardsJ)
{
    AFB_NOTICE("Generating Card Properties");
    json_object *cfgCardPropsJ;
    wrap_json_pack(&cfgCardPropsJ, "{}");
    json_object *currCard = json_object_array_get_idx(cfgCardsJ, 0);
    if (currCard)
    {
        json_object *cardChannels = 0;

        wrap_json_unpack(currCard, "{s:o}", "channels", &cardChannels);
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

PUBLIC json_object *generateStreamMap(json_object *cfgStreamsJ, json_object *cfgZoneJ)
{
    //
    // Construct the stream map by looping over each stream and extracting the mapping.
    //
    int idxStream;
    json_object *cfgStreamMapJ;
    wrap_json_pack(&cfgStreamMapJ, "[]");
    for (idxStream = 0; idxStream < json_object_array_length(cfgStreamsJ); idxStream++)
    {
        json_object *cfgstrStream = 0;

        json_object *currStreamJ = 0;
        const char *strmName = "";
        const char *strmSrcProfile = "", *strmSnkProfile = "";
        const char *strmSrcZone = "", *strmSnkZone = "";
        int strmSrcChannels = 0, strmSnkChannels = 0;
        json_object *strmSrcDflt = 0, *strmSnkDflt = 0;
        json_object *strmSource = 0, *strmSink = 0;

        currStreamJ = json_object_array_get_idx(cfgStreamsJ, idxStream);

        wrap_json_unpack(currStreamJ, "{s:s,s?o,s?o}",
                         "name", &strmName, "source", &strmSource, "sink", &strmSink);

        wrap_json_unpack(strmSource, "{s:i,s?s,s:s,s?o}", "channels", &strmSrcChannels, "profile", &strmSrcProfile, "zone", &strmSrcZone, "defaultconfig", &strmSrcDflt);
        wrap_json_unpack(strmSink, "{s:i,s?s,s:s,s?o}", "channels", &strmSnkChannels, "profile", &strmSnkProfile, "zone", &strmSnkZone, "defaultconfig", &strmSnkDflt);

        json_object *streamSourceMap = getMap(cfgZoneJ, strmSrcZone);
        json_object *streamSinkMap = getMap(cfgZoneJ, strmSnkZone);

        json_object *sourceCfg = 0, *sinkCfg = 0;
        wrap_json_pack(&sourceCfg, "{s:i,s:o}", "channels", strmSrcChannels, "mapping", streamSourceMap);
        wrap_json_pack(&sinkCfg, "{s:i,s:o}", "channels", strmSnkChannels, "mapping", streamSinkMap);
        wrap_json_pack(&cfgstrStream, "{s:s,s:o,s:o}", "stream", strmName, "source", sourceCfg, "sink", sinkCfg);

        json_object_array_add(cfgStreamMapJ, cfgstrStream);

        //AFB_NOTICE("---------");
    }
    return cfgStreamMapJ;
}

PUBLIC json_object *getConfigurationString(json_object *configJ)
{
    json_object *configurationString;
    const char *cfgSchema;
    json_object *cfgMetadataJ;
    json_object *cfgPluginsJ, *cfgControlJ, *cfgEqPointJ, *cfgFilterJ, *cfgCardsJ, *cfgZoneJ, *cfgStreamsJ;

    AFB_NOTICE("---------------------------");
    AFB_NOTICE("Starting Configuration Read");

    wrap_json_unpack(configJ, "{s:s,s:o,s:o,s:o,s:o,s:o,s:o,s:o,s:o}",
                     "$schema", &cfgSchema, "metadata", &cfgMetadataJ, "plugins", &cfgPluginsJ,
                     "control", &cfgControlJ, "eqpoint", &cfgEqPointJ, "filter", &cfgFilterJ,
                     "cards", &cfgCardsJ, "zone", &cfgZoneJ, "streams", &cfgStreamsJ);

    json_object *cfgCardPropsJ = generateCardProperties(cfgCardsJ);
    json_object *cfgStreamMapJ = generateStreamMap(cfgStreamsJ, cfgZoneJ);

    wrap_json_pack(&configurationString, "{s:o,s:o}", "cardprops", cfgCardPropsJ, "streammap", cfgStreamMapJ);

    //AFB_NOTICE("Printing configurationString: %s", json_object_get_string(configurationString));

    return configurationString;
}