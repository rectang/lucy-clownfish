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
#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCGoClass.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCMethod.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCFunction.h"
#include "CFCSymbol.h"
#include "CFCVariable.h"
#include "CFCType.h"
#include "CFCGoFunc.h"
#include "CFCGoMethod.h"
//#include "CFCGoConstructor.h"
#include "CFCGoTypeMap.h"

struct CFCGoClass {
    CFCBase base;
    CFCParcel *parcel;
    char *class_name;
    CFCClass *client;
    char **cons_aliases;
    char **cons_inits;
    size_t num_cons;
    int    exclude_cons;
};

static CFCGoClass **registry = NULL;
static size_t registry_size = 0;
static size_t registry_cap  = 0;

static const CFCMeta CFCGOCLASS_META = {
    "Clownfish::CFC::Binding::Go::Class",
    sizeof(CFCGoClass),
    (CFCBase_destroy_t)CFCGoClass_destroy
};

CFCGoClass*
CFCGoClass_new(CFCParcel *parcel, const char *class_name) {
    CFCGoClass *self = (CFCGoClass*)CFCBase_allocate(&CFCGOCLASS_META);
    return CFCGoClass_init(self, parcel, class_name);
}

CFCGoClass*
CFCGoClass_init(CFCGoClass *self, CFCParcel *parcel,
                  const char *class_name) {
    CFCUTIL_NULL_CHECK(parcel);
    CFCUTIL_NULL_CHECK(class_name);
    self->parcel = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->class_name = CFCUtil_strdup(class_name);
    // Client may be NULL, since fetch_singleton() does not always succeed.
    CFCClass *client = CFCClass_fetch_singleton(parcel, class_name);
    self->client = (CFCClass*)CFCBase_incref((CFCBase*)client);
    self->cons_aliases      = NULL;
    self->cons_inits        = NULL;
    self->num_cons          = 0;
    self->exclude_cons      = 0;
    return self;
}

void
CFCGoClass_destroy(CFCGoClass *self) {
    CFCBase_decref((CFCBase*)self->parcel);
    CFCBase_decref((CFCBase*)self->client);
    FREEMEM(self->class_name);
    for (size_t i = 0; i < self->num_cons; i++) {
        FREEMEM(self->cons_aliases[i]);
        FREEMEM(self->cons_inits[i]);
    }
    FREEMEM(self->cons_aliases);
    FREEMEM(self->cons_inits);
    CFCBase_destroy((CFCBase*)self);
}

static int
S_compare_cfcgoclass(const void *va, const void *vb) {
    CFCGoClass *a = *(CFCGoClass**)va;
    CFCGoClass *b = *(CFCGoClass**)vb;
    return strcmp(a->class_name, b->class_name);
}

void
CFCGoClass_register(CFCGoClass *self) {
    if (registry_size == registry_cap) {
        size_t new_cap = registry_cap + 10;
        registry = (CFCGoClass**)REALLOCATE(registry,
                                              (new_cap + 1) * sizeof(CFCGoClass*));
        for (size_t i = registry_cap; i <= new_cap; i++) {
            registry[i] = NULL;
        }
        registry_cap = new_cap;
    }
    CFCGoClass *existing = CFCGoClass_singleton(self->class_name);
    if (existing) {
        CFCUtil_die("Class '%s' already registered", self->class_name);
    }
    registry[registry_size] = (CFCGoClass*)CFCBase_incref((CFCBase*)self);
    registry_size++;
    qsort(registry, registry_size, sizeof(CFCGoClass*),
          S_compare_cfcgoclass);
}

CFCGoClass*
CFCGoClass_singleton(const char *class_name) {
    CFCUTIL_NULL_CHECK(class_name);
    for (size_t i = 0; i < registry_size; i++) {
        CFCGoClass *existing = registry[i];
        if (strcmp(class_name, existing->class_name) == 0) {
            return existing;
        }
    }
    return NULL;
}

CFCGoClass**
CFCGoClass_registry() {
    if (!registry) {
        registry = (CFCGoClass**)CALLOCATE(1, sizeof(CFCGoClass*));
    }
    return registry;
}

void
CFCGoClass_clear_registry(void) {
    for (size_t i = 0; i < registry_size; i++) {
        CFCBase_decref((CFCBase*)registry[i]);
    }
    FREEMEM(registry);
    registry_size = 0;
    registry_cap  = 0;
    registry      = NULL;
}
#if 0

void
CFCGoClass_exclude_method(CFCGoClass *self, const char *meth_name) {
    if (!self->client) {
        CFCUtil_die("Can't exclude_method %s -- can't find client for %s",
                    meth_name, self->class_name);
    }
    CFCMethod *method = CFCClass_method(self->client, meth_name);
    if (!method) {
        CFCUtil_die("Can't exclude_method %s -- method not found in %s",
                    meth_name, self->class_name);
    }
    if (strcmp(CFCMethod_get_class_name(method), self->class_name) != 0) {
        CFCUtil_die("Can't exclude_method %s -- method not fresh in %s",
                    meth_name, self->class_name);
    }
    CFCMethod_exclude_from_host(method);
}

void
CFCGoClass_bind_constructor(CFCGoClass *self, const char *alias,
                              const char *initializer) {
    alias       = alias       ? alias       : "new";
    initializer = initializer ? initializer : "init";
    size_t size = (self->num_cons + 1) * sizeof(char*);
    self->cons_aliases = (char**)REALLOCATE(self->cons_aliases, size);
    self->cons_inits   = (char**)REALLOCATE(self->cons_inits,   size);
    self->cons_aliases[self->num_cons] = (char*)CFCUtil_strdup(alias);
    self->cons_inits[self->num_cons]   = (char*)CFCUtil_strdup(initializer);
    self->num_cons++;
    if (!self->client) {
        CFCUtil_die("Can't bind_constructor %s -- can't find client for %s",
                    alias, self->class_name);
    }
}

