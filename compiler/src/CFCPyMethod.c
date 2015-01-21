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

int
CFCPyMethod_can_be_bound(CFCMethod *method) {
    if (CFCSymbol_private((CFCSymbol*)method)) {
        return false;
    }

    // Test whether parameters can be mapped automatically.
    CFCParamList  *param_list = CFCMethod_get_param_list(method);
    CFCVariable  **arg_vars   = CFCParamList_get_variables(param_list);
    for (size_t i = 0; arg_vars[i] != NULL; i++) {
        CFCType *type = CFCVariable_get_type(arg_vars[i]);
        if (!CFCType_is_object(type) && !CFCType_is_primitive(type)) {
            return false;
        }
    }

    // Test whether return type can be mapped automatically.
    CFCType *return_type = CFCMethod_get_return_type(method);
    if (!CFCType_is_void(return_type)
        && !CFCType_is_object(return_type)
        && !CFCType_is_primitive(return_type)
       ) {
        return false;
    }

    return true;
}

static char*
S_build_py_args(CFCParamList *param_list) {
    int num_vars = CFCParamList_num_vars(param_list);
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    char pattern[] = "    PyObject *cfcb_ARGS = S_pack_tuple(%d";
    char *py_args = CFCUtil_sprintf(pattern, num_vars - 1);

    for (int i = 1; vars[i] != NULL; i++) {
        const char *var_name = CFCVariable_micro_sym(vars[i]);
        CFCType *type = CFCVariable_get_type(vars[i]);
        char *conversion = CFCPyTypeMap_c_to_py(type, var_name);
        py_args = CFCUtil_cat(py_args, ",\n        ", conversion, NULL);
        FREEMEM(conversion);
    }
    py_args = CFCUtil_cat(py_args, ");", NULL);

    return py_args;
}

static char*
S_gen_declaration(CFCVariable *var, const char *val) {
    CFCType *type = CFCVariable_get_type(var);
    const char *var_name = CFCVariable_micro_sym(var);
    const char *type_str = CFCType_to_c(type);
    char *result = NULL;

    if (CFCType_is_object(type)) {
        const char *specifier = CFCType_get_specifier(type);
        if (strcmp(specifier, "cfish_String") == 0) {
            if (val && strcmp(val, "NULL") != 0) {
                const char pattern[] =
                    "    const char arg_%s_DEFAULT[] = %s;\n"
                    "    cfish_String* arg_%s = (cfish_String*)cfish_SStr_wrap_str(\n"
                    "        alloca(cfish_SStr_size()), arg_%s_DEFAULT, sizeof(arg_%s_DEFAULT) - 1);\n"
                    "    CFBindStringArg wrap_arg_%s = {&arg_%s, arg_%s, NULL};\n"
                    ;
                result = CFCUtil_sprintf(pattern, var_name, val, var_name,
                                         var_name, var_name, var_name,
                                         var_name, var_name);
            }
            else {
                const char pattern[] =
                    "    cfish_String* arg_%s = NULL;\n"
                    "    CFBindStringArg wrap_arg_%s = {&arg_%s, alloca(cfish_SStr_size()), NULL};\n"
                    ;
                result = CFCUtil_sprintf(pattern, var_name, var_name,
                                         var_name);
            }
        }
        else {
            if (val && strcmp(val, "NULL") != 0) {
                CFCUtil_die("Can't assign a default of '%s' to a %s",
                            val, type_str);
            }
            if (strcmp(specifier, "cfish_Hash") == 0
                || strcmp(specifier, "cfish_VArray") == 0
                ) {
                result = CFCUtil_sprintf("    %s arg_%s = NULL;\n", type_str,
                                         var_name);
            }
            else {
                const char *class_var = CFCType_get_class_var(type);
                char pattern[] =
                    "    %s arg_%s = NULL;\n"
                    "    CFBindArg wrap_arg_%s = {%s, &arg_%s};\n"
                    ;
                result = CFCUtil_sprintf(pattern, type_str, var_name,
                                         var_name, class_var, var_name);
            }
        }
    }
    else if (CFCType_is_primitive(type)) {
        if (val) {
            result = CFCUtil_sprintf("    %s arg_%s = %s;\n",
                                     type_str, var_name, val);
        }
        else {
            result = CFCUtil_sprintf("    %s arg_%s;\n",
                                     type_str, var_name);
        }
    }
    else {
        CFCUtil_die("Unexpected type, can't gen declaration: %s", type_str);
    }

    return result;
}

