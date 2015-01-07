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
#include "Clownfish/VArray.h"
#include "Clownfish/Hash.h"
#include "Clownfish/HashIterator.h"
#include "Clownfish/ByteBuf.h"
#include "Clownfish/Num.h"
#include "Clownfish/LockFreeRegistry.h"

static bool Err_initialized;

/******************************** Utility **************************************/

void
CFBind_reraise_pyerr(cfish_Class *err_klass, cfish_String *mess) {
    PyObject *type, *value, *traceback;
    PyObject *type_pystr = NULL;
    PyObject *value_pystr = NULL;
    PyObject *traceback_pystr = NULL;
    char *type_str = "";
    char *value_str = "";
    char *traceback_str = "";
    PyErr_GetExcInfo(&type, &value, &traceback);
    if (type != NULL) {
        PyObject *type_pystr = PyObject_Str(type);
        type_str = PyUnicode_AsUTF8(type_pystr);
    }
    if (value != NULL) {
        PyObject *value_pystr = PyObject_Str(value);
        value_str = PyUnicode_AsUTF8(value_pystr);
    }
    if (traceback != NULL) {
        PyObject *traceback_pystr = PyObject_Str(traceback);
        traceback_str = PyUnicode_AsUTF8(traceback_pystr);
    }
    cfish_String *new_mess = cfish_Str_newf("%o... %s: %s %s", mess, type_str,
                                            value_str, traceback_str);
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    Py_XDECREF(type_pystr);
    Py_XDECREF(value_pystr);
    Py_XDECREF(traceback_pystr);
    CFISH_DECREF(mess);
    cfish_Err_throw_mess(err_klass, new_mess);
}

static PyObject*
S_cfish_array_to_python_list(cfish_VArray *varray) {
    uint32_t num_elems = CFISH_VA_Get_Size(varray);
    PyObject *list = PyList_New(num_elems);

    // Iterate over array elems.
    for (uint32_t i = 0; i < num_elems; i++) {
        cfish_Obj *val = CFISH_VA_Fetch(varray, i);
        PyObject *item = CFBind_cfish_to_py(val);
        PyList_SET_ITEM(list, i, item);
    }

    return list;
}

static PyObject*
S_cfish_hash_to_python_dict(cfish_Hash *hash) {
    PyObject *dict = PyDict_New();

    // Iterate over key-value pairs.
    cfish_HashIterator *iter = cfish_HashIter_new(hash);
    while (CFISH_HashIter_Next(iter)) {
        cfish_String *key = (cfish_String*)CFISH_HashIter_Get_Key(iter);
        if (!CFISH_Obj_Is_A((cfish_Obj*)key, CFISH_STRING)) {
            CFISH_THROW(CFISH_ERR, "Non-string key: %o",
                        CFISH_Obj_Get_Class_Name((cfish_Obj*)key));
        }
        size_t size = CFISH_Str_Get_Size(key);
        const char *ptr = CFISH_Str_Get_Ptr8(key);
        PyObject *py_key = PyUnicode_FromStringAndSize(ptr, size);
        PyObject *py_val = CFBind_cfish_to_py(CFISH_HashIter_Get_Value(iter));
        PyDict_SetItem(dict, py_key, py_val);
        Py_DECREF(py_key);
        Py_DECREF(py_val);
    }
    CFISH_DECREF(iter);

    return dict;
}

PyObject*
CFBind_cfish_to_py(cfish_Obj *obj) {
    if (obj == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_STRING)) {
        const char *ptr = CFISH_Str_Get_Ptr8((cfish_String*)obj);
        size_t size = CFISH_Str_Get_Size((cfish_String*)obj);
        return PyUnicode_FromStringAndSize(ptr, size);
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_BYTEBUF)) {
        char *buf = CFISH_BB_Get_Buf((cfish_ByteBuf*)obj);
        size_t size = CFISH_BB_Get_Size((cfish_ByteBuf*)obj);
        return PyBytes_FromStringAndSize(buf, size);
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_VARRAY)) {
        return S_cfish_array_to_python_list((cfish_VArray*)obj);
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_HASH)) {
        return S_cfish_hash_to_python_dict((cfish_Hash*)obj);
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_FLOATNUM)) {
        return PyFloat_FromDouble(CFISH_Obj_To_F64(obj));
    }
    else if (obj == (cfish_Obj*)CFISH_TRUE) {
        Py_INCREF(Py_True);
        return Py_True;
    }
    else if (obj == (cfish_Obj*)CFISH_FALSE) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    else if (CFISH_Obj_Is_A(obj, CFISH_INTNUM)) {
        int64_t num = CFISH_Obj_To_I64(obj);
        return PyLong_FromLongLong(num);
    }
    else {
        return (PyObject*)CFISH_Obj_To_Host(obj);
    }
}

