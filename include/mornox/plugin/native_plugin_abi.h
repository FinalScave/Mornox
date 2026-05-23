#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(MORNOX_PLUGIN_BUILD)
#define MORNOX_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MORNOX_PLUGIN_EXPORT __declspec(dllimport)
#endif
#else
#define MORNOX_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define MORNOX_PLUGIN_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MornoxHost MornoxHost;
typedef struct MornoxPlugin MornoxPlugin;
typedef struct MornoxHandle MornoxHandle;

typedef struct MornoxStringView {
    const char* data;
    size_t size;
} MornoxStringView;

typedef enum MornoxStatusCode {
    MORNOX_STATUS_OK = 0,
    MORNOX_STATUS_INVALID_ARGUMENT = 1,
    MORNOX_STATUS_NOT_FOUND = 2,
    MORNOX_STATUS_DENIED = 3,
    MORNOX_STATUS_INTERNAL_ERROR = 4
} MornoxStatusCode;

typedef struct MornoxStatus {
    MornoxStatusCode code;
    MornoxStringView message;
} MornoxStatus;

typedef void (*MornoxReleaseCallback)(void* value);

typedef struct MornoxOwnedBytes {
    const uint8_t* data;
    size_t size;
    void* user_data;
    MornoxReleaseCallback release;
} MornoxOwnedBytes;

typedef MornoxStatus (*MornoxCommandCallback)(MornoxHost* host, void* user_data);

typedef struct MornoxCommandDescriptor {
    uint32_t struct_size;
    MornoxStringView id;
    MornoxStringView title;
    MornoxStringView description;
    void* user_data;
    MornoxCommandCallback callback;
} MornoxCommandDescriptor;

typedef struct MornoxHostApi {
    uint32_t abi_version;
    uint32_t struct_size;

    MornoxStatus (*register_command)(
        MornoxHost* host,
        const MornoxCommandDescriptor* descriptor,
        MornoxHandle** out_handle);

    void (*release_handle)(MornoxHost* host, MornoxHandle* handle);
    void (*release_owned_bytes)(MornoxHost* host, MornoxOwnedBytes bytes);
} MornoxHostApi;

typedef struct MornoxPluginCreateInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    MornoxStringView plugin_id;
    MornoxStringView workspace_uri;
} MornoxPluginCreateInfo;

MORNOX_PLUGIN_EXPORT MornoxStatus mornox_plugin_create(
    const MornoxHostApi* host_api,
    MornoxHost* host,
    const MornoxPluginCreateInfo* create_info,
    MornoxPlugin** out_plugin);

MORNOX_PLUGIN_EXPORT void mornox_plugin_destroy(MornoxPlugin* plugin);

#ifdef __cplusplus
}
#endif
