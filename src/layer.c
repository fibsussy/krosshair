#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <math.h>

#include "../include/dispatch.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/default_crosshair.h"
#include "../include/stb_image.h"

#define K_NO_MEMORYTYPE 0xFFFFFFFF

#define vk_foreach_struct(__iter, __start)              \
        for (struct VkBaseOutStructure* __iter =        \
                 (struct VkBaseOutStructure*)(__start); \
             __iter; __iter = __iter->pNext)

#define vk_foreach_struct_const(__iter, __start)             \
        for (const struct VkBaseInStructure* __iter =        \
                 (const struct VkBaseInStructure*)(__start); \
             __iter; __iter = __iter->pNext)

#define KROSSHAIR_LOG(fmt, ...) \
        do { \
                FILE* _kf = fopen("/tmp/krosshair.log", "a"); \
                if (_kf) { \
                        fprintf(_kf, fmt, ##__VA_ARGS__); \
                        fclose(_kf); \
                } \
        } while (0)

#define VK_CHECK(expr)                        \
        do {                                  \
                VkResult __result = (expr);   \
                if (__result != VK_SUCCESS) { \
                        printf("error\n");    \
                }                             \
        } while (0)

pthread_mutex_t global_lock;
VkPhysicalDeviceDriverProperties driver_properties = {};

unsigned char frag_spv[]                           = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00,
    0x02, 0x00, 0x00, 0x00, 0xc2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0a, 0x00,
    0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x63, 0x70,
    0x70, 0x5f, 0x73, 0x74, 0x79, 0x6c, 0x65, 0x5f, 0x6c, 0x69, 0x6e, 0x65,
    0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00,
    0x04, 0x00, 0x08, 0x00, 0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c,
    0x45, 0x5f, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x5f, 0x64, 0x69,
    0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x05, 0x00, 0x09, 0x00, 0x00, 0x00, 0x6f, 0x75, 0x74, 0x5f,
    0x63, 0x6f, 0x6c, 0x6f, 0x72, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x5f, 0x73, 0x61, 0x6d, 0x70,
    0x6c, 0x65, 0x72, 0x00, 0x05, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x66, 0x72, 0x61, 0x67, 0x5f, 0x74, 0x65, 0x78, 0x5f, 0x63, 0x6f, 0x6f,
    0x72, 0x64, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x47, 0x00, 0x04, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3b, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x19, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x57, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00,
    0x38, 0x00, 0x01, 0x00};

unsigned char vert_spv[] = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00,
    0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
    0x1d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xc2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x47, 0x4c, 0x5f, 0x47,
    0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x63, 0x70, 0x70, 0x5f, 0x73, 0x74,
    0x79, 0x6c, 0x65, 0x5f, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x64, 0x69, 0x72,
    0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00,
    0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x69, 0x6e,
    0x63, 0x6c, 0x75, 0x64, 0x65, 0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74,
    0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x50, 0x65, 0x72, 0x56, 0x65,
    0x72, 0x74, 0x65, 0x78, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x50,
    0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x06, 0x00, 0x07, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x50,
    0x6f, 0x69, 0x6e, 0x74, 0x53, 0x69, 0x7a, 0x65, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x07, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x67, 0x6c, 0x5f, 0x43, 0x6c, 0x69, 0x70, 0x44, 0x69, 0x73, 0x74, 0x61,
    0x6e, 0x63, 0x65, 0x00, 0x06, 0x00, 0x07, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x43, 0x75, 0x6c, 0x6c, 0x44,
    0x69, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x00, 0x05, 0x00, 0x03, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
    0x12, 0x00, 0x00, 0x00, 0x69, 0x6e, 0x5f, 0x70, 0x6f, 0x73, 0x69, 0x74,
    0x69, 0x6f, 0x6e, 0x00, 0x05, 0x00, 0x06, 0x00, 0x1c, 0x00, 0x00, 0x00,
    0x66, 0x72, 0x61, 0x67, 0x5f, 0x74, 0x65, 0x78, 0x5f, 0x63, 0x6f, 0x6f,
    0x72, 0x64, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x1d, 0x00, 0x00, 0x00,
    0x69, 0x6e, 0x5f, 0x74, 0x65, 0x78, 0x5f, 0x63, 0x6f, 0x6f, 0x72, 0x64,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x47, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x1c, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
    0x1d, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x15, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x04, 0x00,
    0x0a, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x2b, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x3b, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f,
    0x20, 0x00, 0x04, 0x00, 0x19, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x1b, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
    0x1b, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x3b, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0xf8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
    0x19, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x1a, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
    0x1c, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00,
    0x38, 0x00, 0x01, 0x00};

typedef struct vec2 {
        float x;
        float y;
} vec2_t;

typedef struct vec3 {
        float x;
        float y;
        float z;
} vec3_t;

typedef struct vertex {
        vec2_t pos;
        vec2_t tex_pos;
} vertex_t;

typedef struct vk_object {
        uint64_t obj;
        void* data;
        const char* name;  // only really for debugging purposes
} vk_object_t;

#define MAX_VK_OBJECTS 512
typedef struct vk_object_map {
        vk_object_t data[MAX_VK_OBJECTS];
        size_t count;
} vk_object_map_t;

/*
 * returns index on success, -1 on failure
 * */
int vk_map_set(vk_object_map_t* map, uint64_t obj, void* data, const char* name)
{
        // check if entry already exists
        for (size_t i = 0; i < MAX_VK_OBJECTS; i++) {
                if (map->data[i].obj == obj) {
                        map->data[i].data = data;
                        map->data[i].name = name;
                        map->count++;
                        return i;
                }
        }

        // if it doesn't already exist, find the first free one
        for (size_t i = 0; i < MAX_VK_OBJECTS; i++) {
                if (map->data[i].obj == 0) {
                        map->data[i].obj  = obj;
                        map->data[i].data = data;
                        map->data[i].name = name;
                        map->count++;
                        return i;
                }
        }

        return -1;
}

/*
 * returns 0 on failure, 1 on success
 * */
int vk_map_get(vk_object_map_t* map, uint64_t obj, vk_object_t* obj_out)
{
        for (size_t i = 0; i < MAX_VK_OBJECTS; i++) {
                if (map->data[i].obj == obj) {
                        *obj_out = map->data[i];
                        return 1;
                }
        }

        return 0;
}

void vk_map_print(vk_object_map_t* map)
{
        printf(
            "#################################### [ITERATING MAP] "
            "######################################\n");
        for (size_t i = 0; i < MAX_VK_OBJECTS; i++) {
                if (map->data[i].obj == 0) continue;

                printf("item %lu (%s) has obj: \t\t %lu, data \t\t %p\n", i,
                       map->data[i].name, map->data[i].obj, map->data[i].data);
        }
        printf(
            "##################################################################"
            "####"
            "#####################\n");
}

void vk_map_delete(vk_object_map_t* map, uint64_t obj)
{
        for (size_t i = 0; i < MAX_VK_OBJECTS; i++) {
                if (map->data[i].obj == obj) {
                        map->data[i].obj  = 0;
                        map->data[i].data = NULL;
                        map->data[i].name = NULL;
                        map->count--;
                }
        }
}

vk_object_map_t vk_obj_map;

typedef struct instance_data {
        instance_dispatch_table_t vtable;
        VkInstance instance;
        uint32_t api_version;
} instance_data_t;

typedef struct queue_data {
        struct device_data* device;
        VkQueue queue;
        VkQueueFlags flags;
        uint32_t family_index;
} queue_data_t;

typedef struct device_data {
        device_dispatch_table_t vtable;
        instance_data_t* instance;
        PFN_vkSetDeviceLoaderData set_device_loader_data;
        VkPhysicalDevice physical_device;
        VkDevice device;
        VkPhysicalDeviceProperties properties;
        struct queue_data* graphic_queue;
        struct queue_data* queues[16];  // 16 should be enough?
        uint32_t queue_count;
} device_data_t;

typedef struct command_buffer_data {
        device_data_t* device_data;
        VkCommandBufferLevel level;
        VkCommandBuffer cmd_buffer;
        queue_data_t* queue_data;
} cmd_buffer_data_t;

typedef struct krosshair_draw {
        VkCommandBuffer cmd_buffer;

        VkSemaphore crossengine_semaphore;
        VkSemaphore semaphore;
        VkFence fence;

        VkBuffer vertex_buffer;
        VkDeviceMemory vertex_buffer_mem;
        VkDeviceSize vertex_buffer_size;

        VkBuffer index_buffer;
        VkDeviceMemory index_buffer_mem;
        VkDeviceSize index_buffer_size;
} krosshair_draw_t;

typedef struct swapchain_data {
        device_data_t* device_data;

        VkSwapchainKHR swapchain;
        uint32_t width, height;
        VkFormat format;

        uint32_t n_images;

        VkImage images[16];
        VkImageView image_views[16];
        VkFramebuffer framebuffers[16];

        VkRenderPass render_pass;

        VkDescriptorPool descriptor_pool;
        VkDescriptorSetLayout descriptor_layout;
        VkDescriptorSet descriptor_set;

        VkSampler crosshair_sampler;

        VkPipelineLayout pipeline_layout;
        VkPipeline pipeline;

        VkCommandPool cmd_pool;

        int crosshair_uploaded;
        VkImage crosshair_image;
        VkImageView crosshair_image_view;
        VkDeviceMemory crosshair_mem;
        VkBuffer crosshair_upload_buffer;
        VkDeviceMemory crosshair_upload_buffer_mem;

        char* crosshair_path;
        struct timespec crosshair_mtime;

        krosshair_draw_t* draw;

} swapchain_data_t;

// TODO: change all of this to be adjustable at runtime
// const float texture_size_px       = 24.0f;
// const float screen_width          = 1920.0f;
// const float screen_height         = 1080.0f;
//
// const float width                 = (texture_size_px / screen_width) * 2.0f;
// const float height                = (texture_size_px / screen_height) * 2.0f;
//
// const float tex_height            = 520.0f;
// const float tex_width             = 520.0f;
//
// const float texture_correct_width = width * (tex_height / tex_width);

// vertex_t vertices[]               = {
//     {{-texture_correct_width, -height}, {0.0f, 0.0f}},
//     { {texture_correct_width, -height}, {1.0f, 0.0f}},
//     {  {texture_correct_width, height}, {1.0f, 1.0f}},
//     { {-texture_correct_width, height}, {0.0f, 1.0f}}
// };
//
vertex_t vertices[4] = {0};

static void setup_vertices(float canvas_width, float canvas_height,
                           float tex_width, float tex_height, float scale)
{
        float width_ndc  = ((tex_width * scale) / canvas_width);
        float height_ndc = ((tex_width * scale)/ canvas_height) ;

        /* should fix even-length crosshairs */
        float pixel_offset_x =
            (fmod(canvas_width, 2.0f) == 0) ? 0.0f : (1.0f / canvas_width);
        float pixel_offset_y =
            (fmod(canvas_height, 2.0f) == 0) ? 0.0f : (1.0f / canvas_height);

        vertices[0].pos =
            (vec2_t){-width_ndc + pixel_offset_x, -height_ndc + pixel_offset_y};
        vertices[0].tex_pos = (vec2_t){0.0f, 0.0f};
        vertices[1].pos =
            (vec2_t){width_ndc + pixel_offset_x, -height_ndc + pixel_offset_y};
        vertices[1].tex_pos = (vec2_t){1.0f, 0.0f};
        vertices[2].pos =
            (vec2_t){width_ndc + pixel_offset_x, height_ndc + pixel_offset_y};
        vertices[2].tex_pos = (vec2_t){1.0f, 1.0f};
        vertices[3].pos =
            (vec2_t){-width_ndc + pixel_offset_x, height_ndc + pixel_offset_y};
        vertices[3].tex_pos = (vec2_t){0.0f, 1.0f};
}

uint16_t indices[] = {0, 1, 2, 2, 3, 0};

static uint32_t vk_memory_type(device_data_t* device_data,
                               VkMemoryPropertyFlags properties,
                               uint32_t type_bits)
{
        VkPhysicalDeviceMemoryProperties _properties;
        device_data->instance->vtable.GetPhysicalDeviceMemoryProperties(
            device_data->physical_device, &_properties);
        for (uint32_t i = 0; i < _properties.memoryTypeCount; i++) {
                if ((_properties.memoryTypes[i].propertyFlags & properties) ==
                        properties &&
                    type_bits & (1 << i))
                        return i;
        }
        return K_NO_MEMORYTYPE;
}

static void* find_object_data(uint64_t obj)
{
        pthread_mutex_lock(&global_lock);

        vk_object_t temp_obj;
        if (!vk_map_get(&vk_obj_map, obj, &temp_obj)) {
                pthread_mutex_unlock(&global_lock);
                return NULL;
        }

        pthread_mutex_unlock(&global_lock);
        return temp_obj.data;
}

static void map_object(uint64_t obj, void* data, const char* name)
{
        pthread_mutex_lock(&global_lock);

        if (vk_map_set(&vk_obj_map, obj, data, name) == -1) {
                pthread_mutex_unlock(&global_lock);
                return;
        }
        // vk_map_print(&vk_obj_map);
        pthread_mutex_unlock(&global_lock);
}

static void unmap_object(uint64_t obj)
{
        pthread_mutex_lock(&global_lock);
        vk_map_delete(&vk_obj_map, obj);
        pthread_mutex_unlock(&global_lock);
}

#define HKEY(obj)           ((uint64_t)(obj))
#define FIND_OBJ(type, obj) ((type*)(find_object_data(HKEY(obj))))

static VkLayerInstanceCreateInfo* get_instance_chain_info(
    const VkInstanceCreateInfo* p_create_info, VkLayerFunction func)
{
        vk_foreach_struct(item, p_create_info->pNext)
        {
                if (item->sType ==
                        VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
                    ((VkLayerInstanceCreateInfo*)item)->function == func)
                        return (VkLayerInstanceCreateInfo*)item;
        }
        return NULL;
}

static instance_data_t* new_instance_data(VkInstance instance)
{
        instance_data_t* instance_data = malloc(sizeof(instance_data_t));
        instance_data->instance        = instance;
        printf("[*] mapping data->instance obj: %lu data: %p\n",
               HKEY(instance_data->instance), instance_data);
        map_object(HKEY(instance_data->instance), instance_data,
                   "instance_data->instance");
        return instance_data;
}

static device_data_t* new_device_data(VkDevice device,
                                      instance_data_t* instance)
{
        device_data_t* device_data = malloc(sizeof(device_data_t));
        device_data->instance      = instance;
        device_data->device        = device;
        printf("[*] mapping data->device obj: %lu %p\n",
               HKEY(device_data->device), device_data);
        map_object(HKEY(device_data->device), device_data,
                   "device_data->device");
        return device_data;
}

static cmd_buffer_data_t* new_cmd_buffer_data(VkCommandBuffer cmd_buffer,
                                              VkCommandBufferLevel level,
                                              device_data_t* device_data)
{
        cmd_buffer_data_t* cmdbuffer_data = malloc(sizeof(cmd_buffer_data_t));
        cmdbuffer_data->device_data       = device_data;
        cmdbuffer_data->cmd_buffer        = cmd_buffer;
        cmdbuffer_data->level             = level;
        printf("[*] mapping data->cmd_buffer obj: %lu %p\n",
               HKEY(cmdbuffer_data->cmd_buffer), cmdbuffer_data);
        map_object(HKEY(cmdbuffer_data->cmd_buffer), cmdbuffer_data,
                   "cmdbuffer_data->cmd_buffer");
        return cmdbuffer_data;
}

