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

#define CFC_NEED_BASE_STRUCT_DEF

#include "CFCBase.h"
#include "CFCPyMethod.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCMethod.h"
#include "CFCSymbol.h"
#include "CFCType.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCPyTypeMap.h"
#include "CFCVariable.h"

#ifndef true
#define true  1
#define false 0
#endif

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

static int
S_types_can_be_bound(CFCParamList *param_list, CFCType *return_type) {
    // Test whether parameters can be mapped automatically.
    CFCVariable  **arg_vars   = CFCParamList_get_variables(param_list);
    for (size_t i = 0; arg_vars[i] != NULL; i++) {
        CFCType *type = CFCVariable_get_type(arg_vars[i]);
        if (!CFCType_is_object(type) && !CFCType_is_primitive(type)) {
            return false;
        }

        // TODO: kill this off after changing ClassSpec so that it doesn't
        // look like a legit object type (it's just a struct).
        if (strcmp("cfish_ClassSpec", CFCType_get_specifier(type)) == 0) {
            return false;
        }
        if (strcmp("cfish_Thread", CFCType_get_specifier(type)) == 0) {
            return false;
        }
    }

    // Test whether return type can be mapped automatically.
    if (!CFCType_is_void(return_type)
        && !CFCType_is_object(return_type)
        && !CFCType_is_primitive(return_type)
       ) {
        return false;
    }

    return true;
}

struct CFCPyMethod {
    CFCBase base;
    char *name;
    char *py_method_def;
};

static const CFCMeta CFCPYMETHOD_META = {
    "Clownfish::CFC::Binding::Python::Method",
    sizeof(CFCPyMethod),
    (CFCBase_destroy_t)CFCPyMethod_destroy
};

CFCPyMethod*
CFCPyMethod_new(const char *name, const char *py_method_def) {
    CFCUTIL_NULL_CHECK(name);
    CFCUTIL_NULL_CHECK(py_method_def);
    CFCPyMethod *self = (CFCPyMethod*)CFCBase_allocate(&CFCPYMETHOD_META);
    self->name = CFCUtil_strdup(name);
    self->py_method_def = CFCUtil_strdup(py_method_def);
    return self;
}

void
CFCPyMethod_destroy(CFCPyMethod *self) {
    FREEMEM(self->name);
    FREEMEM(self->py_method_def);
    CFCBase_destroy((CFCBase*)self);
}

const char*
CFCPyMethod_get_name(CFCPyMethod *self) {
    return self->name;
}


const char*
CFCPyMethod_get_py_method_def(CFCPyMethod *self) {
    return self->py_method_def;
}

int
CFCPyMethod_can_be_bound(CFCMethod *method) {
    if (CFCSymbol_private((CFCSymbol*)method)) {
        return false;
    }
    return S_types_can_be_bound(CFCMethod_get_param_list(method),
                                CFCMethod_get_return_type(method));
}

int
CFCPyMethod_func_can_be_bound(CFCFunction *func) {
    return S_types_can_be_bound(CFCFunction_get_param_list(func),
                                CFCFunction_get_return_type(func));
}

static char*
S_build_py_args(CFCParamList *param_list) {
    int num_vars = CFCParamList_num_vars(param_list);
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    char pattern[] = "    PyObject *cfcb_ARGS = S_pack_tuple(%d";
    char *py_args = CFCUtil_sprintf(pattern, num_vars - 1);

    for (int i = 1; vars[i] != NULL; i++) {
        const char *var_name = CFCVariable_get_name(vars[i]);
        CFCType *type = CFCVariable_get_type(vars[i]);
        char *conversion = CFCPyTypeMap_c_to_py(type, var_name);
        py_args = CFCUtil_cat(py_args, ",\n        ", conversion, NULL);
        FREEMEM(conversion);
    }
    py_args = CFCUtil_cat(py_args, ");", NULL);

    return py_args;
}

