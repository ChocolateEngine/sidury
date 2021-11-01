/*
 *    shim.h ( Authored by p0lyh3dron )
 *
 *    Include this file to use auto-generated shims
 *    for large DLL imports.
 */

#pragma once

#define MAX_DLL_FUNCS 128

#define DLLOPEN( module ) \
        extern void init_##module##_shims(  );          \
        extern void *p##module##Ptrs[ MAX_DLL_FUNCS ];

#define DLLFUNC( return, funcName, ... )        \
        extern return funcName( ... ); \

#define DLLEXPORT( functions ) \
        extern "C" \
        { \
                functions                       \
        }
