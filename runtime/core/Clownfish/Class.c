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

#define C_CFISH_CLASS
#define C_CFISH_OBJ
#define C_CFISH_STRING
#define C_CFISH_METHOD
#define CFISH_USE_SHORT_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "Clownfish/Class.h"
#include "Clownfish/String.h"
#include "Clownfish/CharBuf.h"
#include "Clownfish/Err.h"
#include "Clownfish/Hash.h"
#include "Clownfish/LockFreeRegistry.h"
#include "Clownfish/Method.h"
#include "Clownfish/Num.h"
#include "Clownfish/VArray.h"
#include "Clownfish/Util/Atomic.h"
#include "Clownfish/Util/Memory.h"

#define NovelMethSpec            cfish_NovelMethSpec
#define OverriddenMethSpec       cfish_OverriddenMethSpec
#define InheritedMethSpec        cfish_InheritedMethSpec
#define ClassSpec                cfish_ClassSpec

size_t Class_offset_of_parent = offsetof(Class, parent);

static Method*
S_find_method(Class *self, const char *meth_name);

static int32_t
S_claim_parcel_id(void);

LockFreeRegistry *Class_registry = NULL;
cfish_Class_bootstrap_hook1_t cfish_Class_bootstrap_hook1 = NULL;

void
Class_bootstrap(const ClassSpec *specs, size_t num_specs)
{
    int32_t parcel_id = S_claim_parcel_id();

    /* Pass 1:
     * - Initialize IVARS_OFFSET.
     * - Allocate memory.
     * - Initialize parent, flags, obj_alloc_size, class_alloc_size.
     * - Assign parcel_id.
     * - Initialize method pointers.
     */
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *parent = spec->parent ? *spec->parent : NULL;

        size_t ivars_offset = 0;
        if (spec->ivars_offset_ptr != NULL) {
            if (parent) {
                Class *ancestor = parent;
                while (ancestor && ancestor->parcel_id == parcel_id) {
                    ancestor = ancestor->parent;
                }
                ivars_offset = ancestor ? ancestor->obj_alloc_size : 0;
                *spec->ivars_offset_ptr = ivars_offset;
            }
            else {
                *spec->ivars_offset_ptr = 0;
            }
        }

        size_t novel_offset = parent
                              ? parent->class_alloc_size
                              : offsetof(Class, vtable);
        size_t class_alloc_size = novel_offset
                                  + spec->num_novel_meths
                                    * sizeof(cfish_method_t);
        Class *klass = (Class*)Memory_wrapped_calloc(class_alloc_size, 1);

        klass->parent           = parent;
        klass->parcel_id        = parcel_id;
        klass->flags            = 0;
        klass->obj_alloc_size   = ivars_offset + spec->ivars_size;
        klass->class_alloc_size = class_alloc_size;

        if (parent) {
            // Copy parent vtable.
            size_t parent_vt_size = parent->class_alloc_size
                                    - offsetof(Class, vtable);
            memcpy(klass->vtable, parent->vtable, parent_vt_size);
        }

        for (size_t i = 0; i < spec->num_inherited_meths; ++i) {
            const InheritedMethSpec *mspec = &spec->inherited_meth_specs[i];
            *mspec->offset = *mspec->parent_offset;
        }

        for (size_t i = 0; i < spec->num_overridden_meths; ++i) {
            const OverriddenMethSpec *mspec = &spec->overridden_meth_specs[i];
            *mspec->offset = *mspec->parent_offset;
            Class_Override_IMP(klass, mspec->func, *mspec->offset);
        }

        for (size_t i = 0; i < spec->num_novel_meths; ++i) {
            const NovelMethSpec *mspec = &spec->novel_meth_specs[i];
            *mspec->offset = novel_offset;
            novel_offset += sizeof(cfish_method_t);
            Class_Override_IMP(klass, mspec->func, *mspec->offset);
        }

        *spec->klass = klass;
    }

    /* Pass 2:
     * - Initialize 'klass' instance variable.
     * - Initialize refcount.
     */
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *klass = *spec->klass;

        Class_Init_Obj_IMP(CLASS, klass);
        if (cfish_Class_bootstrap_hook1 != NULL) {
            cfish_Class_bootstrap_hook1(klass);
        }
    }

    /* Now it's safe to call methods.
     *
     * Pass 3:
     * - Inititalize name and method array.
     * - Register class.
     */
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *klass = *spec->klass;

        klass->name    = Str_newf("%s", spec->name);
        klass->methods = (Method**)MALLOCATE((spec->num_novel_meths + 1)
                                             * sizeof(Method*));

        // Only store novel methods for now.
        for (size_t i = 0; i < spec->num_novel_meths; ++i) {
            const NovelMethSpec *mspec = &spec->novel_meth_specs[i];
            String *name = Str_newf("%s", mspec->name);
            Method *method = Method_new(name, mspec->callback_func,
                                        *mspec->offset);
            klass->methods[i] = method;
            DECREF(name);
        }

        klass->methods[spec->num_novel_meths] = NULL;

        Class_add_to_registry(klass);
    }
}

