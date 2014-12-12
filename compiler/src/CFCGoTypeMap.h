/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_CFCGOTYPEMAP
#define H_CFCGOTYPEMAP

#ifdef __cplusplus
extern "C" {
#endif

struct CFCType;
struct CFCParcel;

char*
CFCGoTypeMap_go_type_name(struct CFCType *type,
                          struct CFCParcel *current_parcel);

char*
CFCGoTypeMap_cgo_type_name(struct CFCType *type);

char*
CFCGoTypeMap_go_var_to_c(struct CFCType *type, const char *go_var);

char*
CFCGoTypeMap_c_var_to_go(struct CFCType *type, const char *c_var,
                         struct CFCParcel *current_parcel);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCGOTYPEMAP */

