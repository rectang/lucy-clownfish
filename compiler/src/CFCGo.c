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
#include "CFCGo.h"
#include "CFCParcel.h"
#include "CFCClass.h"
#include "CFCMethod.h"
#include "CFCHierarchy.h"
#include "CFCUtil.h"
#include "CFCGoClass.h"
#include "CFCGoFunc.h"
#include "CFCGoMethod.h"
// #include "CFCGoConstructor.h"
#include "CFCGoTypeMap.h"
#include "CFCBindCore.h"

static void
S_write_callbacks(CFCGo *self, CFCParcel *parcel, const char *h_includes);

struct CFCGo {
    CFCBase base;
    CFCHierarchy *hierarchy;
    char *header;
    char *footer;
};

static const CFCMeta CFCGO_META = {
    "Clownfish::CFC::Binding::Go",
    sizeof(CFCGo),
    (CFCBase_destroy_t)CFCGo_destroy
};

CFCGo*
CFCGo_new(CFCHierarchy *hierarchy) {
    CFCGo *self = (CFCGo*)CFCBase_allocate(&CFCGO_META);
    return CFCGo_init(self, hierarchy);
}

CFCGo*
CFCGo_init(CFCGo *self, CFCHierarchy *hierarchy) {
    CFCUTIL_NULL_CHECK(hierarchy);
    self->hierarchy  = (CFCHierarchy*)CFCBase_incref((CFCBase*)hierarchy);
    self->header     = CFCUtil_strdup("");
    self->footer     = CFCUtil_strdup("");
    return self;
}

void
CFCGo_destroy(CFCGo *self) {
    CFCBase_decref((CFCBase*)self->hierarchy);
    FREEMEM(self->header);
    FREEMEM(self->footer);
    CFCBase_destroy((CFCBase*)self);
}

void
CFCGo_set_header(CFCGo *self, const char *header) {
    CFCUTIL_NULL_CHECK(header);
    free(self->header);
    self->header = CFCUtil_strdup(header);
}

void
CFCGo_set_footer(CFCGo *self, const char *footer) {
    CFCUTIL_NULL_CHECK(footer);
    free(self->footer);
    self->footer = CFCUtil_strdup(footer);
}

void
S_write_hostdefs(CFCGo *self) {
    const char pattern[] =
        "%s\n"
        "\n"
        "#ifndef H_CFISH_HOSTDEFS\n"
        "#define H_CFISH_HOSTDEFS 1\n"
        "\n"
        "#define CFISH_OBJ_HEAD \\\n"
        "    size_t refcount;\n"
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

static char*
S_gen_init(CFCGo *self, CFCParcel *parcel) {
    const char *prefix = CFCParcel_get_prefix(parcel);
    const char pattern[] =
        "func init() {\n"
        "    C.%sbootstrap_parcel()\n"
        "    C.%sglue_cgo_symbols()\n"
        "}\n";
    return CFCUtil_sprintf(pattern, prefix, prefix);
}

static void
S_write_cfbind_go(CFCGo *self, const char *dest, const char *go_short_package,
                  const char *cgo_comment, const char *init_code,
                  const char *custom_go, const char *autogen_go) {
    const char pattern[] =
        "%s"
        "\n"
        "package %s\n"
        "\n"
        "/*\n"
        "%s\n"
        "*/\n"
        "import \"C\"\n"
        "import \"unsafe\"\n"
        "import \"runtime\"\n"
        "\n"
        "%s\n"
        "\n"
        "%s\n"
        "\n"
        "%s\n"
        "\n"
        "//export DummyExport%s\n"
        "func DummyExport%s() int {\n"
        "\treturn 1\n"
        "}\n"
        "%s";
    char *content
        = CFCUtil_sprintf(pattern, self->header, go_short_package,
                          cgo_comment, init_code, custom_go, autogen_go,
                          go_short_package, go_short_package,
                          self->footer);

    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "cfbind.go", dest);
    CFCUtil_write_if_changed(filepath, content, strlen(content));

    FREEMEM(filepath);
    FREEMEM(content);
}