void
Class_Destroy_IMP(Class *self) {
    THROW(ERR, "Insane attempt to destroy Class for class '%o'", self->name);
}

Class*
Class_Clone_IMP(Class *self) {
    Class *twin
        = (Class*)Memory_wrapped_calloc(self->class_alloc_size, 1);

    memcpy(twin, self, self->class_alloc_size);
    Class_Init_Obj(self->klass, twin); // Set refcount.
    twin->name = Str_Clone(self->name);

    return twin;
}

Obj*
Class_Inc_RefCount_IMP(Class *self) {
    return (Obj*)self;
}

uint32_t
Class_Dec_RefCount_IMP(Class *self) {
    UNUSED_VAR(self);
    return 1;
}

uint32_t
Class_Get_RefCount_IMP(Class *self) {
    UNUSED_VAR(self);
    /* Class_Get_RefCount() lies to other Clownfish code about the refcount
     * because we don't want to have to synchronize access to the cached host
     * object to which we have delegated responsibility for keeping refcounts.
     * It always returns 1 because 1 is a positive number, and thus other
     * Clownfish code will be fooled into believing it never needs to take
     * action such as initiating a destructor.
     *
     * It's possible that the host has in fact increased the refcount of the
     * cached host object if there are multiple refs to it on the other side
     * of the Clownfish/host border, but returning 1 is good enough to fool
     * Clownfish code.
     */
    return 1;
}

void
Class_Override_IMP(Class *self, cfish_method_t method, size_t offset) {
    union { char *char_ptr; cfish_method_t *func_ptr; } pointer;
    pointer.char_ptr = ((char*)self) + offset;
    pointer.func_ptr[0] = method;
}

String*
Class_Get_Name_IMP(Class *self) {
    return self->name;
}

Class*
Class_Get_Parent_IMP(Class *self) {
    return self->parent;
}

size_t
Class_Get_Obj_Alloc_Size_IMP(Class *self) {
    return self->obj_alloc_size;
}

VArray*
Class_Get_Methods_IMP(Class *self) {
    VArray *retval = VA_new(0);

    for (size_t i = 0; self->methods[i]; ++i) {
        VA_Push(retval, INCREF(self->methods[i]));
    }

    return retval;
}

void
Class_init_registry() {
    LockFreeRegistry *reg = LFReg_new(256);
    if (Atomic_cas_ptr((void*volatile*)&Class_registry, NULL, reg)) {
        return;
    }
    else {
        DECREF(reg);
    }
}

