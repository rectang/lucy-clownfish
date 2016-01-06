# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import cfc

def spec_classes():
    spec_obj()

def spec_obj():
    klass = cfc.model.Class.fetch_singleton(parcel='Clownfish',
                                            class_name='Clownfish::Obj')
    obj_binding = cfc.binding.python.Class(client=klass)
    #obj_binding.set_pre_code(obj_custom_code)
    #meth_def = '{"is_a", (PyCFunction)S_cfish_Obj_is_a, METH_O, NULL}'
   # obj_binding.exclude_method('Is_A')
    #obj_binding.add_py_method_def(meth_def)
    obj_binding.register()

obj_custom_code = """
static PyObject*
S_cfish_Obj_is_a(PyObject *py_self, PyObject *class_name) {
    Py_ssize_t size;
    char *utf8 = PyUnicode_AsUTF8AndSize(class_name, &size);
    if (!utf8) {
        return NULL;
    }
    cfish_String *name = CFISH_SSTR_WRAP_UTF8(utf8, size);
    cfish_Class *klass = cfish_Class_singleton(name, NULL);
    bool result = cfish_Obj_is_a((cfish_Obj*)py_self, klass);
    return PyBool_FromLong(result);
}
"""

