/* See vk_loader.h. */
#define _GNU_SOURCE
#include "vk_loader.h"
#include "../../adrenotools/include/adrenotools/driver.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <android/log.h>

#define TAG "DroidDeckWayland"

struct vk_api g_vk;
static void *g_handle;
static int g_custom;  /* g_handle is the adrenotools driver, not the system libvulkan */
static PFN_vkGetInstanceProcAddr g_gip;

int vk_loader_open(const char *driver_path, const char *library_name,
                   const char *native_lib_dir) {
    if (driver_path && library_name && native_lib_dir) {
        char *tmpdir = NULL;
        if (asprintf(&tmpdir, "%stemp", driver_path) < 0) tmpdir = NULL;
        if (tmpdir) mkdir(tmpdir, S_IRWXU | S_IRWXG);
        g_handle = adrenotools_open_libvulkan(
            RTLD_LOCAL | RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmpdir,
            native_lib_dir, driver_path, library_name, NULL, NULL);
        free(tmpdir);
        g_custom = g_handle != NULL;
        __android_log_print(g_handle ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG,
                            "adrenotools Turnip handle=%p (%s / %s)", g_handle,
                            driver_path, library_name);
    }
    if (!g_handle) {
        g_handle = dlopen("libvulkan.so", RTLD_LOCAL | RTLD_NOW);
        __android_log_print(ANDROID_LOG_WARN, TAG,
                            "no adrenotools driver -> system libvulkan handle=%p "
                            "(dmabuf import likely unsupported)", g_handle);
    }
    if (!g_handle) return -1;
    g_gip = (PFN_vkGetInstanceProcAddr)dlsym(g_handle, "vkGetInstanceProcAddr");
    if (!g_gip) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "no vkGetInstanceProcAddr");
        return -1;
    }
#define X(n) g_vk.n = (PFN_vk##n)g_gip(NULL, "vk" #n);
    VK_GLOBAL_FUNCS(X)
#undef X
    return g_vk.CreateInstance ? 0 : -1;
}

int vk_loader_is_custom(void) { return g_custom; }

int vk_loader_open_system(void) {
    /* The custom driver's handle stays loaded: adrenotools offers no safe way to unload it. */
    g_handle = NULL;
    g_gip = NULL;
    g_custom = 0;
    return vk_loader_open(NULL, NULL, NULL);
}

PFN_vkGetInstanceProcAddr vk_loader_gipa(void) { return g_gip; }

void vk_loader_load_instance(VkInstance instance) {
#define X(n) g_vk.n = (PFN_vk##n)g_gip(instance, "vk" #n);
    VK_INSTANCE_FUNCS(X)
#undef X
}

void vk_loader_load_device(VkDevice device) {
#define X(n) g_vk.n = (PFN_vk##n)g_vk.GetDeviceProcAddr(device, "vk" #n);
    VK_DEVICE_FUNCS(X)
#undef X
}
