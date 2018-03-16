#ifndef HAL_DSPUTILITY_H
#define HAL_DSPUTILITY_H

#include <stdio.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

#include <json-c/json.h>

#ifndef PUBLIC
  #define PUBLIC
#endif
#define STATIC static

PUBLIC json_object *getMap(json_object *cfgZones, const char *chanType);

PUBLIC json_object* getConfigurationString(json_object *configJ);

#endif /* HAL_DSPUTILITY_H */