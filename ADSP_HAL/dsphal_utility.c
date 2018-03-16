#include "dsphal_utility.h"
#include "wrap-json.h"
#include <string.h>

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

    AFB_NOTICE("Printing Card Prop: %s", json_object_get_string(configurationString));

//    AFB_NOTICE("Schema: %s", cfgSchema);
//    AFB_NOTICE("Streams: %s", json_object_get_string(cfgStreamsJ));
}