Class*
Class_singleton(String *class_name, Class *parent) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }

    Class *singleton = (Class*)LFReg_Fetch(Class_registry, (Obj*)class_name);
    if (singleton == NULL) {
        VArray *fresh_host_methods;
        uint32_t num_fresh;

        if (parent == NULL) {
            String *parent_class = Class_find_parent_class(class_name);
            if (parent_class == NULL) {
                THROW(ERR, "Class '%o' doesn't descend from %o", class_name,
                      OBJ->name);
            }
            else {
                parent = Class_singleton(parent_class, NULL);
                DECREF(parent_class);
            }
        }

        // Copy source class.
        singleton = Class_Clone(parent);

        // Turn clone into child.
        singleton->parent = parent;
        DECREF(singleton->name);
        singleton->name = Str_Clone(class_name);
        singleton->methods = (Method**)CALLOCATE(1, sizeof(Method*));

        // Allow host methods to override.
        fresh_host_methods = Class_fresh_host_methods(class_name);
        num_fresh = VA_Get_Size(fresh_host_methods);
        if (num_fresh) {
            Hash *meths = Hash_new(num_fresh);
            for (uint32_t i = 0; i < num_fresh; i++) {
                String *meth = (String*)VA_Fetch(fresh_host_methods, i);
                Hash_Store(meths, (Obj*)meth, (Obj*)CFISH_TRUE);
            }
            for (Class *klass = parent; klass; klass = klass->parent) {
                for (size_t i = 0; klass->methods[i]; i++) {
                    Method *method = klass->methods[i];
                    if (method->callback_func) {
                        String *name = Method_Host_Name(method);
                        if (Hash_Fetch(meths, (Obj*)name)) {
                            Class_Override(singleton, method->callback_func,
                                            method->offset);
                        }
                        DECREF(name);
                    }
                }
            }
            DECREF(meths);
        }
        DECREF(fresh_host_methods);

        // Register the new class, both locally and with host.
        if (Class_add_to_registry(singleton)) {
            // Doing this after registering is racy, but hard to fix. :(
            Class_register_with_host(singleton, parent);
        }
        else {
            DECREF(singleton);
            singleton = (Class*)LFReg_Fetch(Class_registry, (Obj*)class_name);
            if (!singleton) {
                THROW(ERR, "Failed to either insert or fetch Class for '%o'",
                      class_name);
            }
        }
    }

    return singleton;
}

bool
Class_add_to_registry(Class *klass) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }
    if (LFReg_Fetch(Class_registry, (Obj*)klass->name)) {
        return false;
    }
    else {
        String *class_name = Str_Clone(klass->name);
        bool retval
            = LFReg_Register(Class_registry, (Obj*)class_name, (Obj*)klass);
        DECREF(class_name);
        return retval;
    }
}

bool
Class_add_alias_to_registry(Class *klass, const char *alias_ptr,
                             size_t alias_len) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }
    StackString *alias = SSTR_WRAP_UTF8(alias_ptr, alias_len);
    if (LFReg_Fetch(Class_registry, (Obj*)alias)) {
        return false;
    }
    else {
        String *class_name = SStr_Clone(alias);
        bool retval
            = LFReg_Register(Class_registry, (Obj*)class_name, (Obj*)klass);
        DECREF(class_name);
        return retval;
    }
}

Class*
Class_fetch_class(String *class_name) {
    Class *klass = NULL;
    if (Class_registry != NULL) {
        klass = (Class*)LFReg_Fetch(Class_registry, (Obj*)class_name);
    }
    return klass;
}

void
Class_Add_Host_Method_Alias_IMP(Class *self, const char *alias,
                             const char *meth_name) {
    Method *method = S_find_method(self, meth_name);
    if (!method) {
        fprintf(stderr, "Method %s not found\n", meth_name);
        abort();
    }
    method->host_alias = Str_newf("%s", alias);
}

void
Class_Exclude_Host_Method_IMP(Class *self, const char *meth_name) {
    Method *method = S_find_method(self, meth_name);
    if (!method) {
        fprintf(stderr, "Method %s not found\n", meth_name);
        abort();
    }
    method->is_excluded = true;
}

static Method*
S_find_method(Class *self, const char *name) {
    size_t name_len = strlen(name);

    for (size_t i = 0; self->methods[i]; i++) {
        Method *method = self->methods[i];
        if (Str_Equals_Utf8(method->name, name, name_len)) {
            return method;
        }
    }

    return NULL;
}

static size_t parcel_count;

static int32_t
S_claim_parcel_id(void) {
    // TODO: use ordinary cas rather than cas_ptr.
    union { size_t num; void *ptr; } old_value;
    union { size_t num; void *ptr; } new_value;

    bool succeeded = false;
    do {
        old_value.num = parcel_count;
        new_value.num = old_value.num + 1;
        succeeded = Atomic_cas_ptr((void*volatile*)&parcel_count,
                                   old_value.ptr, new_value.ptr);
    } while (!succeeded);

    return new_value.num;
}

