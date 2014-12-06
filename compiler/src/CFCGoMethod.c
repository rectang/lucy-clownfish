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
#include <ctype.h>

#define CFC_NEED_GOFUNC_STRUCT_DEF 1
#include "CFCGoFunc.h"
#include "CFCGoMethod.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCMethod.h"
#include "CFCSymbol.h"
#include "CFCType.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCGoTypeMap.h"
#include "CFCVariable.h"

struct CFCGoMethod {
    CFCBase     base;
    CFCMethod  *method;
};

/* Take a NULL-terminated list of CFCVariables and build up a string of
 * directives like:
 *
 *     UNUSED_VAR(var1);
 *     UNUSED_VAR(var2);
 */
static char*
S_build_unused_vars(CFCVariable **vars);

/* Create an unreachable return statement if necessary, in order to thwart
 * compiler warnings. */
static char*
S_maybe_unreachable(CFCType *return_type);

static const CFCMeta CFCPERLMETHOD_META = {
    "Clownfish::CFC::Binding::Perl::Method",
    sizeof(CFCGoMethod),
    (CFCBase_destroy_t)CFCGoMethod_destroy
};

CFCGoMethod*
CFCGoMethod_new(CFCMethod *method) {
    CFCGoMethod *self
        = (CFCGoMethod*)CFCBase_allocate(&CFCPERLMETHOD_META);
    return CFCGoMethod_init(self, method);
}

CFCGoMethod*
CFCGoMethod_init(CFCGoMethod *self, CFCMethod *method) {
    self->method = (CFCMethod*)CFCBase_incref((CFCBase*)method);
    return self;
}

void
CFCGoMethod_destroy(CFCGoMethod *self) {
    CFCBase_decref((CFCBase*)self->method);
    CFCBase_destroy((CFCBase*)self);
}

char*
CFCGoMethod_func_def(CFCGoMethod *self) {
    CFCMethod    *method     = self->method;
    CFCParcel    *parcel     = CFCMethod_get_parcel(method);
    CFCParamList *param_list = CFCMethod_get_param_list(method);
    CFCType      *ret_type   = CFCMethod_get_return_type(method);
    char *name = CFCGoFunc_go_meth_name(CFCMethod_get_macro_sym(method));
    char *start = CFCGoFunc_func_start(name, param_list, parcel, ret_type, 1);
    char pattern[] =
        "%s\n"
        "}\n";
    char *content = CFCUtil_sprintf(pattern, start);

    FREEMEM(name);
    FREEMEM(start);
    return content;
}

int
CFCGoMethod_can_be_bound(CFCMethod *method) {
    // Check for
    // - private methods
    // - methods with types which cannot be mapped automatically
    return !CFCSymbol_private((CFCSymbol*)method)
           && CFCGoFunc_can_be_bound((CFCFunction*)method);
}

char*
CFCGoMethod_callback_def(CFCMethod *method) {
    CFCParamList *param_list   = CFCMethod_get_param_list(method);
    CFCType      *return_type  = CFCMethod_get_return_type(method);
    char         *meth_typedef = CFCMethod_full_typedef(method, NULL);
    char         *meth_sym     = CFCMethod_full_method_sym(method, NULL);
    const char   *ret_type_str = CFCType_to_c(return_type);
    const char   *override_sym = CFCMethod_full_override_sym(method);
    const char   *params       = CFCParamList_to_c(param_list);
    const char   *name_list    = CFCParamList_name_list(param_list);
    const char   *maybe_return = CFCType_is_void(return_type)
                                 ? "" : "return ";
    char *content;

    if (CFCGoMethod_can_be_bound(method)) {
        const char pattern[] =
            "%s %s_wrapper;\n"
            "%s\n"
            "%s(%s) {\n"
            "    %s%s_wrapper(%s);\n"
            "}\n";
        content = CFCUtil_sprintf(pattern, meth_typedef, override_sym,
                                  ret_type_str, override_sym, params,
                                  maybe_return, override_sym, name_list);
    }
    else {
        CFCVariable **vars = CFCParamList_get_variables(param_list);
        char *unused = S_build_unused_vars(vars);
        char *unreachable = S_maybe_unreachable(return_type);
        const char pattern[] =
            "%s\n"
            "%s(%s) {%s\n"
            "    CFISH_THROW(CFISH_ERR, \"Can't override %s via binding\");%s\n"
            "}\n";
        content = CFCUtil_sprintf(pattern, ret_type_str, override_sym,
                                  params, unused, meth_sym, unreachable);
        FREEMEM(unused);
        FREEMEM(unreachable);
    }

    FREEMEM(meth_sym);
    FREEMEM(meth_typedef);
    return content;
}

