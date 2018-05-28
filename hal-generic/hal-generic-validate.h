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

#ifndef HAL_GENERIC_VALIDATE_H
#define HAL_GENERIC_VALIDATE_H

#include "hal-generic.h"

#include <json-c/json.h>


/*****************************************************************************
 *  Global Function Declarations
 ****************************************************************************/
PUBLIC HAL_ERRCODE validateCards(json_object *cardsJ);
PUBLIC HAL_ERRCODE validateZones(json_object *zonesJ, json_object *cardsJ);
PUBLIC HAL_ERRCODE validateStreams(json_object *streamsJ, json_object *zonesJ);
PUBLIC HAL_ERRCODE validateCtls(json_object *ctlsJ, json_object *streamsJ);

#endif // HAL_GENERIC_VALIDATE_H