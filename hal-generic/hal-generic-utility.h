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

#ifndef HAL_GENERIC_UTILITY_H
#define HAL_GENERIC_UTILITY_H

#include "hal-generic.h"
#include "hal-interface.h"

#include <json-c/json.h>


/*****************************************************************************
 * Global Function Declarations
 ****************************************************************************/
PUBLIC json_object *json_object_array_find(json_object *arrayJ,
                                           const char *key,
                                           const char *value);

PUBLIC json_object *getCardInfo(json_object *cardsJ);
PUBLIC alsaHalMapT *generateAlsaHalMap(json_object *ctlsJ);
PUBLIC json_object *generateCardProperties(json_object *cardJ,
                                           const char *cardname);
PUBLIC json_object *generateStreamMap(json_object *streamsJ,
                                      json_object *zonesJ,
                                      const char *cardname);
PUBLIC HAL_ERRCODE initHalPlugin(const char *halPluginName,
                                 json_object *cardpropsJ,
                                 json_object *streammapJ);

#endif /* HAL_GENERIC_UTILITY_H */