static struct {
    const char *key;
    const char *value;
} run_trapped_primitive_map[] = {
    {"int8_t", "CFBIND_RUN_TRAPPED_int8_t"},
    {"int16_t", "CFBIND_RUN_TRAPPED_int16_t"},
    {"int32_t", "CFBIND_RUN_TRAPPED_int32_t"},
    {"int64_t", "CFBIND_RUN_TRAPPED_int64_t"},
    {"uint8_t", "CFBIND_RUN_TRAPPED_uint8_t"},
    {"uint16_t", "CFBIND_RUN_TRAPPED_uint16_t"},
    {"uint32_t", "CFBIND_RUN_TRAPPED_uint32_t"},
    {"uint64_t", "CFBIND_RUN_TRAPPED_uint64_t"},
    {"char", "CFBIND_RUN_TRAPPED_char"},
    {"short", "CFBIND_RUN_TRAPPED_short"},
    {"int", "CFBIND_RUN_TRAPPED_int"},
    {"long", "CFBIND_RUN_TRAPPED_long"},
    {"size_t", "CFBIND_RUN_TRAPPED_size_t"},
    {"bool", "CFBIND_RUN_TRAPPED_bool"},
    {"float", "CFBIND_RUN_TRAPPED_float"},
    {"double", "CFBIND_RUN_TRAPPED_double"},
    {NULL, NULL}
};

static const char*
S_choose_run_trapped(CFCType *return_type) {
    if (CFCType_is_void(return_type)) {
        return "CFBIND_RUN_TRAPPED_void";
    }
    else if (CFCType_incremented(return_type)) {
        return "CFBIND_RUN_TRAPPED_inc_obj";
    }
    else if (CFCType_is_object(return_type)) {
        return "CFBIND_RUN_TRAPPED_obj";
    }
    else if (CFCType_is_primitive(return_type)) {
        const char *specifier = CFCType_get_specifier(return_type);
        for (int i = 0; run_trapped_primitive_map[i].key != NULL; i++) {
            if (strcmp(run_trapped_primitive_map[i].key, specifier) == 0) {
                return run_trapped_primitive_map[i].value;
            }
        }
    }

    CFCUtil_die("Unexpected return type: %s",
                CFCType_to_c(return_type));
    return NULL; // Unreachable
}

static struct {
    const char *key;
    const char *value;
} any_t_member_map[] = {
    {"int8_t", "int8_t_"},
    {"int16_t", "int16_t_"},
    {"int32_t", "int32_t_"},
    {"int64_t", "int64_t_"},
    {"uint8_t", "uint8_t_"},
    {"uint16_t", "uint16_t_"},
    {"uint32_t", "uint32_t_"},
    {"uint64_t", "uint64_t_"},
    {"char", "char_"},
    {"short", "short_"},
    {"int", "int_"},
    {"long", "long_"},
    {"size_t", "size_t_"},
    {"bool", "bool_"},
    {"float", "float_"},
    {"double", "double_"},
    {NULL, NULL}
};

static const char*
S_choose_any_t(CFCType *type) {
    if (CFCType_is_object(type)) {
        return "ptr";
    }
    else if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);
        for (int i = 0; any_t_member_map[i].key != NULL; i++) {
            if (strcmp(any_t_member_map[i].key, specifier) == 0) {
                return any_t_member_map[i].value;
            }
        }
    }
    CFCUtil_die("Unexpected return type: %s",
                CFCType_to_c(type));
    return NULL; // Unreachable
}

static char*
S_gen_declaration(CFCVariable *var, const char *val, int tick) {
    CFCType *type = CFCVariable_get_type(var);
    const char *var_name = CFCVariable_get_name(var);
    const char *type_str = CFCType_to_c(type);
    char *result = NULL;

    if (CFCType_is_object(type)) {
        const char *specifier = CFCType_get_specifier(type);
        if (strcmp(specifier, "cfish_String") == 0) {
            if (val && strcmp(val, "NULL") != 0) {
                const char pattern[] =
                    "    const char arg_%s_DEFAULT[] = %s;\n"
                    "    cfargs[%d].ptr = CFISH_SSTR_WRAP_UTF8(\n"
                    "        arg_%s_DEFAULT, sizeof(arg_%s_DEFAULT) - 1);\n"
                    "    CFBindStringArg wrap_arg_%s = {&cfargs[%d].ptr, cfargs[%d].ptr, NULL};\n"
                    ;
                result = CFCUtil_sprintf(pattern, var_name, val, tick,
                                         var_name, var_name, var_name,
                                         tick, tick);
            }
            else {
                const char pattern[] =
                    "    CFBindStringArg wrap_arg_%s = {&cfargs[%d].ptr, CFISH_ALLOCA_OBJ(CFISH_STRING), NULL};\n"
                    ;
                result = CFCUtil_sprintf(pattern, var_name, tick);
            }
        }
        else {
            if (val && strcmp(val, "NULL") != 0) {
                CFCUtil_die("Can't assign a default of '%s' to a %s",
                            val, type_str);
            }
            if (strcmp(specifier, "cfish_Hash") != 0
                && strcmp(specifier, "cfish_VArray") != 0
                ) {
                const char *class_var = CFCType_get_class_var(type);
                char pattern[] =
                    "    CFBindArg wrap_arg_%s = {%s, &cfargs[%d].ptr};\n"
                    ;
                result = CFCUtil_sprintf(pattern, var_name, class_var, tick);
            }
        }
    }
    else if (CFCType_is_primitive(type)) {
        ;
    }
    else {
        CFCUtil_die("Unexpected type, can't gen declaration: %s", type_str);
    }

    return result;
}

