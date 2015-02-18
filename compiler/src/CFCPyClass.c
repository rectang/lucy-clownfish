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

#define CFC_NEED_BASE_STRUCT_DEF 1

#include "CFCBase.h"
#include "CFCPyClass.h"
#include "CFCParcel.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCPyMethod.h"

struct CFCPyClass {
    CFCBase base;
    CFCParcel *parcel;
    char *class_name;
    CFCClass *client;
};

static CFCPyClass **registry = NULL;
static size_t registry_size = 0;
static size_t registry_cap  = 0;

static const CFCMeta CFCPERLCLASS_META = {
    "Clownfish::CFC::Binding::Perl::Class",
    sizeof(CFCPyClass),
    (CFCBase_destroy_t)CFCPyClass_destroy
};

CFCPyClass*
CFCPyClass_new(CFCParcel *parcel, const char *class_name) {
    CFCUTIL_NULL_CHECK(parcel);
    CFCUTIL_NULL_CHECK(class_name);
    CFCPyClass *self = (CFCPyClass*)CFCBase_allocate(&CFCPERLCLASS_META);
    self->parcel = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->class_name = CFCUtil_strdup(class_name);
    // Client may be NULL, since fetch_singleton() does not always succeed.
    CFCClass *client = CFCClass_fetch_singleton(parcel, class_name);
    self->client = (CFCClass*)CFCBase_incref((CFCBase*)client);
    return self;
}

void
CFCPyClass_destroy(CFCPyClass *self) {
    CFCBase_decref((CFCBase*)self->parcel);
    CFCBase_decref((CFCBase*)self->client);
    FREEMEM(self->class_name);
    CFCBase_destroy((CFCBase*)self);
}

static int
S_compare_cfcperlclass(const void *va, const void *vb) {
    CFCPyClass *a = *(CFCPyClass**)va;
    CFCPyClass *b = *(CFCPyClass**)vb;
    return strcmp(a->class_name, b->class_name);
}

void
CFCPyClass_add_to_registry(CFCPyClass *self) {
    if (registry_size == registry_cap) {
        size_t new_cap = registry_cap + 10;
        registry = (CFCPyClass**)REALLOCATE(registry,
                                            (new_cap + 1) * sizeof(CFCPyClass*));
        for (size_t i = registry_cap; i <= new_cap; i++) {
            registry[i] = NULL;
        }
        registry_cap = new_cap;
    }
    CFCPyClass *existing = CFCPyClass_singleton(self->class_name);
    if (existing) {
        CFCUtil_die("Class '%s' already registered", self->class_name);
    }
    registry[registry_size] = (CFCPyClass*)CFCBase_incref((CFCBase*)self);
    registry_size++;
    qsort(registry, registry_size, sizeof(CFCPyClass*),
          S_compare_cfcperlclass);
}

CFCPyClass*
CFCPyClass_singleton(const char *class_name) {
    CFCUTIL_NULL_CHECK(class_name);
    for (size_t i = 0; i < registry_size; i++) {
        CFCPyClass *existing = registry[i];
        if (strcmp(class_name, existing->class_name) == 0) {
            return existing;
        }
    }
    return NULL;
}

CFCPyClass**
CFCPyClass_registry() {
    if (!registry) {
        registry = (CFCPyClass**)CALLOCATE(1, sizeof(CFCPyClass*));
    }
    return registry;
}

void
CFCPyClass_clear_registry(void) {
    for (size_t i = 0; i < registry_size; i++) {
        CFCBase_decref((CFCBase*)registry[i]);
    }
    FREEMEM(registry);
    registry_size = 0;
    registry_cap  = 0;
    registry      = NULL;
}

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

