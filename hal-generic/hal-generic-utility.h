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
#include "ctl-config.h"

int CardConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *cardsJ);
int StreamConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *streamsJ);
int ZoneConfig(AFB_ApiT apiHandle, CtlSectionT *section, json_object *zonesJ);

PUBLIC json_object *loadHalConfig(void);
HAL_ERRCODE initialize_sound_card();

#endif /* HAL_GENERIC_UTILITY_H */