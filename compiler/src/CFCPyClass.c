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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "CFCPyClass.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCPyMethod.h"

char*
CFCPyClass_pytype_struct_def(CFCClass *klass, const char *pymod_name) {
    const char *struct_sym = CFCClass_get_struct_sym(klass);

    char *tp_new;
    CFCFunction *init_func = CFCClass_function(klass, "init");
    if (init_func && CFCPyMethod_func_can_be_bound(init_func)) {
        tp_new = CFCUtil_sprintf("S_%s_PY_NEW",
                                 CFCClass_full_struct_sym(klass));
    }
    else {
        tp_new = CFCUtil_strdup("0");
    }

    char pattern[] =
        "static PyTypeObject %s_pytype_struct = {\n"
        "    PyVarObject_HEAD_INIT(NULL, 0)\n"
        "    \"%s.%s\", // tp_name\n"
        "    0,                                  // tp_basicsize THIS IS A LIE!\n"
        "    0,                                  // tp_itemsize\n"
        "    0,                                  // tp_dealloc\n"
        "    0,                                  // tp_print\n"
        "    0,                                  // tp_getattr\n"
        "    0,                                  // tp_setattr\n"
        "    0,                                  // tp_reserved\n"
        "    0,                                  // tp_repr\n"
        "    0,                                  // tp_as_number\n"
        "    0,                                  // tp_as_sequence\n"
        "    0,                                  // tp_as_mapping\n"
        "    0,                                  // tp_hash\n"
        "    0,                                  // tp_call\n"
        "    0,                                  // tp_str\n"
        "    0,                                  // tp_getattro\n"
        "    0,                                  // tp_setattro\n"
        "    0,                                  // tp_as_buffer\n"
        "    Py_TPFLAGS_DEFAULT,                 // tp_flags\n"
        "    0,                                  // tp_doc\n"
        "    0,                                  // tp_traverse\n"
        "    0,                                  // tp_clear\n"
        "    0,                                  // tp_richcompare\n"
        "    0,                                  // tp_weaklistoffset\n"
        "    0,                                  // tp_iter\n"
        "    0,                                  // tp_iternext\n"
        "    %s_pymethods,                       // tp_methods\n"
        "    0,                                  // tp_members\n"
        "    0,                                  // tp_getset\n"
        "    0,                                  // tp_base\n"
        "    0,                                  // tp_dict\n"
        "    0,                                  // tp_descr_get\n"
        "    0,                                  // tp_descr_set\n"
        "    0,                                  // tp_dictoffset\n"
        "    0,                                  // tp_init\n"
        "    0,                                  // tp_allow\n"
        "    %s,                                 // tp_new\n"
        "};\n"
        ;
    char *content = CFCUtil_sprintf(pattern, struct_sym, pymod_name,
                                    struct_sym, struct_sym, tp_new);
    return content;
}