static char*
S_gen_target(CFCVariable *var, const char *value, int tick) {
    CFCType *type = CFCVariable_get_type(var);
    const char *specifier = CFCType_get_specifier(type);
    const char *micro_sym = CFCVariable_get_name(var);
    const char *maybe_maybe = "";
    const char *dest_name;
    char *var_name = NULL;
    if (CFCType_is_primitive(type)) {
        dest_name = CFCType_get_specifier(type);
        if (value != NULL) {
            maybe_maybe = "maybe_";
        }
        var_name = CFCUtil_sprintf("cfargs[%d].%s_", tick, dest_name);
    }
    else if (CFCType_is_object(type)) {
        if (strcmp(specifier, "cfish_String") == 0) {
            dest_name = "string";
            var_name = CFCUtil_sprintf("wrap_arg_%s", micro_sym);
        }
        else if (strcmp(specifier, "cfish_Hash") == 0) {
            dest_name = "hash";
            var_name = CFCUtil_sprintf("cfargs[%d].ptr", tick);
        }
        else if (strcmp(specifier, "cfish_VArray") == 0) {
            dest_name = "array";
            var_name = CFCUtil_sprintf("cfargs[%d].ptr", tick);
        }
        else {
            dest_name = "obj";
            var_name = CFCUtil_sprintf("wrap_arg_%s", micro_sym);
        }
    }
    else {
        dest_name = "INVALID";
    }
    char *content = CFCUtil_sprintf(", CFBind_%sconvert_%s, &%s",
                                    maybe_maybe, dest_name, var_name);
    FREEMEM(var_name);
    return content;
}

static char*
S_gen_arg_parsing(CFCParamList *param_list, int first_tick, char **error) {
    char *content = NULL;

    CFCVariable **vars = CFCParamList_get_variables(param_list);
    const char **vals = CFCParamList_get_initial_values(param_list);
    int num_vars = CFCParamList_num_vars(param_list);

    char *declarations = CFCUtil_strdup("");
    char *keywords     = CFCUtil_strdup("");
    char *format_str   = CFCUtil_strdup("");
    char *targets      = CFCUtil_strdup("");
    int optional_started = 0;

    for (int i = first_tick; i < num_vars; i++) {
        CFCVariable *var  = vars[i];
        const char  *val  = vals[i];

        const char *var_name = CFCVariable_get_name(var);
        keywords = CFCUtil_cat(keywords, "\"", var_name, "\", ", NULL);

        // Build up ParseTuple format string.
        if (val == NULL) {
            if (optional_started) { // problem!
                *error = "Optional after required param";
                goto CLEAN_UP_AND_RETURN;
            }
        }
        else {
            if (!optional_started) {
                optional_started = 1;
                format_str = CFCUtil_cat(format_str, "|", NULL);
            }
        }
        format_str = CFCUtil_cat(format_str, "O&", NULL);

        char *declaration = S_gen_declaration(var, val, i);
        declarations = CFCUtil_cat(declarations, declaration, NULL);
        FREEMEM(declaration);

        char *target = S_gen_target(var, val, i);
        targets = CFCUtil_cat(targets, target, NULL);
        FREEMEM(target);
    }

    char parse_pattern[] =
        "    CFBindTrapContext context = {{0}};\n"
        "    cfbind_any_t cfargs[%d] = {{0}};\n"
        "    context.args = cfargs;\n"
        "%s"
        "    char *keywords[] = {%sNULL};\n"
        "    char *fmt = \"%s\";\n"
        "    int ok = PyArg_ParseTupleAndKeywords(args, kwargs, fmt,\n"
        "        keywords%s);\n"
        "    if (!ok) { return NULL; }\n"
        ;
    content = CFCUtil_sprintf(parse_pattern, num_vars, declarations, keywords,
                              format_str, targets);

CLEAN_UP_AND_RETURN:
    FREEMEM(declarations);
    FREEMEM(keywords);
    FREEMEM(format_str);
    FREEMEM(targets);
    return content;
}