static char*
S_gen_target(CFCVariable *var, const char *value) {
    CFCType *type = CFCVariable_get_type(var);
    const char *specifier = CFCType_get_specifier(type);
    const char *var_name = CFCVariable_micro_sym(var);
    const char *maybe_maybe = "";
    const char *maybe_wrap = "";
    const char *dest_name;
    if (CFCType_is_primitive(type)) {
        dest_name = CFCType_get_specifier(type);
        if (value != NULL) {
            maybe_maybe = "maybe_";
        }
    }
    else if (CFCType_is_object(type)) {
        if (strcmp(specifier, "cfish_String") == 0) {
            dest_name = "string";
            maybe_wrap = "wrap_";
        }
        else if (strcmp(specifier, "cfish_Hash") == 0) {
            dest_name = "hash";
        }
        else if (strcmp(specifier, "cfish_VArray") == 0) {
            dest_name = "array";
        }
        else {
            dest_name = "obj";
            maybe_wrap = "wrap_";
        }
    }
    else {
        dest_name = "INVALID";
    }
    return CFCUtil_sprintf(", CFBind_%sconvert_%s, &%sarg_%s", maybe_maybe,
                           dest_name, maybe_wrap, var_name);
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

        const char *var_name = CFCVariable_micro_sym(var);
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

        char *declaration = S_gen_declaration(var, val);
        declarations = CFCUtil_cat(declarations, declaration, NULL);
        FREEMEM(declaration);

        char *target = S_gen_target(var, val);
        targets = CFCUtil_cat(targets, target, NULL);
        FREEMEM(target);
    }

    char parse_pattern[] =
        "%s"
        "    char *keywords[] = {%sNULL};\n"
        "    char *fmt = \"%s\";\n"
        "    int ok = PyArg_ParseTupleAndKeywords(args, kwargs, fmt,\n"
        "        keywords%s);\n"
        "    if (!ok) { return NULL; }\n"
        ;
    content = CFCUtil_sprintf(parse_pattern, declarations, keywords,
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
    const char *micro_sym = CFCSymbol_micro_sym((CFCSymbol*)method);
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
        const char  *name = CFCVariable_micro_sym(var);
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
CFCPyMethod_callback_def(CFCMethod *method) {
    CFCParamList *param_list   = CFCMethod_get_param_list(method);
    CFCVariable **vars         = CFCParamList_get_variables(param_list);
    CFCType      *return_type  = CFCMethod_get_return_type(method);
    const char   *ret_type_str = CFCType_to_c(return_type);
    const char   *override_sym = CFCMethod_full_override_sym(method);
    const char   *params       = CFCParamList_to_c(param_list);
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
        char *meth_sym = CFCMethod_full_method_sym(method, NULL);
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

char*
S_meth_top(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list = CFCMethod_get_param_list(method);
    const char *self_struct = CFCClass_full_struct_sym(invoker);
    const char *self_class_var = CFCClass_full_class_var(invoker);

    if (CFCParamList_num_vars(param_list) == 1) {
        char pattern[] =
            "(PyObject *py_self, PyObject *unused) {\n"
            "    %s* self;\n"
            "    CFBindArg wrap_self = {%s, &self};\n"
            "    if (!CFBind_convert_obj(py_self, &wrap_self)) {\n"
            "        return NULL;\n"
            "    }\n"
            "    CFISH_UNUSED_VAR(unused);\n"
            ;
        return CFCUtil_sprintf(pattern, self_struct, self_class_var);
    }
    else {
        char *error = NULL;
        char *arg_parsing = S_gen_arg_parsing(param_list, 1, &error);
        if (error) {
            CFCUtil_die("%s in %s", error, CFCMethod_get_macro_sym(method));
        }
        if (!arg_parsing) {
            return NULL;
        }
        char pattern[] =
            "(PyObject *py_self, PyObject *args, PyObject *kwargs) {\n"
            "    %s* self;\n"
            "    CFBindArg wrap_self = {%s, &self};\n"
            "    if (!CFBind_convert_obj(py_self, &wrap_self)) {\n"
            "        return NULL;\n"
            "    }\n"
            "%s"
            ;
        char *result = CFCUtil_sprintf(pattern, self_struct, self_class_var,
                                       arg_parsing);
        FREEMEM(arg_parsing);
        return result;
    }
}

char*
CFCPyMethod_wrapper(CFCMethod *method, CFCClass *invoker) {
    char *meth_sym = CFCMethod_full_method_sym(method, invoker);
    char *meth_top = S_meth_top(method, invoker);
    char pattern[] =
        "static PyObject*\n"
        "S_%s%s"
        "    Py_RETURN_NONE;\n"
        "}\n"
        ;
    char *wrapper = CFCUtil_sprintf(pattern, meth_sym, meth_top);
    FREEMEM(meth_sym);
    FREEMEM(meth_top);
    return wrapper;
}

char*
CFCPyMethod_pymethoddef(CFCMethod *method, CFCClass *invoker) {
    CFCParamList *param_list = CFCMethod_get_param_list(method);
    const char *flags = CFCParamList_num_vars(param_list) == 1
                        ? "METH_NOARGS"
                        : "METH_KEYWORDS|METH_VARARGS";
    const char *micro_sym = CFCSymbol_micro_sym((CFCSymbol*)method);
    char *meth_sym = CFCMethod_full_method_sym(method, invoker);
    char pattern[] =
        "{\"%s\", (PyCFunction)S_%s, %s, NULL},";
    char *py_meth_def = CFCUtil_sprintf(pattern, micro_sym, meth_sym, flags);
    FREEMEM(meth_sym);
    return py_meth_def;
}
