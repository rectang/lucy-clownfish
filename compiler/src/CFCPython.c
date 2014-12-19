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

#include "charmony.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCPython.h"
#include "CFCParcel.h"
#include "CFCClass.h"
#include "CFCMethod.h"
#include "CFCHierarchy.h"
#include "CFCUtil.h"
//#include "CFCPyClass.h"
//#include "CFCPyFunc.h"
//#include "CFCPyMethod.h"
//#include "CFCPyConstructor.h"
//#include "CFCPyTypeMap.h"
#include "CFCBindCore.h"

struct CFCPython {
    CFCBase base;
    CFCHierarchy *hierarchy;
    char *header;
    char *footer;
};

static const CFCMeta CFCPYTHON_META = {
    "Clownfish::CFC::Binding::Python",
    sizeof(CFCPython),
    (CFCBase_destroy_t)CFCPython_destroy
};

CFCPython*
CFCPython_new(CFCHierarchy *hierarchy) {
    CFCPython *self = (CFCPython*)CFCBase_allocate(&CFCPYTHON_META);
    return CFCPython_init(self, hierarchy);
}

CFCPython*
CFCPython_init(CFCPython *self, CFCHierarchy *hierarchy) {
    CFCUTIL_NULL_CHECK(hierarchy);
    self->hierarchy  = (CFCHierarchy*)CFCBase_incref((CFCBase*)hierarchy);
    self->header     = CFCUtil_strdup("");
    self->footer     = CFCUtil_strdup("");
    return self;
}

void
CFCPython_destroy(CFCPython *self) {
    CFCBase_decref((CFCBase*)self->hierarchy);
    FREEMEM(self->header);
    FREEMEM(self->footer);
    CFCBase_destroy((CFCBase*)self);
}

void
CFCPython_set_header(CFCPython *self, const char *header) {
    CFCUTIL_NULL_CHECK(header);
    free(self->header);
    self->header = CFCUtil_strdup(header);
}

void
CFCPython_set_footer(CFCPython *self, const char *footer) {
    CFCUTIL_NULL_CHECK(footer);
    free(self->footer);
    self->footer = CFCUtil_strdup(footer);
}

static void
S_write_hostdefs(CFCPython *self) {
    const char pattern[] =
        "%s\n"
        "\n"
        "#ifndef H_CFISH_HOSTDEFS\n"
        "#define H_CFISH_HOSTDEFS 1\n"
        "\n"
        "#include \"Python.h\"\n"
        "\n"
        "#define CFISH_OBJ_HEAD \\\n"
        "    PyObject_HEAD\n"
        "\n"
        "#endif /* H_CFISH_HOSTDEFS */\n"
        "\n"
        "%s\n";
    char *content
        = CFCUtil_sprintf(pattern, self->header, self->footer);

    // Write if the content has changed.
    const char *inc_dest = CFCHierarchy_get_include_dest(self->hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "cfish_hostdefs.h",
                                     inc_dest);
    CFCUtil_write_if_changed(filepath, content, strlen(content));

    FREEMEM(filepath);
    FREEMEM(content);
}

static void
S_write_callbacks(CFCPython *self) {
    // Generate header files declaring callbacks.
    CFCBindCore *core_binding
        = CFCBindCore_new(self->hierarchy, self->header, self->footer);
    CFCBindCore_write_callbacks_h(core_binding);
    CFCBase_decref((CFCBase*)core_binding);

    const char *source_dest = CFCHierarchy_get_source_dest(self->hierarchy);
    char *cb_c_filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "callbacks.c",
                                     source_dest);
    CFCUtil_write_if_changed(cb_c_filepath, "", 0);
    FREEMEM(cb_c_filepath);
}

void
CFCPython_write_bindings(CFCPython *self, const char *parcel_name, const char *dest) {
    S_write_hostdefs(self);
    S_write_callbacks(self);
}