static char*
S_build_pymeth_invocation(CFCMethod *method) {
    CFCType *return_type = CFCMethod_get_return_type(method);
    const char *micro_sym = CFCSymbol_get_name((CFCSymbol*)method);
    char *invocation = NULL;
    const char *ret_type_str = CFCType_to_c(return_type);

    if (CFCType_is_void(return_type)) {
        const char pattern[] =
            "    CALL_PYMETH_VOID((PyObject*)self, \"%s\", cfcb_ARGS);";
        invocation = CFCUtil_sprintf(pattern, micro_sym);
    }
    else if (CFCType_is_object(return_type)) {
        const char *nullable
            = CFCType_nullable(return_type) ? "true" : "false";
        const char *ret_class = CFCType_get_class_var(return_type);
        const char pattern[] =
            "    %s cfcb_RESULT = (%s)CALL_PYMETH_OBJ((PyObject*)self, \"%s\", cfcb_ARGS, %s, %s);";
        invocation = CFCUtil_sprintf(pattern, ret_type_str, ret_type_str, micro_sym,
                                     ret_class, nullable);
    }
    else if (CFCType_is_primitive(return_type)) {
        char type_upcase[64];
        if (strlen(ret_type_str) > 63) {
            CFCUtil_die("Unexpectedly long type name: %s", ret_type_str);
        }
        for (int i = 0, max = strlen(ret_type_str) + 1; i < max; i++) {
            type_upcase[i] = toupper(ret_type_str[i]);
        }
        const char pattern[] =
            "    %s cfcb_RESULT = CALL_PYMETH_%s((PyObject*)self, \"%s\", cfcb_ARGS);";
        invocation = CFCUtil_sprintf(pattern, ret_type_str, type_upcase,
                                     micro_sym);
    }
    else {
        CFCUtil_die("Unexpected return type: %s", CFCType_to_c(return_type));
    }

    return invocation;
}

static char*
S_callback_refcount_mods(CFCParamList *param_list) {
    char *refcount_mods = CFCUtil_strdup("");
    CFCVariable **arg_vars = CFCParamList_get_variables(param_list);

    // Adjust refcounts of arguments per method signature, so that Perl code
    // does not have to.
    for (int i = 0; arg_vars[i] != NULL; i++) {
        CFCVariable *var  = arg_vars[i];
        CFCType     *type = CFCVariable_get_type(var);
        const char  *name = CFCVariable_get_name(var);
        if (!CFCType_is_object(type)) {
            continue;
        }   
        else if (CFCType_incremented(type)) {
            refcount_mods = CFCUtil_cat(refcount_mods, "    CFISH_INCREF(",
                                        name, ");\n", NULL);
        }   
        else if (CFCType_decremented(type)) {
            refcount_mods = CFCUtil_cat(refcount_mods, "    CFISH_DECREF(",
                                        name, ");\n", NULL);
        }   
    }   

    return refcount_mods;
}

char*
CFCPyMethod_callback_def(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list   = CFCMethod_get_param_list(method);
    CFCVariable **vars         = CFCParamList_get_variables(param_list);
    CFCType      *return_type  = CFCMethod_get_return_type(method);
    const char   *ret_type_str = CFCType_to_c(return_type);
    const char   *params       = CFCParamList_to_c(param_list);
    char         *override_sym = CFCMethod_full_override_sym(method, invoker);
    char *content;

    if (CFCPyMethod_can_be_bound(method)) {
        char *py_args = S_build_py_args(param_list);
        char *invocation = S_build_pymeth_invocation(method);
        char *refcount_mods = S_callback_refcount_mods(param_list);
        const char *maybe_return = CFCType_is_void(return_type)
                                   ? ""
                                   : "    return cfcb_RESULT;\n";

        const char pattern[] = 
            "%s\n"
            "%s(%s) {\n"
            "%s\n"
            "%s\n"
            "%s"
            "%s"
            "}\n";
        content = CFCUtil_sprintf(pattern, ret_type_str, override_sym, params,
                                  py_args, invocation, refcount_mods,
                                  maybe_return);
    }
    else {
        char *unused = S_build_unused_vars(vars);
        char *unreachable = S_maybe_unreachable(return_type);
        char *meth_sym = CFCMethod_full_method_sym(method, invoker);
        const char pattern[] =
            "%s\n"
            "%s(%s) {%s\n"
            "    CFISH_THROW(CFISH_ERR, \"Can't override %s via binding\");%s\n"
            "}\n";
        content = CFCUtil_sprintf(pattern, ret_type_str, override_sym,
                                  params, unused, meth_sym, unreachable);
        FREEMEM(meth_sym);
        FREEMEM(unused);
        FREEMEM(unreachable);
    }

    FREEMEM(override_sym);
    return content;
}

