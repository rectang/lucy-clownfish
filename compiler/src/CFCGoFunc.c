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


#include <string.h>
#include <stdio.h>
#define CFC_NEED_BASE_STRUCT_DEF
#define CFC_NEED_GOFUNC_STRUCT_DEF
#include "CFCGoFunc.h"
#include "CFCGoTypeMap.h"
#include "CFCBase.h"
#include "CFCFunction.h"
#include "CFCUtil.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCVariable.h"
#include "CFCType.h"

#ifndef true
    #define true 1
    #define false 0
#endif

CFCGoFunc*
CFCGoFunc_init(CFCGoFunc *self, CFCParcel *parcel,
               CFCParamList *param_list) {
    CFCUTIL_NULL_CHECK(param_list);
    CFCUTIL_NULL_CHECK(parcel);
    self->param_list  = (CFCParamList*)CFCBase_incref((CFCBase*)param_list);
    self->parcel      = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->go_name     = NULL;
    return self;
}

void
CFCGoFunc_destroy(CFCGoFunc *self) {
    CFCBase_decref((CFCBase*)self->param_list);
    CFCBase_decref((CFCBase*)self->parcel);
    FREEMEM(self->go_name);
    CFCBase_destroy((CFCBase*)self);
}

int
CFCGoFunc_can_be_bound(CFCFunction *function) {
    // Test whether parameters can be mapped automatically.
    CFCParamList  *param_list = CFCFunction_get_param_list(function);
    CFCVariable  **arg_vars   = CFCParamList_get_variables(param_list);
    for (size_t i = 0; arg_vars[i] != NULL; i++) {
        CFCType *type = CFCVariable_get_type(arg_vars[i]);
        if (!CFCType_is_object(type) && !CFCType_is_primitive(type)) {
            return false;
        }
    }

    // Test whether return type can be mapped automatically.
    CFCType *return_type = CFCFunction_get_return_type(function);
    if (!CFCType_is_void(return_type)
        && !CFCType_is_object(return_type)
        && !CFCType_is_primitive(return_type)
       ) {
        return false;
    }

    return true;
}

char*
CFCGoFunc_func_start(const char *name, CFCParamList *param_list,
                     CFCParcel *parcel, CFCType *return_type, int is_method) {
    CFCVariable **param_vars = CFCParamList_get_variables(param_list);
    char *invocant;
    if (is_method) {
        CFCVariable *var = param_vars[0];
        CFCType *type = CFCVariable_get_type(var);
        char *go_type_name = CFCGoTypeMap_go_type_name(type, parcel);
        invocant = CFCUtil_sprintf("(%s %s) ", CFCVariable_micro_sym(var),
                                   go_type_name);
        FREEMEM(go_type_name);
    }
    else {
        invocant = CFCUtil_strdup("");
    }

    char *params = CFCUtil_strdup("");
    int start = is_method ? 1 : 0;
    for (int i = start; param_vars[i] != NULL; i++) {
        CFCVariable *var = param_vars[i];
        CFCType *type = CFCVariable_get_type(var);
        char *go_type_name = CFCGoTypeMap_go_type_name(type, parcel);
        if (i > start) {
            params = CFCUtil_cat(params, ", ", NULL);
        }
        params = CFCUtil_cat(params, CFCVariable_micro_sym(var), " ",
                             go_type_name, NULL);
        FREEMEM(go_type_name);
    }

    char *ret_type_str;
    if (CFCType_is_void(return_type)) {
        ret_type_str = CFCUtil_strdup("");
    }
    else {
        ret_type_str = CFCGoTypeMap_go_type_name(return_type, parcel);
        if (ret_type_str == NULL) {
            CFCUtil_die("Can't convert invalid type in method %s", name);
        }
        ret_type_str = CFCUtil_cat(ret_type_str, " ", NULL);
    }

    char pattern[] = "func %s%s(%s) %s{";
    char *content = CFCUtil_sprintf(pattern, invocant, name, params,
                                    ret_type_str);

    FREEMEM(invocant);
    FREEMEM(params);
    FREEMEM(ret_type_str);

    return content;
}

char*
CFCGoFunc_go_meth_name(const char *orig) {
    char *go_name = CFCUtil_strdup(orig);
    for (int i = 0, j = 0, max = strlen(go_name) + 1; i < max; i++) {
        if (go_name[i] != '_') {
            go_name[j++] = go_name[i];
        }
    }
    return go_name;
}


CFCParamList*
CFCGoFunc_get_param_list(CFCGoFunc *self) {
    return self->param_list;
}

CFCParcel*
CFCGoFunc_get_parcel(CFCGoFunc *self) {
    return self->parcel;
}

void
CFCGoFunc_set_go_name(CFCGoFunc *self, const char *go_name) {
    FREEMEM(self->go_name);
    self->go_name = CFCUtil_strdup(go_name);
}

const char*
CFCGoFunc_get_go_name(CFCGoFunc *self) {
    if (!self->go_name) {
        CFCUtil_die("go name not yet set");
    }
    return self->go_name;
}

const char*
CFCGoFunc_c_name_list(CFCGoFunc *self) {
    return CFCParamList_name_list(self->param_list);
}