void
CFCGoClass_exclude_constructor(CFCGoClass *self) {
    self->exclude_cons = 1;
}

#endif
CFCGoMethod**
CFCGoClass_method_bindings(CFCClass *klass) {
    CFCClass     *parent        = CFCClass_get_parent(klass);
    size_t        num_bound     = 0;
    CFCMethod   **fresh_methods = CFCClass_fresh_methods(klass);
    CFCGoMethod **bound 
        = (CFCGoMethod**)CALLOCATE(1, sizeof(CFCGoMethod*));

     // Iterate over the class's fresh methods.
    for (size_t i = 0; fresh_methods[i] != NULL; i++) {
        CFCMethod *method = fresh_methods[i];

        // Skip methods which have been explicitly excluded.
        if (CFCMethod_excluded_from_host(method)) {
            continue;
        }

        // Skip methods that shouldn't be bound.
        if (!CFCGoMethod_can_be_bound(method)) {
            continue;
        }

        /* Create the binding, add it to the array.
         */
        CFCGoMethod *meth_binding = CFCGoMethod_new(method);
        size_t size = (num_bound + 2) * sizeof(CFCGoMethod*);
        bound = (CFCGoMethod**)REALLOCATE(bound, size);
        bound[num_bound] = meth_binding;
        num_bound++;
        bound[num_bound] = NULL;
    }

    FREEMEM(fresh_methods);

    return bound;
}
#if 0

static const char NEW[] = "new";

CFCPerlConstructor**
CFCGoClass_constructor_bindings(CFCClass *klass) {
    const char    *class_name = CFCClass_get_class_name(klass);
    CFCGoClass  *perl_class = CFCGoClass_singleton(class_name);
    CFCFunction  **functions  = CFCClass_functions(klass);
    size_t         num_bound  = 0;
    CFCPerlConstructor **bound 
        = (CFCPerlConstructor**)CALLOCATE(1, sizeof(CFCPerlConstructor*));

    // Iterate over the list of possible initialization functions.
    for (size_t i = 0; functions[i] != NULL; i++) {
        CFCFunction  *function  = functions[i];
        const char   *micro_sym = CFCFunction_micro_sym(function);
        const char   *alias     = NULL;

        // Find user-specified alias.
        if (perl_class == NULL) {
            // Bind init() to new() when possible.
            if (strcmp(micro_sym, "init") == 0
                && CFCPerlSub_can_be_bound(function)
               ) {
                alias = NEW;
            }
        }
        else {
            for (size_t j = 0; j < perl_class->num_cons; j++) {
                if (strcmp(micro_sym, perl_class->cons_inits[j]) == 0) {
                    alias = perl_class->cons_aliases[j];
                    if (!CFCPerlSub_can_be_bound(function)) {
                        CFCUtil_die("Can't bind %s as %s"
                                    " -- types can't be mapped",
                                    micro_sym, alias);
                    }
                    break;
                }
            }

            // Automatically bind init() to new() when possible.
            if (!alias
                && !perl_class->exclude_cons
                && strcmp(micro_sym, "init") == 0
                && CFCPerlSub_can_be_bound(function)
               ) {
                int saw_new = 0;
                for (size_t j = 0; j < perl_class->num_cons; j++) {
                    if (strcmp(perl_class->cons_aliases[j], "new") == 0) {
                        saw_new = 1;
                    }
                }
                if (!saw_new) {
                    alias = NEW;
                }
            }
        }

        if (!alias) {
            continue;
        }

        // Create the binding, add it to the array.
        CFCPerlConstructor *cons_binding
            = CFCPerlConstructor_new(klass, alias, micro_sym);
        size_t size = (num_bound + 2) * sizeof(CFCPerlConstructor*);
        bound = (CFCPerlConstructor**)REALLOCATE(bound, size);
        bound[num_bound] = cons_binding;
        num_bound++;
        bound[num_bound] = NULL;
    }

    return bound;
}

CFCClass*
CFCGoClass_get_client(CFCGoClass *self) {
    return self->client;
}

const char*
CFCGoClass_get_class_name(CFCGoClass *self) {
    return self->class_name;
}

// Generate C code which initializes method metadata.
char*
CFCGoClass_method_metadata_code(CFCGoClass *self) {
    const char *class_var = CFCClass_full_class_var(self->client);
    CFCMethod **fresh_methods = CFCClass_fresh_methods(self->client);
    char *code = CFCUtil_strdup("");

    for (int i = 0; fresh_methods[i] != NULL; i++) {
        CFCMethod *method = fresh_methods[i];
        if (!CFCMethod_novel(method)) { continue; }

        const char *macro_sym = CFCMethod_get_macro_sym(method);
        const char *alias     = CFCMethod_get_host_alias(method);
        if (alias) {
            code = CFCUtil_cat(code, "    CFISH_Class_Add_Host_Method_Alias(",
                               class_var, ", \"", alias, "\", \"", macro_sym,
                               "\");\n", NULL);
        }
        if (CFCMethod_excluded_from_host(method)) {
            code = CFCUtil_cat(code, "    CFISH_Class_Exclude_Host_Method(",
                               class_var, ", \"", macro_sym, "\");\n", NULL);
        }
    }

    FREEMEM(fresh_methods);
    return code;
}


#endif