static char*
S_build_unused_vars(CFCVariable **vars) {
    char *unused = CFCUtil_strdup("");

    for (int i = 0; vars[i] != NULL; i++) {
        const char *var_name = CFCVariable_get_name(vars[i]);
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

// Prep any decrefs for running inside `CFBind_run_trapped`.
char*
S_gen_trap_decrefs(CFCParamList *param_list, int first_tick) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    int num_decrefs = 0;
    char *decrefs = CFCUtil_strdup("");

    for (int i = first_tick; i < num_vars; i++) {
        CFCVariable *var = vars[i];
        CFCType *type = CFCVariable_get_type(var);
        const char *micro_sym = CFCVariable_get_name(var);
        const char *specifier = CFCType_get_specifier(type);

        if (strcmp(specifier, "cfish_String") == 0) {
            char pattern[] =
                "%s    decrefs[%d].ptr = wrap_arg_%s.stringified;\n";
            char *temp = CFCUtil_sprintf(pattern, decrefs, num_decrefs,
                         micro_sym);
            FREEMEM(decrefs);
            decrefs = temp;
            num_decrefs++;
        }
        else if (strcmp(specifier, "cfish_Hash") == 0
                 || strcmp(specifier, "cfish_VArray") == 0
                ) {
            char pattern[] = "%s    decrefs[%d].ptr = cfargs[%d].ptr;\n";
            char *temp = CFCUtil_sprintf(pattern, decrefs, num_decrefs, i);
            FREEMEM(decrefs);
            decrefs = temp;
            num_decrefs++;
        }
    }

    if (num_decrefs) {
        char pattern[] =
            "    cfbind_any_t decrefs[%d];\n"
            "    context.decrefs = decrefs;\n"
            "%s"
            ;
        char *temp = CFCUtil_sprintf(pattern, num_decrefs, decrefs);
        FREEMEM(decrefs);
        decrefs = temp;
    }

    return decrefs;
}

char*
S_meth_top(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list = CFCMethod_get_param_list(method);

    if (CFCParamList_num_vars(param_list) == 1) {
        char pattern[] =
            "(PyObject *self, PyObject *unused) {\n"
            "    CFISH_UNUSED_VAR(unused);\n"
            "    CFBindTrapContext context = {{0}};\n"
            "    cfbind_any_t cfargs[1] = {{0}};\n"
            "    context.args = cfargs;\n"
            ;
        return CFCUtil_sprintf(pattern);
    }
    else {
        char *error = NULL;
        char *arg_parsing = S_gen_arg_parsing(param_list, 1, &error);
        if (error) {
            CFCUtil_die("%s in %s", error, CFCMethod_get_name(method));
        }
        if (!arg_parsing) {
            return NULL;
        }
        char pattern[] =
            "(PyObject *self, PyObject *args, PyObject *kwargs) {\n"
            "%s"
            ;
        char *result = CFCUtil_sprintf(pattern, arg_parsing);
        FREEMEM(arg_parsing);
        return result;
    }
}

char*
S_static_meth_top(CFCFunction *func) {
    CFCParamList *param_list = CFCFunction_get_param_list(func);

    if (CFCParamList_num_vars(param_list) == 0) {
        char pattern[] =
            "(PyObject *unused1, PyObject *unused2) {\n"
            "    CFISH_UNUSED_VAR(unused1);\n"
            "    CFISH_UNUSED_VAR(unused2);\n"
            ;
        return CFCUtil_strdup(pattern);
    }
    else {
        char *error = NULL;
        char *arg_parsing = S_gen_arg_parsing(param_list, 0, &error);
        if (error) {
            CFCUtil_die("%s in %s", error, CFCFunction_get_name(func));
        }
        if (!arg_parsing) {
            return NULL;
        }
        char pattern[] =
            "(PyObject *unused, PyObject *args, PyObject *kwargs) {\n"
            "    CFISH_UNUSED_VAR(unused);\n"
            "%s"
            ;
        char *result = CFCUtil_sprintf(pattern, arg_parsing);
        FREEMEM(arg_parsing);
        return result;
    }
}

char*
S_gen_arg_increfs(CFCParamList *param_list, int first_tick) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    char *content = CFCUtil_strdup("");
    for (int i = first_tick;i < num_vars; i++) {
        CFCType *type = CFCVariable_get_type(vars[i]);
        if (CFCType_decremented(type)) {
            char pattern[] =
                "    cfargs[%d].ptr = CFISH_INCREF(cfargs[%d].ptr);\n";
            char *incref = CFCUtil_sprintf(pattern, i, i);
            content = CFCUtil_cat(content, incref, NULL);
            FREEMEM(incref);
        }
    }
    return content;
}

