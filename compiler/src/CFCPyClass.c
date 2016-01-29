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
#include <stdlib.h>

#define CFC_NEED_BASE_STRUCT_DEF 1

#include "CFCBase.h"
#include "CFCPyClass.h"
#include "CFCPyMethod.h"
#include "CFCParcel.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCMethod.h"

struct CFCPyClass {
    CFCBase base;
    CFCParcel *parcel;
    char *class_name;
    CFCClass *client;
    char *pre_code;
    char *meth_defs;
};

static CFCPyClass **registry = NULL;
static size_t registry_size = 0;
static size_t registry_cap  = 0;

static void
S_CFCPyClass_destroy(CFCPyClass *self);

static const CFCMeta CFCPERLCLASS_META = {
    "Clownfish::CFC::Binding::Python::Class",
    sizeof(CFCPyClass),
    (CFCBase_destroy_t)S_CFCPyClass_destroy
};

CFCPyClass*
CFCPyClass_new(CFCClass *client) {
    CFCUTIL_NULL_CHECK(client);
    CFCPyClass *self = (CFCPyClass*)CFCBase_allocate(&CFCPERLCLASS_META);
    CFCParcel *parcel = CFCClass_get_parcel(client);
    self->parcel = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->class_name = CFCUtil_strdup(CFCClass_get_name(client));
    self->client = (CFCClass*)CFCBase_incref((CFCBase*)client);
    self->pre_code = NULL;
    self->meth_defs = CFCUtil_strdup("");
    return self;
}

static void
S_CFCPyClass_destroy(CFCPyClass *self) {
    CFCBase_decref((CFCBase*)self->parcel);
    CFCBase_decref((CFCBase*)self->client);
    FREEMEM(self->class_name);
    FREEMEM(self->pre_code);
    FREEMEM(self->meth_defs);
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
CFCPyClass_gen_binding_code(CFCPyClass *self) {
    return CFCUtil_strdup("");
}