static void create_or_resize_buffer(device_data_t* device_data,
                                    VkBuffer* buffer,
                                    VkDeviceMemory* buffer_mem,
                                    VkDeviceSize* buffer_size, size_t new_size,
                                    VkBufferUsageFlagBits usage)
{
        if (*buffer != VK_NULL_HANDLE) {
                device_data->vtable.DestroyBuffer(device_data->device, *buffer,
                                                  NULL);
        }
        if (*buffer_mem) {
                device_data->vtable.FreeMemory(device_data->device, *buffer_mem,
                                               NULL);
        }

        if (device_data->properties.limits.nonCoherentAtomSize > 0) {
                VkDeviceSize atom_size =
                    device_data->properties.limits.nonCoherentAtomSize - 1;
                new_size = (new_size + atom_size) & ~atom_size;
        }

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size               = new_size;
        buffer_info.usage              = usage;
        buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(device_data->vtable.CreateBuffer(device_data->device,
                                                  &buffer_info, NULL, buffer));

        VkMemoryRequirements req;
        device_data->vtable.GetBufferMemoryRequirements(device_data->device,
                                                        *buffer, &req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex =
            vk_memory_type(device_data, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                           req.memoryTypeBits);
        VK_CHECK(device_data->vtable.AllocateMemory(
            device_data->device, &alloc_info, NULL, buffer_mem));

        VK_CHECK(device_data->vtable.BindBufferMemory(device_data->device,
                                                      *buffer, *buffer_mem, 0));
        *buffer_size = new_size;
}

static void shutdown_krosshair_image(swapchain_data_t* data)
{
        device_data_t* device_data = data->device_data;

        if (data->crosshair_image_view) {
                device_data->vtable.DestroyImageView(device_data->device,
                                                     data->crosshair_image_view, NULL);
                data->crosshair_image_view = VK_NULL_HANDLE;
        }
        if (data->crosshair_image) {
                device_data->vtable.DestroyImage(device_data->device,
                                                 data->crosshair_image, NULL);
                data->crosshair_image = VK_NULL_HANDLE;
        }
        if (data->crosshair_mem) {
                device_data->vtable.FreeMemory(device_data->device, data->crosshair_mem,
                                               NULL);
                data->crosshair_mem = VK_NULL_HANDLE;
        }

        if (data->crosshair_upload_buffer) {
                device_data->vtable.DestroyBuffer(device_data->device,
                                                  data->crosshair_upload_buffer, NULL);
                data->crosshair_upload_buffer = VK_NULL_HANDLE;
        }
        if (data->crosshair_upload_buffer_mem) {
                device_data->vtable.FreeMemory(device_data->device,
                                               data->crosshair_upload_buffer_mem, NULL);
                data->crosshair_upload_buffer_mem = VK_NULL_HANDLE;
        }

        if (data->descriptor_set) {
                device_data->vtable.FreeDescriptorSets(device_data->device,
                                                       data->descriptor_pool,
                                                       1, &data->descriptor_set);
                data->descriptor_set = VK_NULL_HANDLE;
        }
}

static void update_image_descriptor(swapchain_data_t* data,
                                    VkImageView image_view, VkDescriptorSet set)
{
        device_data_t* device_data       = data->device_data;

        VkDescriptorImageInfo desc_image = {};
        desc_image.sampler               = data->crosshair_sampler;
        desc_image.imageView             = image_view;
        desc_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_desc = {};
        write_desc.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc.dstSet          = set;
        write_desc.descriptorCount = 1;
        write_desc.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_desc.pImageInfo      = &desc_image;
        device_data->vtable.UpdateDescriptorSets(device_data->device, 1,
                                                 &write_desc, 0, NULL);
}

static void create_image(swapchain_data_t* data, VkDescriptorSet descriptor_set,
                         uint32_t width, uint32_t height, VkFormat format,
                         VkImage* image, VkDeviceMemory* image_mem,
                         VkImageView* image_view)
{
        device_data_t* device_data   = data->device_data;

        VkImageCreateInfo image_info = {};
        image_info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType         = VK_IMAGE_TYPE_2D;
        image_info.format            = format;
        image_info.extent.width      = width;
        image_info.extent.height     = height;
        image_info.extent.depth      = 1;
        image_info.mipLevels         = 1;
        image_info.arrayLayers       = 1;
        image_info.samples           = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling            = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(device_data->vtable.CreateImage(device_data->device,
                                                 &image_info, NULL, image));

        VkMemoryRequirements kh_image_req;
        device_data->vtable.GetImageMemoryRequirements(device_data->device,
                                                       *image, &kh_image_req);

        VkMemoryAllocateInfo image_alloc_info = {};
        image_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        image_alloc_info.allocationSize = kh_image_req.size;
        image_alloc_info.memoryTypeIndex =
            vk_memory_type(device_data, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           kh_image_req.memoryTypeBits);
        VK_CHECK(device_data->vtable.AllocateMemory(
            device_data->device, &image_alloc_info, NULL, image_mem));
        VK_CHECK(device_data->vtable.BindImageMemory(device_data->device,
                                                     *image, *image_mem, 0));

        VkImageViewCreateInfo view_info = {};
        view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image    = *image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format   = format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        VK_CHECK(device_data->vtable.CreateImageView(
            device_data->device, &view_info, NULL, image_view));

        update_image_descriptor(data, *image_view, descriptor_set);
}

static VkDescriptorSet create_image_with_desc(swapchain_data_t* data,
                                              uint32_t width, uint32_t height,
                                              VkFormat format, VkImage* image,
                                              VkDeviceMemory* image_mem,
                                              VkImageView* image_view)
{
        device_data_t* device_data             = data->device_data;

        VkDescriptorSet descriptor_set         = {};

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = data->descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts        = &data->descriptor_layout;
        VK_CHECK(device_data->vtable.AllocateDescriptorSets(
            device_data->device, &alloc_info, &descriptor_set));

        create_image(data, descriptor_set, width, height, format, image,
                     image_mem, image_view);
        return descriptor_set;
}

static void upload_image_data(device_data_t* device_data,
                              VkCommandBuffer cmd_buffer, void* pixels,
                              VkDeviceSize upload_size, uint32_t width,
                              uint32_t height, VkBuffer* upload_buffer,
                              VkDeviceMemory* upload_buffer_mem, VkImage image)
{
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size               = upload_size;
        buffer_info.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(device_data->vtable.CreateBuffer(
            device_data->device, &buffer_info, NULL, upload_buffer));

        VkMemoryRequirements upload_buffer_req;
        device_data->vtable.GetBufferMemoryRequirements(
            device_data->device, *upload_buffer, &upload_buffer_req);

        VkMemoryAllocateInfo upload_alloc_info = {};
        upload_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        upload_alloc_info.allocationSize = upload_buffer_req.size;
        upload_alloc_info.memoryTypeIndex =
            vk_memory_type(device_data, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                           upload_buffer_req.memoryTypeBits);
        VK_CHECK(device_data->vtable.AllocateMemory(
            device_data->device, &upload_alloc_info, NULL, upload_buffer_mem));
        VK_CHECK(device_data->vtable.BindBufferMemory(
            device_data->device, *upload_buffer, *upload_buffer_mem, 0));

        char* map = NULL;
        VK_CHECK(device_data->vtable.MapMemory(device_data->device,
                                               *upload_buffer_mem, 0,
                                               upload_size, 0, (void**)(&map)));
        memcpy(map, pixels, upload_size);
        VkMappedMemoryRange range = {};
        range.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory              = *upload_buffer_mem;
        range.size                = upload_size;
        VK_CHECK(device_data->vtable.FlushMappedMemoryRanges(
            device_data->device, 1, &range));
        device_data->vtable.UnmapMemory(device_data->device,
                                        *upload_buffer_mem);

        VkImageMemoryBarrier copy_barrier = {};
        copy_barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.image                       = image;
        copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier.subresourceRange.levelCount = 1;
        copy_barrier.subresourceRange.layerCount = 1;
        device_data->vtable.CmdPipelineBarrier(
            cmd_buffer, VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
            &copy_barrier);

        VkBufferImageCopy region           = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width           = width;
        region.imageExtent.height          = height;
        region.imageExtent.depth           = 1;
        device_data->vtable.CmdCopyBufferToImage(
            cmd_buffer, *upload_buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier = {};
        use_barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.image                       = image;
        use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier.subresourceRange.levelCount = 1;
        use_barrier.subresourceRange.layerCount = 1;
        device_data->vtable.CmdPipelineBarrier(
            cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
            &use_barrier);
}

// malloc's returned string, free later
static char* get_crosshair_file(const char* path)
{
        if (!path) return NULL;

        if (!(path[0] == '~')) return strdup(path);

        const char* home_dir = NULL;

        if (path[1] == '/' || path[1] == '\0') {
                home_dir = getenv("HOME");
                if (!home_dir) return NULL;
        }

        const char* rest_str = path + 1;
        while (*rest_str && *rest_str != '/') rest_str++;

        char* expanded_str = malloc(strlen(home_dir) + strlen(rest_str) + 1);
        strcpy(expanded_str, home_dir);
        strcat(expanded_str, rest_str);
        return expanded_str;
}

static int get_file_mtime(const char* path, struct timespec* mtime)
{
        struct stat st;
        if (stat(path, &st) != 0) return -1;
        *mtime = st.st_mtim;
        return 0;
}

static void ensure_swapchain_crosshair(swapchain_data_t* data,
                                       VkCommandBuffer cmd_buffer)
{
        device_data_t* device_data = data->device_data;

        char* crosshair_path = get_crosshair_file(getenv("KROSSHAIR_IMG"));
        int using_file = (crosshair_path != NULL);

        if (data->crosshair_uploaded) {
                int needs_reload = 0;

                if (using_file && data->crosshair_path) {
                        if (strcmp(data->crosshair_path, crosshair_path) != 0) {
                                KROSSHAIR_LOG("[KROSSHAIR] path changed, reloading\n");
                                needs_reload = 1;
                        } else {
                                struct timespec new_mtime;
                                if (get_file_mtime(crosshair_path, &new_mtime) == 0) {
                                        if (new_mtime.tv_sec != data->crosshair_mtime.tv_sec ||
                                            new_mtime.tv_nsec != data->crosshair_mtime.tv_nsec) {
                                                KROSSHAIR_LOG("[KROSSHAIR] mtime changed (%ld.%ld -> %ld.%ld), reloading\n",
                                                       data->crosshair_mtime.tv_sec,
                                                       data->crosshair_mtime.tv_nsec,
                                                       new_mtime.tv_sec,
                                                       new_mtime.tv_nsec);
                                                needs_reload = 1;
                                        }
                                }
                        }
                }

                if (!needs_reload) {
                        free(crosshair_path);
                        return;
                }

                KROSSHAIR_LOG("[KROSSHAIR] shutting down old crosshair image\n");

                shutdown_krosshair_image(data);
                data->crosshair_uploaded = 0;
                free(data->crosshair_path);
                data->crosshair_path = NULL;
        }

        int tex_width;
        int tex_height;
        int tex_channels;
        VkDeviceSize image_size;
        stbi_uc* pixels;

        if (using_file) {
                pixels = stbi_load(crosshair_path, &tex_width, &tex_height,
                                   &tex_channels, STBI_rgb_alpha);
                image_size = tex_width * tex_height * 4;

                if (!pixels) {
                        printf(
                            "[KROSSHAIR_ERROR] failed to load crosshair "
                            "image.\n");
                        free(crosshair_path);
                        return;
                }

                data->descriptor_set = create_image_with_desc(
                    data, tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB,
                    &data->crosshair_image, &data->crosshair_mem,
                    &data->crosshair_image_view);

                upload_image_data(
                    device_data, cmd_buffer, pixels, image_size, tex_width,
                    tex_height, &data->crosshair_upload_buffer,
                    &data->crosshair_upload_buffer_mem, data->crosshair_image);
                stbi_image_free(pixels);

                data->crosshair_path = crosshair_path;
                get_file_mtime(crosshair_path, &data->crosshair_mtime);
                KROSSHAIR_LOG("[KROSSHAIR] loaded crosshair from: %s (mtime %ld.%ld)\n",
                              crosshair_path,
                              data->crosshair_mtime.tv_sec,
                              data->crosshair_mtime.tv_nsec);
        } else {
                free(crosshair_path);
                tex_width            = default_crosshair_width;
                tex_height           = default_crosshair_height;
                image_size           = tex_width * tex_height * 4;

                data->descriptor_set = create_image_with_desc(
                    data, tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB,
                    &data->crosshair_image, &data->crosshair_mem,
                    &data->crosshair_image_view);

                upload_image_data(
                    device_data, cmd_buffer, (stbi_uc*)default_crosshair_data,
                    image_size, tex_width, tex_height,
                    &data->crosshair_upload_buffer,
                    &data->crosshair_upload_buffer_mem, data->crosshair_image);
        }

        float scale           = 1;
        const char* scale_str = getenv("KROSSHAIR_SCALE");
        if (scale_str) {
                char* end = NULL;
                for (float scale_cvt = strtof(scale_str, &end); scale_str != end;
                     scale_cvt = strtof(scale_str, &end)) {
                        printf("got scale: %f\n", scale_cvt);
                        scale = scale_cvt; 
                        scale_str = end;
                        if (errno == ERANGE) {
                                printf("KROSSHAIR_SCALE invalid.\n");
                                scale = 1;
                        }
                }
        }

        printf("scale: %f\n", scale);

        setup_vertices((float)data->width, (float)data->height,
                       (float)tex_height, (float)tex_width, scale);

        data->crosshair_uploaded = 1;
}

/* allocated a krosshair_draw_t instance that must be free'd at the end of its
 * lifetime
 * */
static void create_draw(swapchain_data_t* data)
{
        device_data_t* device_data = data->device_data;
        krosshair_draw_t* draw     = data->draw;

        if (draw) {
                if (device_data->vtable.GetFenceStatus(
                        device_data->device, draw->fence) == VK_SUCCESS) {
                        VK_CHECK(device_data->vtable.ResetFences(
                            device_data->device, 1, &draw->fence));
                        return;
                }
        }

        VkSemaphoreCreateInfo sem_info = {};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        draw           = malloc(sizeof(*draw));
        memset(draw, 0, sizeof(*draw));

        VkCommandBufferAllocateInfo cmd_buffer_info = {};
        cmd_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buffer_info.commandPool        = data->cmd_pool;
        cmd_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buffer_info.commandBufferCount = 1;
        VK_CHECK(device_data->vtable.AllocateCommandBuffers(
            device_data->device, &cmd_buffer_info, &draw->cmd_buffer));
        VK_CHECK(device_data->set_device_loader_data(device_data->device,
                                                     draw->cmd_buffer));

        VkFenceCreateInfo fence_info = {};
        fence_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(device_data->vtable.CreateFence(
            device_data->device, &fence_info, NULL, &draw->fence));

        VK_CHECK(device_data->vtable.CreateSemaphore(
            device_data->device, &sem_info, NULL, &draw->semaphore));
        VK_CHECK(device_data->vtable.CreateSemaphore(
            device_data->device, &sem_info, NULL,
            &draw->crossengine_semaphore));

        data->draw = draw;
}

static krosshair_draw_t* render_swapchain_display(
    swapchain_data_t* data, queue_data_t* present_queue,
    const VkSemaphore* wait_semaphores, unsigned n_wait_semaphores,
    unsigned image_index)
{
        device_data_t* device_data = data->device_data;

        create_draw(data);
        krosshair_draw_t* draw = data->draw;
        device_data->vtable.ResetCommandBuffer(draw->cmd_buffer, 0);

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass  = data->render_pass;
        render_pass_info.framebuffer = data->framebuffers[image_index];
        render_pass_info.renderArea.extent.width   = data->width;
        render_pass_info.renderArea.extent.height  = data->height;

        VkCommandBufferBeginInfo buffer_begin_info = {};
        buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        device_data->vtable.BeginCommandBuffer(draw->cmd_buffer,
                                               &buffer_begin_info);
        ensure_swapchain_crosshair(data, draw->cmd_buffer);

        VkImageMemoryBarrier imb = {};
        imb.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imb.pNext                = NULL;
        imb.srcAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imb.dstAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imb.oldLayout            = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imb.newLayout            = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imb.image                = data->images[image_index];
        imb.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imb.subresourceRange.baseMipLevel   = 0;
        imb.subresourceRange.levelCount     = 1;
        imb.subresourceRange.baseArrayLayer = 0;
        imb.subresourceRange.layerCount     = 1;
        imb.srcQueueFamilyIndex             = present_queue->family_index;
        imb.dstQueueFamilyIndex = device_data->graphic_queue->family_index;
        device_data->vtable.CmdPipelineBarrier(
            draw->cmd_buffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 1, &imb);

        device_data->vtable.CmdBeginRenderPass(
            draw->cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        // create vertex & index buffers
        // since our contents aren't changing at runtime, might as well do
        // creation and upload only once somewhere else? upload vertex & index
        // data bind vertex & index buffers

        size_t vertex_size = sizeof(vertices);
        size_t index_size  = sizeof(indices);
        if (draw->vertex_buffer_size < vertex_size) {
                create_or_resize_buffer(device_data, &draw->vertex_buffer,
                                        &draw->vertex_buffer_mem,
                                        &draw->vertex_buffer_size, vertex_size,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }
        if (draw->index_buffer_size < index_size) {
                create_or_resize_buffer(device_data, &draw->index_buffer,
                                        &draw->index_buffer_mem,
                                        &draw->index_buffer_size, index_size,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }

        // upload vertex buffer
        void* vtx_dst = NULL;
        VK_CHECK(device_data->vtable.MapMemory(
            device_data->device, draw->vertex_buffer_mem, 0,
            draw->vertex_buffer_size, 0, &vtx_dst));
        memcpy(vtx_dst, vertices, (size_t)draw->vertex_buffer_size);

        VkMappedMemoryRange vtx_range = {};
        vtx_range.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        vtx_range.memory              = draw->vertex_buffer_mem;
        vtx_range.size                = VK_WHOLE_SIZE;
        VK_CHECK(device_data->vtable.FlushMappedMemoryRanges(
            device_data->device, 1, &vtx_range));
        device_data->vtable.UnmapMemory(device_data->device,
                                        draw->vertex_buffer_mem);

        // upload index buffer
        void* idx_dst = NULL;
        VK_CHECK(device_data->vtable.MapMemory(
            device_data->device, draw->index_buffer_mem, 0,
            draw->index_buffer_size, 0, &idx_dst));
        memcpy(idx_dst, indices, (size_t)draw->index_buffer_size);

        VkMappedMemoryRange idx_range = {};
        idx_range.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        idx_range.memory              = draw->index_buffer_mem;
        idx_range.size                = VK_WHOLE_SIZE;
        VK_CHECK(device_data->vtable.FlushMappedMemoryRanges(
            device_data->device, 1, &idx_range));
        device_data->vtable.UnmapMemory(device_data->device,
                                        draw->index_buffer_mem);

        device_data->vtable.CmdBindPipeline(
            draw->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pipeline);

        VkDeviceSize offsets[1] = {0};
        device_data->vtable.CmdBindVertexBuffers(draw->cmd_buffer, 0, 1,
                                                 &draw->vertex_buffer, offsets);
        device_data->vtable.CmdBindIndexBuffer(
            draw->cmd_buffer, draw->index_buffer, 0, VK_INDEX_TYPE_UINT16);

        VkViewport viewport = {};
        viewport.x          = 0;
        viewport.y          = 0;
        viewport.width      = data->width;
        viewport.height     = data->height;
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        device_data->vtable.CmdSetViewport(draw->cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor      = {};
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = data->width;
        scissor.extent.height = data->height;
        device_data->vtable.CmdSetScissor(draw->cmd_buffer, 0, 1, &scissor);

        device_data->vtable.CmdBindDescriptorSets(
            draw->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            data->pipeline_layout, 0, 1, &data->descriptor_set, 0, NULL);
        device_data->vtable.CmdDrawIndexed(draw->cmd_buffer, sizeof(indices), 1,
                                           0, 0, 0);

        device_data->vtable.CmdEndRenderPass(draw->cmd_buffer);

        /*
         * transfer the image back to the present queue family
         * image layout was already changed to present by the render pass
         */
        if (device_data->graphic_queue->family_index !=
            present_queue->family_index) {
                imb.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imb.pNext         = NULL;
                imb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                imb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                imb.oldLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                imb.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                imb.image         = data->images[image_index];
                imb.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                imb.subresourceRange.baseMipLevel   = 0;
                imb.subresourceRange.levelCount     = 1;
                imb.subresourceRange.baseArrayLayer = 0;
                imb.subresourceRange.layerCount     = 1;
                imb.srcQueueFamilyIndex =
                    device_data->graphic_queue->family_index;
                imb.dstQueueFamilyIndex = present_queue->family_index;
                device_data->vtable.CmdPipelineBarrier(
                    draw->cmd_buffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 1,
                    &imb);
        }

        device_data->vtable.EndCommandBuffer(draw->cmd_buffer);

        /* when presenting on a different queue than where we're drawing the
         * crosshair *AND* when the application does not provide a semaphore to
         * vkQueuePresent, insert our own cross-engine synchronization
         * semaphore.
         * */
        if (n_wait_semaphores == 0 &&
            device_data->graphic_queue->queue != present_queue->queue) {
                VkPipelineStageFlags stages_wait =
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                VkSubmitInfo submit_info       = {};
                submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.commandBufferCount = 0;
                submit_info.pWaitDstStageMask  = &stages_wait;
                submit_info.waitSemaphoreCount = 0;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores    = &draw->crossengine_semaphore;

                device_data->vtable.QueueSubmit(present_queue->queue, 1,
                                                &submit_info, VK_NULL_HANDLE);

                submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.commandBufferCount = 1;
                submit_info.pWaitDstStageMask  = &stages_wait;
                submit_info.pCommandBuffers    = &draw->cmd_buffer;
                submit_info.waitSemaphoreCount = 1;
                submit_info.pWaitSemaphores    = &draw->crossengine_semaphore;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores    = &draw->semaphore;

                device_data->vtable.QueueSubmit(
                    device_data->graphic_queue->queue, 1, &submit_info,
                    draw->fence);
        } else {
                /* wait in the fragment stage until the swapchain image is ready
                 */
                VkPipelineStageFlags* stages_wait =
                    malloc(sizeof(*stages_wait) * n_wait_semaphores);
                memset(stages_wait, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       sizeof(*stages_wait) * n_wait_semaphores);

                VkSubmitInfo submit_info       = {};
                submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.commandBufferCount = 1;
                submit_info.pCommandBuffers    = &draw->cmd_buffer;
                submit_info.pWaitDstStageMask  = stages_wait;
                submit_info.waitSemaphoreCount = n_wait_semaphores;
                submit_info.pWaitSemaphores    = wait_semaphores;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores    = &draw->semaphore;

                device_data->vtable.QueueSubmit(
                    device_data->graphic_queue->queue, 1, &submit_info,
                    draw->fence);
                free(stages_wait);
        }

        return draw;
}

static void setup_swapchain_data_pipeline(swapchain_data_t* data)
{
        device_data_t* device_data = data->device_data;
        VkShaderModule vert_module;
        VkShaderModule frag_module;

        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = sizeof(vert_spv);
        vert_info.pCode    = (const uint32_t*)vert_spv;
        VK_CHECK(device_data->vtable.CreateShaderModule(
            device_data->device, &vert_info, NULL, &vert_module));

        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = sizeof(frag_spv);
        frag_info.pCode    = (const uint32_t*)frag_spv;
        VK_CHECK(device_data->vtable.CreateShaderModule(
            device_data->device, &frag_info, NULL, &frag_module));

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter     = VK_FILTER_LINEAR;
        sampler_info.minFilter     = VK_FILTER_LINEAR;
        sampler_info.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod        = -1000;
        sampler_info.maxLod        = 1000;
        sampler_info.maxAnisotropy = 1;
        VK_CHECK(device_data->vtable.CreateSampler(device_data->device,
                                                   &sampler_info, NULL,
                                                   &data->crosshair_sampler));

        VkDescriptorPoolSize sampler_pool_size = {};
        sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_pool_size.descriptorCount         = 1;

        VkDescriptorPoolCreateInfo desc_pool_info = {};
        desc_pool_info.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desc_pool_info.maxSets = 1;
        desc_pool_info.poolSizeCount = 1;
        desc_pool_info.pPoolSizes    = &sampler_pool_size;
        VK_CHECK(device_data->vtable.CreateDescriptorPool(
            device_data->device, &desc_pool_info, NULL,
            &data->descriptor_pool));

        VkSampler sampler                    = data->crosshair_sampler;
        VkDescriptorSetLayoutBinding binding = {};
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = &sampler;

        VkDescriptorSetLayoutCreateInfo set_layout_info = {};
        set_layout_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_layout_info.bindingCount = 1;
        set_layout_info.pBindings    = &binding;
        VK_CHECK(device_data->vtable.CreateDescriptorSetLayout(
            device_data->device, &set_layout_info, NULL,
            &data->descriptor_layout));

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount         = 1;
        layout_info.pSetLayouts            = &data->descriptor_layout;
        layout_info.pushConstantRangeCount = 0;
        VK_CHECK(device_data->vtable.CreatePipelineLayout(
            device_data->device, &layout_info, NULL, &data->pipeline_layout));

        VkPipelineShaderStageCreateInfo stage[2] = {};
        stage[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = vert_module;
        stage[0].pName  = "main";
        stage[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = frag_module;
        stage[1].pName  = "main";

        VkVertexInputBindingDescription binding_desc = {};
        binding_desc.binding                         = 0;
        binding_desc.stride                          = sizeof(vertex_t);
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attribute_desc[2] = {};
        attribute_desc[0].location                          = 0;
        attribute_desc[0].binding                           = 0;
        attribute_desc[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[0].offset   = offsetof(vertex_t, pos);
        attribute_desc[1].location = 1;
        attribute_desc[1].binding  = 0;
        attribute_desc[1].format   = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[1].offset   = offsetof(vertex_t, tex_pos);

        VkPipelineVertexInputStateCreateInfo vertex_info = {};
        vertex_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount             = 1;
        vertex_info.pVertexBindingDescriptions                = &binding_desc;
        vertex_info.vertexAttributeDescriptionCount           = 2;
        vertex_info.pVertexAttributeDescriptions              = attribute_desc;

        VkPipelineInputAssemblyStateCreateInfo input_asm_info = {};
        input_asm_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_asm_info.primitiveRestartEnable           = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_info = {};
        viewport_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.viewportCount                        = 1;
        viewport_info.scissorCount                         = 1;

        VkPipelineRasterizationStateCreateInfo raster_info = {};
        raster_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_info.polygonMode                      = VK_POLYGON_MODE_FILL;
        raster_info.cullMode                         = VK_CULL_MODE_BACK_BIT;
        raster_info.frontFace                        = VK_FRONT_FACE_CLOCKWISE;
        raster_info.lineWidth                        = 1.0f;
        raster_info.rasterizerDiscardEnable          = VK_FALSE;
        raster_info.depthClampEnable                 = VK_FALSE;
        raster_info.depthBiasEnable                  = VK_FALSE;
        raster_info.depthBiasConstantFactor          = 0.0f;
        raster_info.depthBiasClamp                   = 0.0f;
        raster_info.depthBiasSlopeFactor             = 0.0f;

        VkPipelineMultisampleStateCreateInfo ms_info = {};
        ms_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_attachment = {};
        color_attachment.blendEnable                         = VK_TRUE;
        color_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_attachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_attachment.colorBlendOp                    = VK_BLEND_OP_ADD;
        color_attachment.srcAlphaBlendFactor             = VK_BLEND_FACTOR_ONE;
        color_attachment.dstAlphaBlendFactor             = VK_BLEND_FACTOR_ZERO;
        color_attachment.alphaBlendOp                    = VK_BLEND_OP_ADD;

        VkPipelineDepthStencilStateCreateInfo depth_info = {};
        depth_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendStateCreateInfo blend_info = {};
        blend_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_info.attachmentCount       = 1;
        blend_info.pAttachments          = &color_attachment;

        VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount            = 2;
        dynamic_state.pDynamicStates               = dynamic_states;

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.flags = 0;
        pipeline_info.stageCount          = 2;
        pipeline_info.pStages             = stage;
        pipeline_info.pVertexInputState   = &vertex_info;
        pipeline_info.pInputAssemblyState = &input_asm_info;
        pipeline_info.pViewportState      = &viewport_info;
        pipeline_info.pRasterizationState = &raster_info;
        pipeline_info.pMultisampleState   = &ms_info;
        pipeline_info.pDepthStencilState  = &depth_info;
        pipeline_info.pColorBlendState    = &blend_info;
        pipeline_info.pDynamicState       = &dynamic_state;
        pipeline_info.layout              = data->pipeline_layout;
        pipeline_info.renderPass          = data->render_pass;
        VK_CHECK(device_data->vtable.CreateGraphicsPipelines(
            device_data->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
            &data->pipeline));

        device_data->vtable.DestroyShaderModule(device_data->device,
                                                vert_module, NULL);
        device_data->vtable.DestroyShaderModule(device_data->device,
                                                frag_module, NULL);
}

static void setup_swapchain_data(swapchain_data_t* data,
                                 const VkSwapchainCreateInfoKHR* pCreateInfo)
{
        device_data_t* device_data = data->device_data;
        data->width                = pCreateInfo->imageExtent.width;
        data->height               = pCreateInfo->imageExtent.height;
        data->format               = pCreateInfo->imageFormat;

        VkAttachmentDescription attachment_desc = {};
        attachment_desc.format                  = pCreateInfo->imageFormat;
        attachment_desc.samples                 = VK_SAMPLE_COUNT_1_BIT;
        attachment_desc.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment_desc.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_desc.initialLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment            = 0;
        color_attachment.layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_attachment;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass          = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments    = &attachment_desc;
        render_pass_info.subpassCount    = 1;
        render_pass_info.pSubpasses      = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies   = &dependency;
        VK_CHECK(device_data->vtable.CreateRenderPass(
            device_data->device, &render_pass_info, NULL, &data->render_pass));

        setup_swapchain_data_pipeline(data);

        uint32_t n_images = 0;
        VK_CHECK(device_data->vtable.GetSwapchainImagesKHR(
            device_data->device, data->swapchain, &n_images, NULL));

        VK_CHECK(device_data->vtable.GetSwapchainImagesKHR(
            device_data->device, data->swapchain, &n_images, data->images));

        VkImageViewCreateInfo view_info = {};
        view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format   = pCreateInfo->imageFormat;
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;
        for (size_t i = 0; i < n_images; i++) {
                view_info.image = data->images[i];
                VK_CHECK(device_data->vtable.CreateImageView(
                    device_data->device, &view_info, NULL,
                    &data->image_views[i]));
        }

        VkImageView attachment;
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass      = data->render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments    = &attachment;
        fb_info.width           = data->width;
        fb_info.height          = data->height;
        fb_info.layers          = 1;
        for (size_t i = 0; i < n_images; i++) {
                attachment = data->image_views[i];
                VK_CHECK(device_data->vtable.CreateFramebuffer(
                    device_data->device, &fb_info, NULL,
                    &data->framebuffers[i]));
        }

        VkCommandPoolCreateInfo cmd_buffer_pool_info = {};
        cmd_buffer_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_buffer_pool_info.flags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmd_buffer_pool_info.queueFamilyIndex =
            device_data->graphic_queue->family_index;

        VK_CHECK(device_data->vtable.CreateCommandPool(
            device_data->device, &cmd_buffer_pool_info, NULL, &data->cmd_pool));
}

static void instance_data_map_physical_devices(instance_data_t* instance_data,
                                               int map)
{
        uint32_t physical_device_count = 0;
        instance_data->vtable.EnumeratePhysicalDevices(
            instance_data->instance, &physical_device_count, NULL);

        VkPhysicalDevice* physical_devices =
            malloc(sizeof(VkPhysicalDevice) * physical_device_count);
        instance_data->vtable.EnumeratePhysicalDevices(
            instance_data->instance, &physical_device_count, physical_devices);

        for (uint32_t i = 0; i < physical_device_count; i++) {
                if (map) {
                        printf("[*] mapping physical_devices[%d] obj: %lu %p\n",
                               i, HKEY(physical_devices[i]), instance_data);
                        char* fmt_phys_device;
                        asprintf(&fmt_phys_device, "physical_devices[%d]", i);
                        map_object(HKEY(physical_devices[i]), instance_data,
                                   fmt_phys_device);
                } else {
                        unmap_object(HKEY(physical_devices[i]));
                }
        }

        free(physical_devices);
}

static swapchain_data_t* new_swapchain_data(VkSwapchainKHR swapchain,
                                            device_data_t* device_data)
{
        swapchain_data_t* swapchain_data = malloc(sizeof(*swapchain_data));
        memset(swapchain_data, 0, sizeof(*swapchain_data));
        swapchain_data->device_data = device_data;
        swapchain_data->swapchain   = swapchain;
        printf("[*] mapping data->swapchain obj: %lu %p\n",
               HKEY(swapchain_data->swapchain), swapchain_data);
        map_object(HKEY(swapchain_data->swapchain), swapchain_data,
                   "swapchain_data->swapchain");
        return swapchain_data;
}

static VkResult overlay_CreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
        VkSwapchainCreateInfoKHR create_info = *pCreateInfo;
        create_info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        device_data_t* device_data = FIND_OBJ(device_data_t, device);

        VkResult result            = device_data->vtable.CreateSwapchainKHR(
            device, &create_info, pAllocator, pSwapchain);
        if (result != VK_SUCCESS) return result;

        swapchain_data_t* swapchain_data =
            new_swapchain_data(*pSwapchain, device_data);

        setup_swapchain_data(swapchain_data, pCreateInfo);

        return result;
}

static void overlay_DestroySwapchainKHR(VkDevice device,
                                        VkSwapchainKHR* swapchain,
                                        VkAllocationCallbacks* pAllocator)
{
        device_data_t* device_data = FIND_OBJ(device_data_t, device);
        swapchain_data_t* data     = FIND_OBJ(swapchain_data_t, swapchain);

        for (size_t i = 0; i < data->n_images; i++) {
                device_data->vtable.DestroyFramebuffer(
                    device_data->device, data->framebuffers[i], NULL);
                device_data->vtable.DestroyImageView(
                    device_data->device, data->image_views[i], NULL);
        }

        free(data->draw);
        unmap_object(HKEY(data->swapchain));
        device_data->vtable.DestroySwapchainKHR(device_data->device,
                                                data->swapchain, NULL);
        free(data);
}

static VkResult overlay_CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkInstance* pInstance)
{
        VkLayerInstanceCreateInfo* chain_info =
            get_instance_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);
        assert(chain_info->u.pLayerInfo);
        PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr =
            chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        PFN_vkCreateInstance fpCreateInstance =
            (PFN_vkCreateInstance)fpGetInstanceProcAddr(NULL,
                                                        "vkCreateInstance");
        if (!fpCreateInstance) {
                return VK_ERROR_INITIALIZATION_FAILED;
        }

        chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

        VkResult result = fpCreateInstance(pCreateInfo, pAllocator, pInstance);
        if (result != VK_SUCCESS) return result;

        instance_data_t* instance_data = new_instance_data(*pInstance);
        vk_load_instance_commands(instance_data->instance,
                                  fpGetInstanceProcAddr,
                                  &instance_data->vtable);
        instance_data_map_physical_devices(instance_data, 1);

        return result;
}

static queue_data_t* new_queue_data(VkQueue queue,
                                    const VkQueueFamilyProperties* family_props,
                                    uint32_t family_index,
                                    device_data_t* device_data)
{
        queue_data_t* queue_data = malloc(sizeof(*queue_data));
        memset(queue_data, 0, sizeof(*queue_data));
        queue_data->device       = device_data;
        queue_data->queue        = queue;
        queue_data->flags        = family_props->queueFlags;
        queue_data->family_index = family_index;
        printf("[*] mapping data->queue obj: %lu %p\n", HKEY(queue_data->queue),
               queue_data);
        map_object(HKEY(queue_data->queue), queue_data, "queue_data->queue");

        if (queue_data->flags & VK_QUEUE_GRAPHICS_BIT) {
                device_data->graphic_queue = queue_data;
        }

        return queue_data;
}

static void device_map_queues(device_data_t* data,
                              const VkDeviceCreateInfo* pCreateInfo)
{
        uint32_t n_queues = 0;
        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
                n_queues += pCreateInfo->pQueueCreateInfos[i].queueCount;
        }
        data->queue_count              = n_queues;

        instance_data_t* instance_data = data->instance;
        uint32_t n_family_props;
        instance_data->vtable.GetPhysicalDeviceQueueFamilyProperties(
            data->physical_device, &n_family_props, NULL);
        VkQueueFamilyProperties* family_props =
            malloc(sizeof(*family_props) * n_family_props);
        instance_data->vtable.GetPhysicalDeviceQueueFamilyProperties(
            data->physical_device, &n_family_props, family_props);

        uint32_t queue_index = 0;
        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
                for (uint32_t j = 0;
                     j < pCreateInfo->pQueueCreateInfos[i].queueCount; j++) {
                        VkQueue queue;
                        data->vtable.GetDeviceQueue(
                            data->device,
                            pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex,
                            j, &queue);

                        VK_CHECK(
                            data->set_device_loader_data(data->device, queue));

                        data->queues[queue_index++] = new_queue_data(
                            queue,
                            &family_props[pCreateInfo->pQueueCreateInfos[i]
                                              .queueFamilyIndex],
                            pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex,
                            data);
                }
        }

        free(family_props);
}

static krosshair_draw_t* before_present(swapchain_data_t* swapchain_data,
                                        queue_data_t* present_queue,
                                        const VkSemaphore* wait_semaphores,
                                        unsigned n_wait_semaphores,
                                        unsigned image_index)
{
        krosshair_draw_t* draw = NULL;

        draw = render_swapchain_display(swapchain_data, present_queue,
                                        wait_semaphores, n_wait_semaphores,
                                        image_index);

        return draw;
}

static VkLayerDeviceCreateInfo* get_device_chain_info(
    const VkDeviceCreateInfo* pCreateInfo, VkLayerFunction func)
{
        vk_foreach_struct(item, pCreateInfo->pNext)
        {
                if (item->sType ==
                        VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
                    ((VkLayerDeviceCreateInfo*)item)->function == func)
                        return (VkLayerDeviceCreateInfo*)item;
        }
        return NULL;
}

static VkResult overlay_CreateDevice(VkPhysicalDevice physical_device,
                                     const VkDeviceCreateInfo* pCreateInfo,
                                     const VkAllocationCallbacks* pAllocator,
                                     VkDevice* pDevice)
{
        instance_data_t* instance_data =
            FIND_OBJ(instance_data_t, physical_device);
        VkLayerDeviceCreateInfo* chain_info =
            get_device_chain_info(pCreateInfo, VK_LAYER_LINK_INFO);

        assert(chain_info->u.pLayerInfo);
        PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr =
            chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr fpGetDeviceProcAddr =
            chain_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
        PFN_vkCreateDevice fpCreateDevice =
            (PFN_vkCreateDevice)fpGetInstanceProcAddr(NULL, "vkCreateDevice");
        if (fpCreateDevice == NULL) {
                return VK_ERROR_INITIALIZATION_FAILED;
        }

        chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

        /*
         * this whole driver stuff doesn't seem necessary?
         * */
        // const char** enabled_extensions = malloc(
        //     sizeof(*enabled_extensions) *
        //     pCreateInfo->enabledExtensionCount);
        // for (size_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
        //         enabled_extensions[i] =
        //         pCreateInfo->ppEnabledExtensionNames[i];
        // };
        //
        // uint32_t extension_count;
        // instance_data->vtable.EnumerateDeviceExtensionProperties(
        //     physical_device, NULL, &extension_count, NULL);
        //
        // VkExtensionProperties* available_extensions =
        //     malloc(sizeof(*available_extensions) * extension_count);
        // instance_data->vtable.EnumerateDeviceExtensionProperties(
        //     physical_device, NULL, &extension_count, available_extensions);
        //
        // uint32_t found_extensions = 0;
        // // TODO: this works?
        // for (size_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
        //         for (size_t j = 0; j < pCreateInfo->enabledExtensionCount;
        //              j++) {
        //                 printf("available_extensions[%lu]: %s\n", i,
        //                        available_extensions[i].extensionName);
        //                 printf("enabled_extensions[%lu]: %s\n", i,
        //                        enabled_extensions[j]);
        //                 if (!strcmp(available_extensions[i].extensionName,
        //                             enabled_extensions[j])) {
        //                         found_extensions = found_extensions + 1;
        //                 }
        //         }
        // }
        // free(available_extensions);
        // free(enabled_extensions);
        // if (found_extensions != pCreateInfo->enabledExtensionCount) {
        //         printf(
        //             "[KROSSHAIR_ERROR] extensions don't match. "
        //             "found_extensions: %d, enabled_extensions: %d\n",
        //             found_extensions, pCreateInfo->enabledExtensionCount);
        // }

        VkResult result =
            fpCreateDevice(physical_device, pCreateInfo, pAllocator, pDevice);
        if (result != VK_SUCCESS) {
                return result;
        }

        device_data_t* device_data   = new_device_data(*pDevice, instance_data);
        device_data->physical_device = physical_device;
        vk_load_device_commands(*pDevice, fpGetDeviceProcAddr,
                                &device_data->vtable);

        instance_data->vtable.GetPhysicalDeviceProperties(
            device_data->physical_device, &device_data->properties);

        VkLayerDeviceCreateInfo* load_data_info =
            get_device_chain_info(pCreateInfo, VK_LOADER_DATA_CALLBACK);
        device_data->set_device_loader_data =
            load_data_info->u.pfnSetDeviceLoaderData;

        // driver_properties.sType =
        //     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        // driver_properties.pNext = NULL;

        device_map_queues(device_data, pCreateInfo);

        return result;
}

static VkResult overlay_QueuePresentKHR(VkQueue queue,
                                        const VkPresentInfoKHR* pPresentInfo)
{
        queue_data_t* queue_data = FIND_OBJ(queue_data_t, queue);

        VkResult result          = VK_SUCCESS;
        for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++) {
                VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
                swapchain_data_t* swapchain_data =
                    FIND_OBJ(swapchain_data_t, swapchain);

                uint32_t image_index          = pPresentInfo->pImageIndices[i];

                VkPresentInfoKHR present_info = *pPresentInfo;
                present_info.swapchainCount   = 1;
                present_info.pSwapchains      = &swapchain;
                present_info.pImageIndices    = &image_index;

                krosshair_draw_t* draw        = before_present(
                    swapchain_data, queue_data, pPresentInfo->pWaitSemaphores,
                    i == 0 ? pPresentInfo->waitSemaphoreCount : 0, image_index);

                if (draw) {
                        present_info.pWaitSemaphores    = &draw->semaphore;
                        present_info.waitSemaphoreCount = 1;
                }

                VkResult chain_result =
                    queue_data->device->vtable.QueuePresentKHR(queue,
                                                               &present_info);
                if (present_info.pResults) {
                        pPresentInfo->pResults[i] = chain_result;
                }
                if (chain_result != VK_SUCCESS && result == VK_SUCCESS) {
                        result = chain_result;
                }
        }
        return result;
}

static VkResult overlay_AllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer* pCommandBuffers)
{
        device_data_t* device_data = FIND_OBJ(device_data_t, device);
        VkResult result            = device_data->vtable.AllocateCommandBuffers(
            device, pAllocateInfo, pCommandBuffers);
        if (result != VK_SUCCESS) return result;

        for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++) {
                new_cmd_buffer_data(pCommandBuffers[i], pAllocateInfo->level,
                                    device_data);
        }

        return result;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
overlay_GetInstanceProcAddr(VkInstance instance, const char* func_name);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
overlay_GetDeviceProcAddr(VkDevice device, const char* func_name);

typedef struct name_to_funcptr {
        const char* name;
        void* func;
} name_to_funcptr_t;

name_to_funcptr_t name_to_funcptr_map[] = {
    { "vkGetInstanceProcAddr",    (void*)overlay_GetInstanceProcAddr},
    {   "vkGetDeviceProcAddr",      (void*)overlay_GetDeviceProcAddr},
    {      "vkCreateInstance",         (void*)overlay_CreateInstance},
    {     "vkQueuePresentKHR",        (void*)overlay_QueuePresentKHR},
    {  "vkCreateSwapchainKHR",     (void*)overlay_CreateSwapchainKHR},
    { "vkDestroySwapchainKHR",    (void*)overlay_DestroySwapchainKHR},
    {        "vkCreateDevice",           (void*)overlay_CreateDevice},
    {"AllocateCommandBuffers", (void*)overlay_AllocateCommandBuffers}
};

size_t name_to_funcptr_map_count =
    (sizeof(name_to_funcptr_map) / sizeof(name_to_funcptr_map[0]));

static void* find_ptr(const char* name)
{
        for (uint32_t i = 0; i < name_to_funcptr_map_count; i++) {
                if (!strcmp(name, name_to_funcptr_map[i].name)) {
                        return name_to_funcptr_map[i].func;
                }
        }
        return NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
overlay_GetInstanceProcAddr(VkInstance instance, const char* func_name)
{
        void* ptr = find_ptr(func_name);
        // printf("found function %s at %p\n", func_name, ptr);
        if (ptr) return (PFN_vkVoidFunction)ptr;
        if (instance == NULL) return NULL;

        instance_data_t* instance_data = FIND_OBJ(instance_data_t, instance);
        if (instance_data->vtable.GetInstanceProcAddr == NULL) return NULL;

        return instance_data->vtable.GetInstanceProcAddr(instance, func_name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
overlay_GetDeviceProcAddr(VkDevice device, const char* func_name)
{
        void* ptr = find_ptr(func_name);
        if (ptr) return (PFN_vkVoidFunction)ptr;
        if (device == NULL) return NULL;

        device_data_t* device_data = FIND_OBJ(device_data_t, device);
        if (device_data->vtable.GetDeviceProcAddr == NULL) return NULL;

        return device_data->vtable.GetDeviceProcAddr(device, func_name);
}