char*
S_gen_decrefs(CFCParamList *param_list, int first_tick) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    char *content = CFCUtil_strdup("");
    for (int i = first_tick;i < num_vars; i++) {
        CFCType *type = CFCVariable_get_type(vars[i]);
        if (!CFCType_is_object(type)) {
            continue;
        }
        const char *specifier = CFCType_get_specifier(type);
        const char *var_name = CFCVariable_get_name(vars[i]);
        if (strcmp(specifier, "cfish_String") == 0) {
            content = CFCUtil_cat(content, "    CFISH_DECREF(wrap_arg_",
                                  var_name, ".stringified);\n", NULL);
        }
        else if (strcmp(specifier, "cfish_Hash") == 0
                 || strcmp(specifier, "cfish_VArray") == 0
           ) {
            content = CFCUtil_cat(content, "    CFISH_DECREF(arg_",
                                  var_name, ");\n", NULL);
        }
    }
    return content;
}

char*
S_gen_trap_arg_list(CFCParamList *param_list) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    char *arg_list = CFCUtil_strdup("");
    for (int i = 0; i < num_vars; i++) {
        if (i > 0) {
            arg_list = CFCUtil_cat(arg_list, ", ", NULL);
        }
        CFCType *type = CFCVariable_get_type(vars[i]);
        const char *type_str = CFCType_to_c(type);
        char *new_arg_list;
        if (CFCType_is_primitive(type)) {
            new_arg_list = CFCUtil_sprintf("%scontext->args[%d].%s_",
                                           arg_list, i, type_str);
            FREEMEM(arg_list);
            arg_list = new_arg_list;
        }
        else if (CFCType_is_object(type)) {
            new_arg_list = CFCUtil_sprintf("%s(%s)context->args[%d].ptr",
                               arg_list, type_str, i);
            FREEMEM(arg_list);
            arg_list = new_arg_list;
        }
        else {
            CFCUtil_die("Unexpected type: %s", type_str);
        }
    }
    return arg_list;
}

char*
S_gen_meth_trap(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list = CFCMethod_get_param_list(method);
    char *meth_type_c = CFCMethod_full_typedef(method, invoker);
    char *full_meth = CFCMethod_full_method_sym(method, invoker);
    const char *class_var = CFCClass_full_class_var(invoker);
    char *arg_list = S_gen_trap_arg_list(param_list);

    CFCType *return_type = CFCMethod_get_return_type(method);
    char *maybe_retval = CFCUtil_strdup("");
    if (CFCType_is_void(return_type)) {
        maybe_retval = CFCUtil_strdup("");
    }
    else {
        maybe_retval = CFCUtil_sprintf("context->retval.%s = ",
                                       S_choose_any_t(return_type));
    }

    const char pattern[] =
        "static void\n"
        "S_run_%s(void *vcontext) {\n"
        "    CFBindTrapContext *context = (CFBindTrapContext*)vcontext;\n"
        "    %s method = CFISH_METHOD_PTR(%s, %s);\n"
        "    %smethod(%s);\n"
        "}\n"
        ;
    char *content
        = CFCUtil_sprintf(pattern, full_meth, meth_type_c, class_var,
                          full_meth, maybe_retval, arg_list);

    FREEMEM(arg_list);
    FREEMEM(maybe_retval);
    FREEMEM(full_meth);
    FREEMEM(meth_type_c);
    return content;
}