static cfish_VArray*
S_py_list_to_varray(PyObject *list) {
    Py_ssize_t size = PyList_GET_SIZE(list);
    cfish_VArray *array = cfish_VA_new(size);
    for (Py_ssize_t i = 0; i < size; i++) {
        CFISH_VA_Store(array, i, CFBind_py_to_cfish(PyList_GET_ITEM(list, i)));
    }
    return array;
}

static cfish_Hash*
S_py_dict_to_hash(PyObject *dict) {
    Py_ssize_t pos = 0;
    PyObject *key, *value;
    cfish_Hash *hash = cfish_Hash_new(PyDict_Size(dict));
    while (PyDict_Next(dict, &pos, &key, &value)) {
        char *ptr;
        Py_ssize_t size;
        PyObject *stringified = key;
        if (!PyUnicode_CheckExact(key)) {
            stringified = PyObject_Str(key);
        }
        ptr = PyUnicode_AsUTF8AndSize(stringified, &size);
        if (!ptr) {
            cfish_String *mess
                = CFISH_MAKE_MESS("Failed to stringify as UTF-8");
            CFBind_reraise_pyerr(CFISH_ERR, mess);
        }
        CFISH_Hash_Store_Utf8(hash, ptr, size, CFBind_py_to_cfish(value));
        if (stringified != key) {
            Py_DECREF(stringified);
        }
    }
    return hash;
}

cfish_Obj*
CFBind_py_to_cfish(PyObject *py_obj) {
    if (!py_obj || py_obj == Py_None) {
        return NULL;
    }
    else if (py_obj == Py_True) {
        return (cfish_Obj*)CFISH_TRUE;
    }
    else if (py_obj == Py_False) {
        return (cfish_Obj*)CFISH_FALSE;
    }
    else if (PyUnicode_CheckExact(py_obj)) {
        // TODO: Allow Clownfish String to wrap buffer of Python str?
        Py_ssize_t size;
        char *ptr = PyUnicode_AsUTF8AndSize(py_obj, &size);
        // TODO: Can we guarantee that Python will always supply valid UTF-8?
        if (!ptr || !cfish_StrHelp_utf8_valid(ptr, size)) {
            CFISH_THROW(CFISH_ERR, "Failed to convert Python string to UTF8");
        }
        return (cfish_Obj*) cfish_Str_new_from_trusted_utf8(ptr, size);
    }
    else if (PyBytes_CheckExact(py_obj)) {
        char *ptr = PyBytes_AS_STRING(py_obj);
        Py_ssize_t size = PyBytes_GET_SIZE(py_obj);
        return (cfish_Obj*)cfish_BB_new_bytes(ptr, size);
    }
    else if (PyList_CheckExact(py_obj)) {
        return (cfish_Obj*)S_py_list_to_varray(py_obj);
    }
    else if (PyDict_CheckExact(py_obj)) {
        return (cfish_Obj*)S_py_dict_to_hash(py_obj);
    }
    else if (PyLong_CheckExact(py_obj)) {
        // Raises ValueError on overflow.
        return (cfish_Obj*)cfish_Int64_new(PyLong_AsLongLong(py_obj));
    }
    else if (PyFloat_CheckExact(py_obj)) {
        return (cfish_Obj*)cfish_Float64_new(PyFloat_AS_DOUBLE(py_obj));
    }
    else {
        // TODO: Wrap in some sort of cfish_Host object instead of
        // stringifying?
        PyObject *stringified = PyObject_Str(py_obj);
        if (stringified == NULL) {
            cfish_String *mess
                = CFISH_MAKE_MESS("Couldn't stringify object");
            CFBind_reraise_pyerr(CFISH_ERR, mess);
        }
        cfish_Obj *retval = CFBind_py_to_cfish(stringified);
        Py_DECREF(stringified);
        return retval;
    }
}

typedef struct ClassMapElem {
    cfish_Class **klass_handle;
    PyTypeObject *py_type;
} ClassMapElem;

typedef struct ClassMap {
    int32_t size;
    ClassMapElem *elems;
} ClassMap;

/* Before we can invoke methods on any Clownfish object safely, we must know
 * about its corresponding Python type object.  This association must be made
 * early in the bootstrapping process, which is tricky.  We can't use any
 * of Clownfish's convenient data structures yet!
 */
static ClassMap *klass_map = NULL;

static ClassMap*
S_revise_class_map(ClassMap *current, cfish_Class ***klass_handles,
                   PyTypeObject **py_types, int32_t num_items) {
    int32_t num_current = current ? current->size : 0;
    int32_t total = num_current + num_items;
    ClassMap *revised = (ClassMap*)malloc(sizeof(ClassMap));
    revised->elems = (ClassMapElem*)malloc(total * sizeof(ClassMapElem));
    if (current) {
        size_t size = num_current * sizeof(ClassMapElem);
        memcpy(revised->elems, current->elems, size);
    }
    for (int32_t i = 0; i < num_items; i++) {
        revised->elems[i + num_current].klass_handle = klass_handles[i];
        revised->elems[i + num_current].py_type      = py_types[i];
    }
    revised->size = total;
    return revised;
}