char*
CFCGoMethod_callback_cgo(CFCMethod *method) {
    const char   *override_sym = CFCMethod_full_override_sym(method);
    CFCParcel    *parcel       = CFCMethod_get_parcel(method);
    CFCParamList *param_list   = CFCMethod_get_param_list(method);
    CFCVariable **param_vars   = CFCParamList_get_variables(param_list);
    CFCType      *self_type    = CFCMethod_self_type(method);
    CFCType      *return_type  = CFCMethod_get_return_type(method);
    char *meth_body = "";

    char *self_type_str = CFCGoTypeMap_cgo_type_name(self_type);
    if (self_type_str == NULL) {
        CFCUtil_die("Can't convert invalid type in method %s",
                    CFCMethod_full_method_sym(method, NULL));
    }

    char *ret_type_str;
    if (CFCType_is_void(return_type)) {
        ret_type_str = CFCUtil_strdup("");
    }
    else {
        ret_type_str = CFCGoTypeMap_cgo_type_name(return_type);
        if (ret_type_str == NULL) {
            CFCUtil_die("Can't convert invalid type in method %s",
                        CFCMethod_full_method_sym(method, NULL));
        }
        ret_type_str = CFCUtil_cat(ret_type_str, " ", NULL);
    }

    char *params = CFCUtil_strdup("");
    for (int i = 1; param_vars[i] != NULL; i++) {
        CFCVariable *var = param_vars[i];
        CFCType *type = CFCVariable_get_type(var);
        char *cgo_type_name = CFCGoTypeMap_cgo_type_name(type);
        if (cgo_type_name == NULL) {
            CFCUtil_die("Can't convert invalid type in method %s",
                        CFCMethod_full_method_sym(method, NULL));
        }
        if (i > 1) {
            params = CFCUtil_cat(params, ", ", NULL);
        }
        params = CFCUtil_cat(params, CFCVariable_micro_sym(var), " ",
                             cgo_type_name, NULL);
        FREEMEM(cgo_type_name);
    }

    char pattern[] =
        "//export %s_internal\n"
        "func (self %s) %s_internal(%s) %s{\n"
        "%s"
        "}\n";
    char *content = CFCUtil_sprintf(pattern, override_sym, self_type_str,
                                    override_sym, params, ret_type_str,
                                    meth_body);

    FREEMEM(self_type_str);
    FREEMEM(ret_type_str);
    FREEMEM(params);
    return content;
}

static char*
S_build_unused_vars(CFCVariable **vars) {
    char *unused = CFCUtil_strdup("");

    for (int i = 0; vars[i] != NULL; i++) {
        const char *var_name = CFCVariable_micro_sym(vars[i]);
        size_t size = strlen(unused) + strlen(var_name) + 80;
        unused = (char*)REALLOCATE(unused, size);
        strcat(unused, "\n    CFISH_UNUSED_VAR(");
        strcat(unused, var_name);
        strcat(unused, ");");
    }

    return unused;
}

static char*
S_maybe_unreachable(CFCType *return_type) {
    char *return_statement;
    if (CFCType_is_void(return_type)) {
        return_statement = CFCUtil_strdup("");
    }
    else {
        const char *ret_type_str = CFCType_to_c(return_type);
        char pattern[] = "\n    CFISH_UNREACHABLE_RETURN(%s);";
        return_statement = CFCUtil_sprintf(pattern, ret_type_str);
    }
    return return_statement;
}