char*
S_gen_constructor_trap(CFCFunction *init_func, CFCClass *invoker) {
    CFCParamList *param_list = CFCFunction_get_param_list(init_func);
    char *init_func_str = CFCFunction_full_func_sym(init_func, invoker);
    char *arg_list = S_gen_trap_arg_list(param_list);

    const char pattern[] =
        "static void\n"
        "S_run_%s_AS_NEW(void *vcontext) {\n"
        "    CFBindTrapContext *context = (CFBindTrapContext*)vcontext;\n"
        "    context->retval.ptr = %s(%s);\n"
        "}\n"
        ;
    char *content = CFCUtil_sprintf(pattern, init_func_str, init_func_str,
                                    arg_list);

    FREEMEM(arg_list);
    FREEMEM(init_func_str);
    return content;
}

char*
S_gen_inert_func_trap(CFCFunction *func, CFCClass *invoker) {
    CFCParamList *param_list = CFCFunction_get_param_list(func);
    char *func_sym = CFCFunction_full_func_sym(func, invoker);
    char *arg_list = S_gen_trap_arg_list(param_list);

    CFCType *return_type = CFCFunction_get_return_type(func);
    char *maybe_retval = CFCUtil_strdup("");
    if (CFCType_is_void(return_type)) {
        maybe_retval = CFCUtil_strdup("");
    }
    else {
        maybe_retval = CFCUtil_sprintf("context->retval.%s = ",
                                       S_choose_any_t(return_type));
    }

    const char *maybe_context;
    if (CFCType_is_void(return_type) && !CFCParamList_num_vars(param_list)) {
        maybe_context = "    CFISH_UNUSED_VAR(vcontext);\n";

    }
    else {
        maybe_context
            = "    CFBindTrapContext *context = (CFBindTrapContext*)vcontext;\n";
    }

    const char pattern[] =
        "static void\n"
        "S_run_%s(void *vcontext) {\n"
        "%s"
        "    %s%s(%s);\n"
        "}\n"
        ;
    char *content = CFCUtil_sprintf(pattern, func_sym, maybe_context,
                                    maybe_retval, func_sym, arg_list);

    FREEMEM(arg_list);
    FREEMEM(maybe_retval);
    FREEMEM(func_sym);
    return content;
}

char*
S_gen_return_statement(CFCType *return_type) {
    if (CFCType_is_void(return_type)) {
        return CFCUtil_strdup("    Py_RETURN_NONE;\n");
    }
    else if (CFCType_incremented(return_type)) {
        return CFCUtil_strdup(
            "    return CFBind_cfish_to_py_zeroref((cfish_Obj*)retval);\n");
    }
    else {
        char *conversion = CFCPyTypeMap_c_to_py(return_type, "retval");
        char *content = CFCUtil_sprintf("    return %s;\n", conversion);
        FREEMEM(conversion);
        return content;
    }
}

char*
CFCPyMethod_wrapper(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list  = CFCMethod_get_param_list(method);
    CFCType      *return_type = CFCMethod_get_return_type(method);
    char *meth_trap  = S_gen_meth_trap(method, invoker);
    char *meth_sym   = CFCMethod_full_method_sym(method, invoker);
    char *meth_top   = S_meth_top(method, invoker);
    char *increfs    = S_gen_arg_increfs(param_list, 1);
    char *decrefs    = S_gen_trap_decrefs(param_list, 1);
    const char *run_trapped = S_choose_run_trapped(return_type);

    char pattern[] =
        "%s\n"
        "static PyObject*\n"
        "S_%s%s"
        "%s" // increfs
        "    cfargs[0].ptr = self;\n"
        "%s" // decrefs
        "    return %s(S_run_%s, &context);\n" // return statement
        "}\n"
        ;
    char *wrapper = CFCUtil_sprintf(pattern, meth_trap, meth_sym, meth_top,
                                    increfs, decrefs, run_trapped,
                                    meth_sym);
    FREEMEM(decrefs);
    FREEMEM(increfs);
    FREEMEM(meth_sym);
    FREEMEM(meth_top);
    FREEMEM(meth_trap);
    return wrapper;
}

