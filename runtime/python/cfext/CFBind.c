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

#define C_CFISH_OBJ
#define C_CFISH_CLASS
#define C_CFISH_METHOD
#define C_CFISH_ERR
#define C_CFISH_LOCKFREEREGISTRY

#include "Python.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include "CFBind.h"

#include "Clownfish/Obj.h"
#include "Clownfish/Class.h"
#include "Clownfish/Method.h"
#include "Clownfish/Err.h"
#include "Clownfish/Util/Atomic.h"
#include "Clownfish/Util/Memory.h"
#include "Clownfish/Util/StringHelper.h"
#include "Clownfish/String.h"
#include "Clownfish/Vector.h"
#include "Clownfish/Hash.h"
#include "Clownfish/HashIterator.h"
#include "Clownfish/ByteBuf.h"
#include "Clownfish/Blob.h"
#include "Clownfish/Boolean.h"
#include "Clownfish/Num.h"
#include "Clownfish/Num.h"
#include "Clownfish/LockFreeRegistry.h"

/******************************** Obj **************************************/

uint32_t
cfish_get_refcount(void *vself) {
    cfish_Obj *self = (cfish_Obj*)vself;
    return Py_REFCNT(self);
}

cfish_Obj*
cfish_inc_refcount(void *vself) {
    cfish_Obj *self = (cfish_Obj*)vself;
    Py_INCREF(self);
    return self;
}

uint32_t
cfish_dec_refcount(void *vself) {
    cfish_Obj *self = (cfish_Obj*)vself;
    uint32_t modified_refcount = Py_REFCNT(self);
    Py_DECREF(self);
    return modified_refcount;
}

void*
CFISH_Obj_To_Host_IMP(cfish_Obj *self) {
    return self;
}



/******************************* Class *************************************/

cfish_Obj*
CFISH_Class_Make_Obj_IMP(cfish_Class *self) {
    return NULL;
}

cfish_Obj*
CFISH_Class_Init_Obj_IMP(cfish_Class *self, void *allocation) {
    cfish_Obj *obj = (cfish_Obj*)allocation;
    obj->klass = self;
    return obj;
}

/* Foster_Obj() is only needed under hosts where host object allocation and
 * Clownfish object allocation are separate.  When Clownfish is used under
 * Python, Clownfish objects *are* Python objects. */
cfish_Obj*
CFISH_Class_Foster_Obj_IMP(cfish_Class *self, void *host_obj) {
    CFISH_UNUSED_VAR(self);
    CFISH_UNUSED_VAR(host_obj);
    CFISH_THROW(CFISH_ERR, "Unimplemented");
    CFISH_UNREACHABLE_RETURN(cfish_Obj*);
}

void
cfish_Class_register_with_host(cfish_Class *singleton, cfish_Class *parent) {
    CFISH_UNUSED_VAR(singleton);
    CFISH_UNUSED_VAR(parent);
}

cfish_Vector*
cfish_Class_fresh_host_methods(cfish_String *class_name) {
    CFISH_UNUSED_VAR(class_name);
    return cfish_Vec_new(0);
}

cfish_String*
cfish_Class_find_parent_class(cfish_String *class_name) {
    CFISH_UNUSED_VAR(class_name);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(cfish_String*);
}

void*
CFISH_Class_To_Host_IMP(cfish_Class *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

/******************************* Method ************************************/

cfish_String*
CFISH_Method_Host_Name_IMP(cfish_Method *self) {
    return (cfish_String*)CFISH_INCREF(self->name);
}

/******************************** Err **************************************/

/* TODO: Thread safety */
static cfish_Err *current_error;
static cfish_Err *thrown_error;
static jmp_buf  *current_env;

void
cfish_Err_init_class(void) {
}

cfish_Err*
cfish_Err_get_error() {
    return current_error;
}

void
cfish_Err_set_error(cfish_Err *error) {
    if (current_error) {
        CFISH_DECREF(current_error);
    }
    current_error = error;
}

void
cfish_Err_do_throw(cfish_Err *error) {
    if (current_env) {
        thrown_error = error;
        longjmp(*current_env, 1);
    }
    else {
        cfish_String *message = CFISH_Err_Get_Mess(error);
        char *utf8 = CFISH_Str_To_Utf8(message);
        fprintf(stderr, "%s", utf8);
        CFISH_FREEMEM(utf8);
        exit(EXIT_FAILURE);
    }
}

void*
CFISH_Err_To_Host_IMP(cfish_Err *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void
cfish_Err_throw_mess(cfish_Class *klass, cfish_String *message) {
    CFISH_UNUSED_VAR(klass);
    cfish_Err *err = cfish_Err_new(message);
    cfish_Err_do_throw(err);
}

void
cfish_Err_warn_mess(cfish_String *message) {
    char *utf8 = CFISH_Str_To_Utf8(message);
    fprintf(stderr, "%s", utf8);
    CFISH_FREEMEM(utf8);
    CFISH_DECREF(message);
}

cfish_Err*
cfish_Err_trap(CFISH_Err_Attempt_t routine, void *context) {
    jmp_buf  env;
    jmp_buf *prev_env = current_env;
    current_env = &env;

    if (!setjmp(env)) {
        routine(context);
    }   

    current_env = prev_env;

    cfish_Err *error = thrown_error;
    thrown_error = NULL;
    return error;
}

/******************************* to host ***********************************/

void*
CFISH_Str_To_Host_IMP(cfish_String *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_BB_To_Host_IMP(cfish_ByteBuf *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Blob_To_Host_IMP(cfish_Blob *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Vec_To_Host_IMP(cfish_Vector *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Hash_To_Host_IMP(cfish_Hash *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Float_To_Host_IMP(cfish_Float *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Int_To_Host_IMP(cfish_Integer *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

void*
CFISH_Bool_To_Host_IMP(cfish_Boolean *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

/************************** LockFreeRegistry *******************************/

void*
CFISH_LFReg_To_Host_IMP(cfish_LockFreeRegistry *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}

/**************************** TestUtils **********************************/

void*
cfish_TestUtils_clone_host_runtime() {
    return NULL;
}

void
cfish_TestUtils_set_host_runtime(void *runtime) {
    CFISH_UNUSED_VAR(runtime);
}

void
cfish_TestUtils_destroy_host_runtime(void *runtime) {
    CFISH_UNUSED_VAR(runtime);
}