static char*
S_gen_glue_cgo_symbols(CFCGo *self, CFCParcel *parcel) {
    // Generate implementation files containing callback definitions.
    CFCClass **ordered = CFCHierarchy_ordered_classes(self->hierarchy);
    char *decs = CFCUtil_strdup("");
    char *glue = CFCUtil_strdup("");
    for (size_t i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (CFCClass_included(klass)
            || CFCClass_inert(klass)
            || CFCClass_get_parcel(klass) != parcel
           ) {
            continue;
        }

        CFCMethod **fresh_methods = CFCClass_fresh_methods(klass);
        for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
            CFCMethod *method = fresh_methods[meth_num];

            if (CFCMethod_novel(method)
                && !CFCMethod_final(method)
                && CFCGoMethod_can_be_bound(method)
               ) {
                const char *meth_type = CFCMethod_full_typedef(method, NULL);
                const char *override_sym = CFCMethod_full_override_sym(method);
                decs = CFCUtil_cat(decs, "extern ", meth_type, " ",
                                   override_sym, "_wrapper;\n", NULL);
                glue = CFCUtil_cat(glue, "\n    ", override_sym,
                                   "_wrapper = ", override_sym, "_internal;",
                                   NULL);
            }
        }
        FREEMEM(fresh_methods);
    }

    char pattern[] =
        "%s"
        "\n"
        "static CHY_INLINE void\n"
        "%sglue_cgo_symbols(void) {%s\n"
        "}\n"
        "\n";
    char *content = CFCUtil_sprintf(pattern, decs,
                                    CFCParcel_get_prefix(parcel), glue);

    FREEMEM(decs);
    FREEMEM(glue);
    FREEMEM(ordered);

    return content;
}

static void
S_write_cfbind_c(CFCGo *self, const char *dest, const char *glue_cgo_symbols) {
    char pattern[] =
        "%s\n"
        "\n"
        "#include \"_cgo_export.h\"\n"
        "\n"
        "%s\n"
        "\n"
        "%s\n";
    char *content = CFCUtil_sprintf(pattern, self->header, glue_cgo_symbols,
                                    self->footer);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "cfbind.c", dest);
    CFCUtil_write_if_changed(filepath, content, strlen(content));
    FREEMEM(content);
    FREEMEM(filepath);
}

static char*
S_gen_callbacks_cgo(CFCGo *self, CFCParcel *parcel) {
    CFCClass **ordered = CFCHierarchy_ordered_classes(self->hierarchy);
    char *callbacks = CFCUtil_strdup("");

    for (size_t i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (CFCClass_included(klass)
            || CFCClass_inert(klass)
            || CFCClass_get_parcel(klass) != parcel
           ) {
            continue;
        }

        CFCMethod **fresh_methods = CFCClass_fresh_methods(klass);
        for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
            CFCMethod *method = fresh_methods[meth_num];
            if (CFCMethod_novel(method)
                && !CFCMethod_final(method)
                && CFCGoMethod_can_be_bound(method)
               ) {
                char *cb_cgo = CFCGoMethod_callback_cgo(method);
                callbacks = CFCUtil_cat(callbacks, cb_cgo, "\n", NULL);
                FREEMEM(cb_cgo);
            }
        }
        FREEMEM(fresh_methods);
    }

    FREEMEM(ordered);
    return callbacks;
}


