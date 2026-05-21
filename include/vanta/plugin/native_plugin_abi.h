#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(VANTA_PLUGIN_BUILD)
#define VANTA_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VANTA_PLUGIN_EXPORT __declspec(dllimport)
#endif
#else
#define VANTA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define VANTA_PLUGIN_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VantaHost VantaHost;
typedef struct VantaPlugin VantaPlugin;
typedef struct VantaHandle VantaHandle;

typedef struct VantaStringView {
    const char* data;
    size_t size;
} VantaStringView;

typedef enum VantaStatusCode {
    VANTA_STATUS_OK = 0,
    VANTA_STATUS_INVALID_ARGUMENT = 1,
    VANTA_STATUS_NOT_FOUND = 2,
    VANTA_STATUS_DENIED = 3,
    VANTA_STATUS_INTERNAL_ERROR = 4
} VantaStatusCode;

typedef struct VantaStatus {
    VantaStatusCode code;
    VantaStringView message;
} VantaStatus;

typedef void (*VantaReleaseCallback)(void* value);

typedef struct VantaOwnedBytes {
    const uint8_t* data;
    size_t size;
    void* user_data;
    VantaReleaseCallback release;
} VantaOwnedBytes;

typedef VantaStatus (*VantaCommandCallback)(VantaHost* host, void* user_data);

typedef struct VantaCommandDescriptor {
    uint32_t struct_size;
    VantaStringView id;
    VantaStringView title;
    VantaStringView description;
    void* user_data;
    VantaCommandCallback callback;
} VantaCommandDescriptor;

typedef struct VantaHostApi {
    uint32_t abi_version;
    uint32_t struct_size;

    VantaStatus (*register_command)(
        VantaHost* host,
        const VantaCommandDescriptor* descriptor,
        VantaHandle** out_handle);

    void (*release_handle)(VantaHost* host, VantaHandle* handle);
    void (*release_owned_bytes)(VantaHost* host, VantaOwnedBytes bytes);
} VantaHostApi;

typedef struct VantaPluginCreateInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    VantaStringView plugin_id;
    VantaStringView workspace_uri;
} VantaPluginCreateInfo;

VANTA_PLUGIN_EXPORT VantaStatus vanta_plugin_create(
    const VantaHostApi* host_api,
    VantaHost* host,
    const VantaPluginCreateInfo* create_info,
    VantaPlugin** out_plugin);

VANTA_PLUGIN_EXPORT void vanta_plugin_destroy(VantaPlugin* plugin);

#ifdef __cplusplus
}
#endif
