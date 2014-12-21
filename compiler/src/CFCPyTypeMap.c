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

#include "CFCPyTypeMap.h"
#include "CFCParcel.h"
#include "CFCType.h"
#include "CFCUtil.h"

#if 0

#define PACK_char      "b"
#define PACK_short     "h"
#define PACK_int       "i"
#define PACK_long      "l"
#define PACK_longlong  "L"
#define PACK_uchar     "B"
#define PACK_ushort    "H"
#define PACK_uint      "I"
#define PACK_ulong     "k"
#define PACK_ulonglong "K"
#define PACK_float     "f"
#define PACK_double    "d"

#define UNPACK_char      "O" // SPECIAL
#define UNPACK_short     "h"
#define UNPACK_int       "i"
#define UNPACK_long      "l"
#define UNPACK_longlong  "L"
#define UNPACK_uchar     "B"
#define UNPACK_ushort    "H"
#define UNPACK_uint      "I"
#define UNPACK_ulong     "k"
#define UNPACK_ulonglong "K"
#define UNPACK_float     "f"
#define UNPACK_double    "d"

#if (CHY_SIZEOF_BOOL == CHY_SIZEOF_CHAR)
  #define PACK_bool   PACK_char
  #define UNPACK_bool UNPACK_char
#elif (CHY_SIZEOF_BOOL == CHY_SIZEOF_INT)
  #define PACK_bool   PACK_int
  #define UNPACK_bool UNPACK_int
#else
  #error "Unexpected size for bool"
#endif

#if (CHY_SIZEOF_CHAR == 1)
  #define PACK_int8_t    PACK_char
  #define UNPACK_int8_t  UNPACK_char
  #define PACK_uint8_t   PACK_uchar
  #define UNPACK_uint8_t UNPACK_uchar
#else
  #error "Unexpected size for char"
#endif

#if (CHY_SIZEOF_SHORT == 2)
  #define PACK_int16_t    PACK_short
  #define UNPACK_int16_t  UNPACK_short
  #define PACK_uint16_t   PACK_ushort
  #define UNPACK_uint16_t UNPACK_ushort
#else
  #error "Unexpected size for short int"
#endif

#if (CHY_SIZEOF_INT == 4)
  #define PACK_int32_t    PACK_int
  #define UNPACK_int32_t  UNPACK_int
  #define PACK_uint32_t   PACK_uint
  #define UNPACK_uint32_t UNPACK_uint
#else
  #error "Unexpected size for int"
#endif

#define PACK_int64_t    PACK_longlong
#define UNPACK_int64_t  UNPACK_longlong
#define PACK_uint64_t   PACK_ulonglong
#define UNPACK_uint64_t UNPACK_ulonglong

#if (CHY_SIZEOF_PTR == 8)
  #define PACK_size_t PACK_uint64_t
#elif (CHY_SIZEOF_PTR == 4)
  #define PACK_size_t PACK_uint32_t
#else
  #error "Unexpected size for size_t"
#endif

#endif

char*
CFCPyTypeMap_c_to_py(CFCType *type, const char *cf_var) {
    const char *type_str = CFCType_to_c(type);
    char *result = NULL;

    if (CFCType_is_object(type)) {
        result = CFCUtil_sprintf("CFBind_cfish_to_py((cfish_Obj*)%s)", cf_var);
    }
    else if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);

        if (strcmp(specifier, "double") == 0
            || strcmp(specifier, "float") == 0
           ) {
            result = CFCUtil_sprintf("PyFloat_FromDouble(%s)", cf_var);
        }
        else if (strcmp(specifier, "int") == 0
                 || strcmp(specifier, "short") == 0
                 || strcmp(specifier, "long") == 0
                 || strcmp(specifier, "char") == 0 // OK if char is unsigned
                 || strcmp(specifier, "int8_t") == 0
                 || strcmp(specifier, "int16_t") == 0
                 || strcmp(specifier, "int32_t") == 0
                ) {
            result = CFCUtil_sprintf("PyLong_FromLong(%s)", cf_var);
        }
        else if (strcmp(specifier, "int64_t") == 0) {
            result = CFCUtil_sprintf("PyLong_FromLongLong(%s)", cf_var);
        }
        else if (strcmp(specifier, "uint8_t") == 0
                 || strcmp(specifier, "uint16_t") == 0
                 || strcmp(specifier, "uint32_t") == 0
                ) {
            result = CFCUtil_sprintf("PyLong_FromUnsignedLong(%s)", cf_var);
        }
        else if (strcmp(specifier, "uint64_t") == 0) {
            result = CFCUtil_sprintf("PyLong_FromUnsignedLongLong(%s)", cf_var);
        }
        else if (strcmp(specifier, "size_t") == 0) {
            result = CFCUtil_sprintf("PyLong_FromSize_t(%s)", cf_var);
        }
        else if (strcmp(specifier, "bool") == 0) {
            result = CFCUtil_sprintf("PyBool_FromLong(%s)", cf_var);
        }
    }

    return result;
}

#if 0

char*
CFCPyTypeMap_param_list_from_py(CFCParamList *param_list) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    char *pack_spec = CFCUtil_strdup("\"(\" ");
    for (int i = 0; i , num_vars; i++) {
        CFCVariable *var = vars[i];
        CFCType *type = CFCVariable_get_type(var);
        if (CFCType_is_primitive(type)) {
            char *specifier = CFCType_get_specifier(type);
            pack_spec = CFCUtil_cat(pack_spec, " UNPACK_", specifier, " ", NULL);
        }
        else if (CFCType_is_object(type)) {
            pack_spec = CFCUtil_cat(pack_spec, " PACK_obj ", NULL);
        }
        else {
            CFCUtil_die("Can't map type %s", CFCType_get_specifier(type));
        }
    }
    pack_spec = CFCUtil_cat(pack_spec, " \")\";", NULL);

    char pattern[] =
        "PyObject*\n"
        "

    