void
CFCGo_write_bindings(CFCGo *self, const char *parcel_name, const char *dest) {
    //CFCGoClass **registry = CFCGoClass_registry();
    CFCClass   **ordered  = CFCHierarchy_ordered_classes(self->hierarchy);
    
    // The Go short package name is the last component of the dot-separated
    // Clownfish parcel name.
    const char *parcel_frag = strrchr(parcel_name, '.');
    if (parcel_frag) {
        parcel_frag += 1;
    }
    else {
        parcel_frag = parcel_name;
    }
    // TODO: Don't downcase package name once caps are forbidden in Clownfish
    // parcel names.
    char *go_short_package = CFCUtil_strdup(parcel_frag);
    for (int i = 0; go_short_package[i] != '\0'; i++) {
        go_short_package[i] = tolower(go_short_package[i]);
    }

    CFCParcel *parcel = NULL;
    CFCParcel **parcels  = CFCParcel_all_parcels();
    for (size_t i = 0; parcels[i] != NULL; i++) {
        if (strcmp(CFCParcel_get_name(parcels[0]), parcel_name) == 0) {
            parcel = parcels[i];
            break;
        }
    }
    if (!parcel) {
        CFCUtil_die("Can't bind unknown parcel '%s'", parcel_name);
    }
    
    // Start binding files.
    char *h_includes  = CFCUtil_strdup("");
    char *cgo_comment = CFCUtil_strdup("");
    char *autogen_go  = CFCUtil_strdup("");
    char *custom_go   = CFCUtil_strdup("");

    // Bake in parcel privacy define, so that binding code can be compiled
    // without extra compiler flags.
    {
        const char *privacy_sym = CFCParcel_get_privacy_sym(parcel);
        cgo_comment = CFCUtil_cat(cgo_comment, "#define ", privacy_sym,
                                   "\n\n", NULL);
    }

    // Pound-includes for generated headers.
    for (size_t i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        const char *include_h = CFCClass_include_h(klass);
        h_includes = CFCUtil_cat(h_includes, "#include \"", include_h,
                                   "\"\n", NULL);
    }
    cgo_comment = CFCUtil_cat(cgo_comment, h_includes, "\n", NULL);

    // Prep routine for exposing CGO wrappers outside this Go package.
    char *glue_cgo_symbols = S_gen_glue_cgo_symbols(self, parcel);
    cgo_comment = CFCUtil_cat(cgo_comment, "void\n",
                              CFCParcel_get_prefix(parcel),
                              "glue_cgo_symbols(void);\n\n", NULL);

    // Generate reflect-based wrappers for invoking methods overridden in Go.
    {
        char *callbacks_cgo = S_gen_callbacks_cgo(self, parcel);
        autogen_go = CFCUtil_cat(autogen_go, callbacks_cgo, NULL);
        FREEMEM(callbacks_cgo);
    }

    for (size_t i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (CFCClass_included(klass)
            || CFCClass_get_parcel(klass) != parcel
           ) {
            continue;
        }

        /*
        // Constructors.
        CFCGoConstructor **constructors
            = CFCGoClass_constructor_bindings(klass);
        for (size_t j = 0; constructors[j] != NULL; j++) {
            // Add the function body.
            char *ctor_def = CFCGoConstructor_func_def(constructors[j]);
            autogen_go = CFCUtil_cat(autogen_go, ctor_def, "\n", NULL);
            FREEMEM(ctor_def);
        }
        FREEMEM(constructors);
        */

        // Methods.
        CFCGoMethod **meth_bindings = CFCGoClass_method_bindings(klass);
        for (size_t j = 0; meth_bindings[j] != NULL; j++) {
            CFCGoMethod *meth_binding = meth_bindings[j];

            // Add the method body.
            char *method_def = CFCGoMethod_func_def(meth_binding);
            autogen_go = CFCUtil_cat(autogen_go, method_def, "\n", NULL);
            FREEMEM(method_def);
        }
        FREEMEM(meth_bindings);
    }

    /*
    // Hand-rolled binding code.
    for (size_t i = 0; registry[i] != NULL; i++) {
        CFCGoClass *binding = registry[i];
        if (CFCClass_get_parcel(CFCGoClass_get_client(binding)) != parcel) {
            continue;
        }
        const char *custom_code = CFCGoClass_get_custom_go(registry[i]);
        custom_go = CFCUtil_cat(custom_go, custom_code, "\n", NULL);
    }
    */

    char *init_code = S_gen_init(self, parcel);

    // Write out if there have been any changes.
    S_write_cfbind_go(self, dest, go_short_package, cgo_comment, init_code,
                      custom_go, autogen_go);

    // Write out callbacks, hostdefs.
    S_write_cfbind_c(self, dest, glue_cgo_symbols);
    S_write_hostdefs(self);
    S_write_callbacks(self, parcel, h_includes);

    FREEMEM(glue_cgo_symbols);
    FREEMEM(go_short_package);
    FREEMEM(init_code);
    FREEMEM(h_includes);
    FREEMEM(cgo_comment);
    FREEMEM(ordered);
}

static void
S_write_callbacks(CFCGo *self, CFCParcel *parcel, const char *h_includes) {
    // Generate header files declaring callbacks.
    // TODO: This creates header files for all parcels in source directories,
    // not just the parcel being processed right this moment.  It should be
    // harmless, but it would be better to limit generation to one parcel.
    CFCBindCore *core_binding
        = CFCBindCore_new(self->hierarchy, self->header, self->footer);
    CFCBindCore_write_callbacks_h(core_binding);
    CFCBase_decref((CFCBase*)core_binding);

    // Generate implementation files containing callback definitions.
    CFCClass **ordered = CFCHierarchy_ordered_classes(self->hierarchy);
    char *callbacks = CFCUtil_strdup("");

    for (size_t i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (CFCClass_included(klass)
            || CFCClass_inert(klass)
            || CFCClass_get_parcel(klass) != parcel
           ) {
            continue;
        }

        CFCMethod **fresh_methods = CFCClass_fresh_methods(klass);
        for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
            CFCMethod *method = fresh_methods[meth_num];

            // Define callback.
            if (CFCMethod_novel(method) && !CFCMethod_final(method)) {
                char *cb_def = CFCGoMethod_callback_def(method);
                callbacks = CFCUtil_cat(callbacks, cb_def, "\n", NULL);
                FREEMEM(cb_def);
            }
        }
        FREEMEM(fresh_methods);
    }

    static const char pattern[] =
        "%s\n"
        "%s\n"
        "\n"
        "%s\n"
        "\n"
        "%s";
    char *content = CFCUtil_sprintf(pattern, self->header, h_includes,
                                    callbacks, self->footer);

    // Write if changed.
    const char *src_dest = CFCHierarchy_get_source_dest(self->hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "callbacks.c",
                                     src_dest);
    CFCUtil_write_if_changed(filepath, content, strlen(content));

    FREEMEM(filepath);
    FREEMEM(content);
    FREEMEM(callbacks);
    FREEMEM(ordered);
}