void
CFBind_assoc_py_types(cfish_Class ***klass_handles, PyTypeObject **py_types,
                      int32_t num_items) {
    while (1) {
        ClassMap *current = klass_map;
        ClassMap *revised = S_revise_class_map(current, klass_handles,
                                               py_types, num_items);
        if (cfish_Atomic_cas_ptr((void*volatile*)&klass_map, current, revised)) {
            if (current) {
                // TODO: Use read locking.  For now we have to leak this
                // memory to avoid memory errors in case another thread isn't
                // done reading it yet.

                //free(current->elems);
                //free(current);
            }
            break;
        }
        else {
            // Another thread beat us to it.  Try again.
            free(revised->elems);
            free(revised);
        }
    }
}

/******************************** Obj **************************************/

uint32_t
CFISH_Obj_Get_RefCount_IMP(cfish_Obj *self) {
    return Py_REFCNT(self);
}

cfish_Obj*
CFISH_Obj_Inc_RefCount_IMP(cfish_Obj *self) {
    Py_INCREF(self);
    return self;
}

uint32_t
CFISH_Obj_Dec_RefCount_IMP(cfish_Obj *self) {
    uint32_t modified_refcount = Py_REFCNT(self);
    Py_DECREF(self);
    return modified_refcount;
}

void*
CFISH_Obj_To_Host_IMP(cfish_Obj *self) {
    return self;
}

/******************************* Class *************************************/

static PyTypeObject*
S_get_cached_py_type(cfish_Class *self) {
    PyTypeObject *py_type = (PyTypeObject*)self->host_type;
    if (py_type == NULL) {
        ClassMap *current = klass_map;
        for (int32_t i = 0; i < current->size; i++) {
            cfish_Class **handle = current->elems[i].klass_handle;
            if (handle == NULL || *handle != self) {
                continue;
            }
            py_type = current->elems[i].py_type;
            Py_INCREF(py_type);
            if (!cfish_Atomic_cas_ptr((void*volatile*)&self->host_type, py_type, NULL)) {
                // Lost the race to another thread, so get rid of the refcount.
                Py_DECREF(py_type);
            }

            // FIXME HEINOUS HACK!  We don't know the size of the object until
            // after bootstrapping.
            py_type->tp_basicsize = self->obj_alloc_size;
            break;
        }
    }
    if (py_type == NULL) {
        if (Err_initialized) {
            CFISH_THROW(CFISH_ERR,
                        "Can't find a Python type object corresponding to %o",
                        CFISH_Class_Get_Name(self));
        }
        else {
            fprintf(stderr, "Can't find a Python type corresponding to a "
                            "Clownfish class\n");
            exit(1);
        }

    }
    return py_type;
}

cfish_Obj*
CFISH_Class_Make_Obj_IMP(cfish_Class *self) {
    PyTypeObject *py_type = S_get_cached_py_type(self);
    cfish_Obj *obj = (cfish_Obj*)py_type->tp_alloc(py_type, 0);
    obj->klass = self;
    return obj;
}

cfish_Obj*
CFISH_Class_Init_Obj_IMP(cfish_Class *self, void *allocation) {
    PyTypeObject *py_type = S_get_cached_py_type(self);
    // It would be nice if we could call PyObject_Init() here and feel
    // confident that we have performed all Python-specific initialization
    // under all possible configurations, but that's not possible.  In
    // addition to setting ob_refcnt and ob_type, PyObject_Init() performs
    // tracking for heap allocated objects under special builds -- but
    // Class_Init_Obj() may be called on non-heap memory, such as
    // stack-allocated Clownfish Strings.  Therefore, we must perform a subset
    // of tasks selected from PyObject_Init() manually.
    cfish_Obj *obj = (cfish_Obj*)allocation;
    obj->ob_base.ob_refcnt = 1;
    obj->ob_base.ob_type = py_type;
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

cfish_VArray*
cfish_Class_fresh_host_methods(cfish_String *class_name) {
    CFISH_UNUSED_VAR(class_name);
    return cfish_VA_new(0);
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
    Err_initialized = true;
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

/************************** LockFreeRegistry *******************************/

void*
CFISH_LFReg_To_Host_IMP(cfish_LockFreeRegistry *self) {
    CFISH_UNUSED_VAR(self);
    CFISH_THROW(CFISH_ERR, "TODO");
    CFISH_UNREACHABLE_RETURN(void*);
}