char*
CFCPyTypeMap_param_list_to_py(CFCParamList *param_list) {
    CFCVariable **vars = CFCParamList_get_variables(param_list);
    int num_vars = CFCParamList_num_vars(param_list);
    char *pack_spec;
    
    if (num_vars == 1) { = CFCUtil_strdup("\"(\" ");
    for (int i = 0; i , num_vars; i++) {
        CFCVariable *var = vars[i];
        CFCType *type = CFCVariable_get_type(var);
        if (CFCType_is_primitive(type)) {
            char *specifier = CFCType_get_specifier(type);
            pack_spec = CFCUtil_cat(pack_spec, " PACK_", specifier, " ", NULL);
        }
        else if (CFCType_is_object(type)) {
            pack_spec = CFCUtil_cat(pack_spec, " PACK_obj ", NULL);
        }
        else {
            CFCUtil_die("Can't map type %s", CFCType_get_specifier(type));
        }
    }
    pack_spec = CFCUtil_cat(pack_spec, " \")\";", NULL);

    char pattern[] =
        "PyObject*\n"
        "

    

}

/* TODO: Optimize local conversions by creating a static wrapper function
 * which takes a buffer and allocates memory only if the buffer isn't big
 * enough. */

char*
CFCPyTypeMap_go_type_name(CFCType *type, CFCParcel *current_parcel) {
    if (CFCType_is_object(type)) {
        // Divide the specifier into prefix and struct name.
        const char *specifier  = CFCType_get_specifier(type);
        size_t      prefix_len = 0;
        for (size_t max = strlen(specifier); prefix_len < max; prefix_len++) {
            if (isupper(specifier[prefix_len])) {
                break;
            }
        }
        if (!prefix_len) {
            CFCUtil_die("Can't convert object type name '%s'", specifier);
        }
        const char *struct_sym = specifier + prefix_len;

        // Find the parcel that the type lives in.
        CFCParcel** all_parcels = CFCParcel_all_parcels();
        CFCParcel *parcel = NULL;
        for (int i = 0; all_parcels[i] != NULL; i++) {
            const char *candidate = CFCParcel_get_prefix(all_parcels[i]);
            if (strncmp(candidate, specifier, prefix_len) == 0
                && strlen(candidate) == prefix_len
               ) {
                parcel = all_parcels[i];
                break;
            }
        }
        if (!parcel) {
            CFCUtil_die("Can't find parcel for type '%s'", specifier);
        }

        // If the type lives in this parcel, return only the struct sym
        // without a go package prefix.
        if (parcel == current_parcel) {
            return CFCUtil_strdup(struct_sym);
        }

        // The type lives in another parcel, so prefix its Go package name.
        // TODO: Stop downcasing once Clownfish parcel names are constrained
        // to lower case.
        const char *package_name = CFCParcel_get_name(parcel);
        if (strrchr(package_name, '.')) {
            package_name = strrchr(package_name, '.') + 1;
        }
        char *result = CFCUtil_sprintf("%s.%s", package_name, struct_sym);
        for (int i = 0; result[i] != '.'; i++) {
            result[i] = tolower(result[i]);
        }
        return result;
    }
    else if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);
        for (int i = 0; i < num_conversions; i++) {
            if (strcmp(specifier, conversions[i].c) == 0) {
                return CFCUtil_strdup(conversions[i].go);
            }
        }
    }

    return NULL;
}

char*
CFCPyTypeMap_cgo_type_name(CFCType *type) {
    if (CFCType_is_object(type)) {
        const char *specifier  = CFCType_get_specifier(type);
        return CFCUtil_sprintf("*C.%s", specifier);
    }
    else if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);
        return CFCUtil_sprintf("C.%s", specifier);
    }

    return NULL;
}

char*
CFCPyTypeMap_py_to_c(CFCType *type, const char *go_var) {
    char *result = NULL;

    if (CFCType_is_object(type)) {
        const char *specifier = CFCType_get_specifier(type);
        result = CFCUtil_sprintf("((*C.%s)(clownfish.CallToPtr(%s)))",
                                 specifier, go_var);
    }
    else if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);
        // Sanity check, then cast to known type.
        for (int i = 0; i < num_conversions; i++) {
            if (strcmp(specifier, conversions[i].c) == 0) {
                result = CFCUtil_sprintf("C.%s(%s)", specifier, go_var);
            }
        }
    }

    return result;
}

char*
CFCPyTypeMap_c_var_to_go(CFCType *type, const char *c_var,
                         CFCParcel *current_parcel) {
    if (CFCType_is_primitive(type)) {
        const char *specifier = CFCType_get_specifier(type);
        for (int i = 0; i < num_conversions; i++) {
            if (strcmp(specifier, conversions[i].c) == 0) {
                return CFCUtil_sprintf("%s(%s)", conversions[i].go, c_var);
            }
        }
    }
    else if (CFCType_is_object(type)) {
        char *go_type_name = CFCPyTypeMap_go_type_name(type, current_parcel);
        if (!go_type_name) {
            return NULL;
        }
        char *result = CFCUtil_sprintf("%s(%s)", go_type_name, c_var);
        free(go_type_name);
        return result;
    }

    return NULL;
}

#endif