char*
CFCPyMethod_constructor_wrapper(CFCFunction *init_func, CFCClass *invoker) {
    CFCParamList *param_list  = CFCFunction_get_param_list(init_func);
    char *trap_wrap  = S_gen_constructor_trap(init_func, invoker);
    char *increfs    = S_gen_arg_increfs(param_list, 1);
    char *decrefs    = S_gen_trap_decrefs(param_list, 1);
    const char *class_var  = CFCClass_full_class_var(invoker);
    const char *struct_sym = CFCClass_full_struct_sym(invoker);
    char *func_name = CFCFunction_full_func_sym(init_func, invoker);
    char *error = NULL;
    char *arg_parsing = S_gen_arg_parsing(param_list, 1, &error);
    if (error) {
        CFCUtil_die("%s in constructor for %s", error,
                    CFCClass_get_name(invoker));
    }
    if (!arg_parsing) {
        CFCUtil_die("Unexpected arg parsing error for %s",
                    CFCClass_get_name(invoker));
    }

    char pattern[] =
        "%s\n" // trap_wrap
        "static PyObject*\n"
        "S_%s_PY_NEW(PyTypeObject *type, PyObject *args, PyObject *kwargs) {\n"
        "%s" // arg_parsing
        "%s" // increfs
        "    context.args[0].ptr = CFISH_Class_Make_Obj(%s);\n"
        "%s" // decrefs
        "    return CFBIND_RUN_TRAPPED_raw_obj(S_run_%s_AS_NEW, &context);\n"
        "}\n"
        ;
    char *wrapper = CFCUtil_sprintf(pattern, trap_wrap, struct_sym,
                                    arg_parsing, increfs, class_var,
                                    decrefs, func_name);
    FREEMEM(func_name);
    FREEMEM(decrefs);
    FREEMEM(increfs);
    FREEMEM(arg_parsing);
    FREEMEM(trap_wrap);
    return wrapper;
}

char*
CFCPyFunc_inert_wrapper(CFCFunction *func, struct CFCClass *invoker) {
    CFCParamList *param_list  = CFCFunction_get_param_list(func);
    CFCType      *return_type = CFCFunction_get_return_type(func);
    char *func_sym   = CFCFunction_full_func_sym(func, invoker);
    char *func_trap  = S_gen_inert_func_trap(func, invoker);
    char *top        = S_static_meth_top(func);
    char *increfs    = S_gen_arg_increfs(param_list, 0);
    char *decrefs    = S_gen_trap_decrefs(param_list, 0);
    const char *run_trapped = S_choose_run_trapped(return_type);
    const char *maybe_context_arg = CFCParamList_num_vars(param_list)
                                    ? "&context" : "NULL";

    char pattern[] =
        "%s\n" // trap func
        "static PyObject*\n"
        "S_%s%s"
        "%s" // increfs
        "%s" // decrefs
        "    return %s(S_run_%s, %s);\n"
        "}\n"
        ;
    char *wrapper = CFCUtil_sprintf(pattern, func_trap, func_sym, top,
                                    increfs, decrefs, run_trapped,
                                    func_sym, maybe_context_arg);
    FREEMEM(decrefs);
    FREEMEM(increfs);
    FREEMEM(top);
    FREEMEM(func_trap);
    FREEMEM(func_sym);
    return wrapper;
}

char*
CFCPyMethod_pymethoddef(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list = CFCMethod_get_param_list(method);
    const char *flags = CFCParamList_num_vars(param_list) == 1
                        ? "METH_NOARGS"
                        : "METH_KEYWORDS|METH_VARARGS";
    const char *micro_sym = CFCSymbol_get_name((CFCSymbol*)method);
    char *meth_sym = CFCMethod_full_method_sym(method, invoker);
    char pattern[] =
        "{\"%s\", (PyCFunction)S_%s, %s, NULL},";
    char *py_meth_def = CFCUtil_sprintf(pattern, micro_sym, meth_sym, flags);
    FREEMEM(meth_sym);
    return py_meth_def;
}

char*
CFCPyFunc_static_pymethoddef(CFCFunction *func, CFCClass *invoker) {
    CFCParamList *param_list = CFCFunction_get_param_list(func);
    const char *flags = CFCParamList_num_vars(param_list) == 0
                        ? "METH_STATIC|METH_NOARGS"
                        : "METH_STATIC|METH_KEYWORDS|METH_VARARGS";
    const char *micro_sym = CFCSymbol_get_name((CFCSymbol*)func);
    char       *func_sym  = CFCFunction_full_func_sym(func, invoker);
    char pattern[] =
        "{\"%s\", (PyCFunction)S_%s, %s, NULL},";
    char *retval = CFCUtil_sprintf(pattern, micro_sym, func_sym, flags);
    FREEMEM(func_sym);
    return retval;
}

