/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vendor.h"

void vendor_init()
{
}

// Returns the MAC System Mode based on the set of SFP IDs provided.
// The derived class will most likely need to override this method to provide a
// different implementation for that particular OLT platform.
pair<bcmolt_system_mode, bool> PonTrx::get_mac_system_mode(int olt_mac_id, set<int> sfp_ids) {
    bool ret = true;
    bcmolt_system_mode sm = BCMOLT_SYSTEM_MODE_XGS__2_X;

    return {sm, ret};
}