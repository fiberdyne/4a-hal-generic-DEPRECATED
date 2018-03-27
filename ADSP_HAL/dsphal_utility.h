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

typedef enum
{
  DSPHAL_OK,
  DSPHAL_FAIL
} DSPHAL_ERRCODE;

PUBLIC json_object *loadHalConfig(void);
DSPHAL_ERRCODE initialize_sound_card(json_object *configJ);

PUBLIC json_object *getMap(json_object *cfgZones, const char *chanType);
PUBLIC json_object *generateCardProperties(json_object *cfgCardsJ);
PUBLIC json_object *generateStreamMap(json_object *cfgStreamsJ, json_object *cfgZoneJ);
PUBLIC json_object *getConfigurationString(json_object *configJ);

#endif /* HAL_DSPUTILITY_H */