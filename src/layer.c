#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <limits.h>
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

typedef struct static_vk_resources {
        VkSampler crosshair_sampler;
        VkDescriptorPool descriptor_pool;
        VkDescriptorSetLayout descriptor_layout;
        VkPipelineLayout pipeline_layout;
        VkPipeline pipeline;
        VkRenderPass render_pass;
        VkCommandPool cmd_pool;
        VkSemaphore crossengine_semaphore;
        VkSemaphore semaphore;
        VkFence fence;
        VkBuffer vertex_buffer;
        VkDeviceMemory vertex_buffer_mem;
        VkDeviceSize vertex_buffer_size;
        VkBuffer index_buffer;
        VkDeviceMemory index_buffer_mem;
        VkDeviceSize index_buffer_size;
        int initialized;
} static_vk_resources_t;

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
        static_vk_resources_t* vk_resources;
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

        int vertex_buffer_initialized;
        int index_buffer_initialized;
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

        /* crosshair texture dimensions (for vertex setup) */
        int crosshair_tex_width;

        /* GIF animation state */
        int gif_frame_count;
        int gif_frame_height;       /* height of a single frame */
        int* gif_delays;            /* delay per frame in ms */
        int gif_current_frame;
        struct timespec gif_last_frame_time;

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

/*
 * Set up the quad vertices for rendering.
 *   uv_top / uv_bottom: vertical UV range (0..1 for full texture,
 *   or a sub-range for atlas frame selection)
 */
static void setup_vertices_uv(float canvas_width, float canvas_height,
                               float tex_width, float tex_height, float scale,
                               float uv_top, float uv_bottom)
{
        float width_ndc  = ((tex_width * scale) / canvas_width);
        float height_ndc = ((tex_width * scale) / canvas_height);

        /* should fix even-length crosshairs */
        float pixel_offset_x =
            (fmod(canvas_width, 2.0f) == 0) ? 0.0f : (1.0f / canvas_width);
        float pixel_offset_y =
            (fmod(canvas_height, 2.0f) == 0) ? 0.0f : (1.0f / canvas_height);

        vertices[0].pos =
            (vec2_t){-width_ndc + pixel_offset_x, -height_ndc + pixel_offset_y};
        vertices[0].tex_pos = (vec2_t){0.0f, uv_top};
        vertices[1].pos =
            (vec2_t){width_ndc + pixel_offset_x, -height_ndc + pixel_offset_y};
        vertices[1].tex_pos = (vec2_t){1.0f, uv_top};
        vertices[2].pos =
            (vec2_t){width_ndc + pixel_offset_x, height_ndc + pixel_offset_y};
        vertices[2].tex_pos = (vec2_t){1.0f, uv_bottom};
        vertices[3].pos =
            (vec2_t){-width_ndc + pixel_offset_x, height_ndc + pixel_offset_y};
        vertices[3].tex_pos = (vec2_t){0.0f, uv_bottom};
}

static void setup_vertices(float canvas_width, float canvas_height,
                           float tex_width, float tex_height, float scale)
{
        setup_vertices_uv(canvas_width, canvas_height, tex_width, tex_height,
                          scale, 0.0f, 1.0f);
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
                /* use ResetDescriptorPool instead of FreeDescriptorSets
                 * because the pool was created without
                 * VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT */
                device_data->vtable.ResetDescriptorPool(device_data->device,
                                                        data->descriptor_pool, 0);
                data->descriptor_set = VK_NULL_HANDLE;
        }

        /* clean up GIF animation state */
        if (data->gif_delays) {
                free(data->gif_delays);
                data->gif_delays = NULL;
        }
        data->gif_frame_count    = 0;
        data->gif_frame_height   = 0;
        data->gif_current_frame  = 0;
        data->crosshair_tex_width = 0;
}

static void destroy_draw(swapchain_data_t* data, krosshair_draw_t* draw);

static void destroy_swapchain_data(swapchain_data_t* data)
{
        if (!data) return;

        device_data_t* device_data = data->device_data;

        if (data->draw) {
                destroy_draw(data, data->draw);
                data->draw = NULL;
        }

        shutdown_krosshair_image(data);

        if (data->descriptor_pool != VK_NULL_HANDLE) {
                device_data->vtable.DestroyDescriptorPool(device_data->device,
                                                           data->descriptor_pool, NULL);
                data->descriptor_pool = VK_NULL_HANDLE;
        }
        if (data->descriptor_layout != VK_NULL_HANDLE) {
                device_data->vtable.DestroyDescriptorSetLayout(device_data->device,
                                                               data->descriptor_layout, NULL);
                data->descriptor_layout = VK_NULL_HANDLE;
        }
        if (data->crosshair_sampler != VK_NULL_HANDLE) {
                device_data->vtable.DestroySampler(device_data->device,
                                                   data->crosshair_sampler, NULL);
                data->crosshair_sampler = VK_NULL_HANDLE;
        }
        if (data->pipeline != VK_NULL_HANDLE) {
                device_data->vtable.DestroyPipeline(device_data->device,
                                                   data->pipeline, NULL);
                data->pipeline = VK_NULL_HANDLE;
        }
        if (data->pipeline_layout != VK_NULL_HANDLE) {
                device_data->vtable.DestroyPipelineLayout(device_data->device,
                                                          data->pipeline_layout, NULL);
                data->pipeline_layout = VK_NULL_HANDLE;
        }
        if (data->render_pass != VK_NULL_HANDLE) {
                device_data->vtable.DestroyRenderPass(device_data->device,
                                                      data->render_pass, NULL);
                data->render_pass = VK_NULL_HANDLE;
        }
        if (data->cmd_pool != VK_NULL_HANDLE) {
                device_data->vtable.DestroyCommandPool(device_data->device,
                                                       data->cmd_pool, NULL);
                data->cmd_pool = VK_NULL_HANDLE;
        }

        for (uint32_t i = 0; i < data->n_images; i++) {
                if (data->framebuffers[i] != VK_NULL_HANDLE) {
                        device_data->vtable.DestroyFramebuffer(device_data->device,
                                                                data->framebuffers[i], NULL);
                        data->framebuffers[i] = VK_NULL_HANDLE;
                }
                if (data->image_views[i] != VK_NULL_HANDLE) {
                        device_data->vtable.DestroyImageView(device_data->device,
                                                            data->image_views[i], NULL);
                        data->image_views[i] = VK_NULL_HANDLE;
                }
        }

        if (data->crosshair_path) {
                free(data->crosshair_path);
                data->crosshair_path = NULL;
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

static void create_or_resize_upload_buffer(device_data_t* device_data,
                                          VkBuffer* upload_buffer,
                                          VkDeviceMemory* upload_buffer_mem,
                                          VkDeviceSize new_size)
{
        if (*upload_buffer != VK_NULL_HANDLE) {
                device_data->vtable.DestroyBuffer(device_data->device, *upload_buffer,
                                                  NULL);
                *upload_buffer = VK_NULL_HANDLE;
        }
        if (*upload_buffer_mem) {
                device_data->vtable.FreeMemory(device_data->device, *upload_buffer_mem,
                                               NULL);
                *upload_buffer_mem = VK_NULL_HANDLE;
        }

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size               = new_size;
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
}

static void upload_image_data(device_data_t* device_data,
                              VkCommandBuffer cmd_buffer, void* pixels,
                              VkDeviceSize upload_size, uint32_t width,
                              uint32_t height, VkBuffer* upload_buffer,
                              VkDeviceMemory* upload_buffer_mem, VkImage image)
{
        /* always create the upload buffer - each swapchain has its own */
        create_or_resize_upload_buffer(device_data, upload_buffer,
                                       upload_buffer_mem, upload_size);

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

// malloc's returned string, free later
// returns crosshair-maker default if installed and no explicit KROSSHAIR_IMG set
// tries current.apng first, then current.gif, then current.png
static char* get_crosshair_path(void)
{
        const char* explicit = getenv("KROSSHAIR_IMG");
        if (explicit)
                return get_crosshair_file(explicit);

        const char* home = getenv("HOME");
        if (!home)
                return NULL;

        char cm_path[4096];
        struct stat st;

        /* prefer animated formats first, then static PNG */
        snprintf(cm_path, sizeof(cm_path),
                 "%s/.config/crosshair-maker/projects/current.apng", home);
        if (stat(cm_path, &st) == 0)
                return strdup(cm_path);

        snprintf(cm_path, sizeof(cm_path),
                 "%s/.config/crosshair-maker/projects/current.apng", home);
        if (stat(cm_path, &st) == 0)
                return strdup(cm_path);

        snprintf(cm_path, sizeof(cm_path),
                 "%s/.config/crosshair-maker/projects/current.png", home);
        if (stat(cm_path, &st) == 0)
                return strdup(cm_path);

        return NULL;
}

/* ───────────────────── APNG loader ───────────────────── */

static const unsigned char png_signature[8] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
};

static uint32_t apng_read_be32(const unsigned char* p)
{
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t apng_read_be16(const unsigned char* p)
{
        return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static void apng_write_be32(unsigned char* p, uint32_t v)
{
        p[0] = (v >> 24) & 0xff;
        p[1] = (v >> 16) & 0xff;
        p[2] = (v >> 8) & 0xff;
        p[3] = v & 0xff;
}

/* CRC32 used by PNG chunk checksums */
static uint32_t apng_crc32_table[256];
static int apng_crc32_table_ready = 0;

static void apng_crc32_init(void)
{
        if (apng_crc32_table_ready) return;
        for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++)
                        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                apng_crc32_table[i] = c;
        }
        apng_crc32_table_ready = 1;
}

static uint32_t apng_crc32(const unsigned char* data, size_t len)
{
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++)
                crc = apng_crc32_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFF;
}

/* write a complete PNG chunk: length + type + data + crc */
static size_t apng_write_chunk(unsigned char* out, const char* type,
                               const unsigned char* data, uint32_t data_len)
{
        apng_write_be32(out, data_len);
        memcpy(out + 4, type, 4);
        if (data && data_len)
                memcpy(out + 8, data, data_len);
        /* CRC covers type + data */
        uint32_t crc = apng_crc32(out + 4, 4 + data_len);
        apng_write_be32(out + 8 + data_len, crc);
        return 12 + data_len; /* length(4) + type(4) + data + crc(4) */
}

typedef struct apng_frame_info {
        uint32_t width, height;
        uint32_t x_offset, y_offset;
        uint16_t delay_num, delay_den;
        uint8_t dispose_op, blend_op;
        /* raw compressed data (IDAT content or fdAT content minus seq) */
        unsigned char* idat_data;
        size_t idat_size;
        size_t idat_capacity;
} apng_frame_info_t;

static void apng_frame_append_idat(apng_frame_info_t* f,
                                    const unsigned char* data, size_t len)
{
        if (f->idat_size + len > f->idat_capacity) {
                size_t new_cap = (f->idat_capacity == 0) ? 4096 : f->idat_capacity;
                while (new_cap < f->idat_size + len)
                        new_cap *= 2;
                f->idat_data = realloc(f->idat_data, new_cap);
                f->idat_capacity = new_cap;
        }
        memcpy(f->idat_data + f->idat_size, data, len);
        f->idat_size += len;
}

/*
 * Build a minimal valid PNG in memory for a single APNG frame,
 * then decode it with stbi.  Returns RGBA pixels (caller must
 * stbi_image_free), or NULL on failure.
 *
 * ihdr_raw: the 13 bytes of the original IHDR *data* (no length/type/crc)
 */
static stbi_uc* apng_decode_frame(const apng_frame_info_t* f,
                                  const unsigned char* ihdr_raw,
                                  int* out_w, int* out_h)
{
        /* build a new IHDR with the frame's dimensions */
        unsigned char ihdr[13];
        memcpy(ihdr, ihdr_raw, 13);
        apng_write_be32(ihdr + 0, f->width);
        apng_write_be32(ihdr + 4, f->height);

        /* total PNG size: sig(8) + IHDR chunk(25) + IDAT chunk(12+data) + IEND(12) */
        size_t png_size = 8 + 25 + (12 + f->idat_size) + 12;
        unsigned char* png = malloc(png_size);
        if (!png) return NULL;

        size_t off = 0;
        memcpy(png, png_signature, 8);
        off += 8;
        off += apng_write_chunk(png + off, "IHDR", ihdr, 13);
        off += apng_write_chunk(png + off, "IDAT", f->idat_data, (uint32_t)f->idat_size);
        off += apng_write_chunk(png + off, "IEND", NULL, 0);

        int w, h, c;
        stbi_uc* pixels = stbi_load_from_memory(png, (int)off, &w, &h, &c, 4);
        free(png);

        if (pixels) {
                *out_w = w;
                *out_h = h;
        }
        return pixels;
}

/* alpha-composite src over dst (premultiply-aware, straight alpha) */
static void apng_blend_over(unsigned char* dst, const unsigned char* src,
                            uint32_t canvas_w, uint32_t canvas_h,
                            uint32_t x_off, uint32_t y_off,
                            uint32_t fw, uint32_t fh)
{
        for (uint32_t y = 0; y < fh; y++) {
                if (y + y_off >= canvas_h) break;
                for (uint32_t x = 0; x < fw; x++) {
                        if (x + x_off >= canvas_w) break;
                        size_t di = ((y + y_off) * canvas_w + (x + x_off)) * 4;
                        size_t si = (y * fw + x) * 4;
                        uint8_t sa = src[si + 3];
                        if (sa == 255) {
                                memcpy(dst + di, src + si, 4);
                        } else if (sa > 0) {
                                uint8_t da = dst[di + 3];
                                /* standard "over" blend */
                                for (int k = 0; k < 3; k++) {
                                        int sv = src[si + k] * sa;
                                        int dv = dst[di + k] * da * (255 - sa) / 255;
                                        int oa = sa + da * (255 - sa) / 255;
                                        dst[di + k] = oa ? (uint8_t)((sv + dv) / oa) : 0;
                                }
                                dst[di + 3] = (uint8_t)(sa + da * (255 - sa) / 255);
                        }
                }
        }
}

/* copy src frame pixels into canvas at offset (source replace, no blending) */
static void apng_blend_source(unsigned char* dst, const unsigned char* src,
                              uint32_t canvas_w, uint32_t canvas_h,
                              uint32_t x_off, uint32_t y_off,
                              uint32_t fw, uint32_t fh)
{
        for (uint32_t y = 0; y < fh; y++) {
                if (y + y_off >= canvas_h) break;
                size_t di = ((y + y_off) * canvas_w + x_off) * 4;
                size_t si = (y * fw) * 4;
                uint32_t copy_w = fw;
                if (x_off + fw > canvas_w) copy_w = canvas_w - x_off;
                memcpy(dst + di, src + si, copy_w * 4);
        }
}

/* clear a region to transparent black */
static void apng_clear_region(unsigned char* canvas, uint32_t canvas_w,
                              uint32_t canvas_h, uint32_t x, uint32_t y,
                              uint32_t w, uint32_t h)
{
        for (uint32_t row = y; row < y + h && row < canvas_h; row++) {
                size_t off = (row * canvas_w + x) * 4;
                uint32_t cw = w;
                if (x + w > canvas_w) cw = canvas_w - x;
                memset(canvas + off, 0, cw * 4);
        }
}

/*
 * Load an APNG file and return a vertical atlas of fully-composited frames,
 * identical in layout to what stbi_load_gif_from_memory produces.
 *
 * Returns RGBA pixel data (free with free()) or NULL on failure.
 * *out_width / *out_height are per-frame dimensions.
 * *out_frames is the frame count.
 * *out_delays is malloc'd array of per-frame delays in ms (free with free()).
 */
static unsigned char* load_apng(const unsigned char* file_data, size_t file_len,
                                int* out_width, int* out_height,
                                int* out_frames, int** out_delays)
{
        apng_crc32_init();

        if (file_len < 8 + 25 || memcmp(file_data, png_signature, 8) != 0) {
                KROSSHAIR_LOG("[APNG] not a PNG file\n");
                return NULL;
        }

        /* parse IHDR */
        size_t pos = 8;
        uint32_t chunk_len = apng_read_be32(file_data + pos);
        if (memcmp(file_data + pos + 4, "IHDR", 4) != 0 || chunk_len != 13) {
                KROSSHAIR_LOG("[APNG] missing IHDR\n");
                return NULL;
        }
        const unsigned char* ihdr_data = file_data + pos + 8;
        uint32_t canvas_w = apng_read_be32(ihdr_data);
        uint32_t canvas_h = apng_read_be32(ihdr_data + 4);

        /* first pass: find acTL and count frames */
        uint32_t num_frames = 0;
        int found_actl = 0;
        size_t scan = 8;
        while (scan + 12 <= file_len) {
                uint32_t clen = apng_read_be32(file_data + scan);
                const unsigned char* ctype = file_data + scan + 4;
                if (scan + 12 + clen > file_len) break;
                if (memcmp(ctype, "acTL", 4) == 0 && clen >= 8) {
                        num_frames = apng_read_be32(file_data + scan + 8);
                        found_actl = 1;
                }
                if (memcmp(ctype, "IEND", 4) == 0) break;
                scan += 12 + clen;
        }

        if (!found_actl || num_frames < 1) {
                KROSSHAIR_LOG("[APNG] no acTL chunk or 0 frames (not an APNG)\n");
                return NULL;
        }

        /* allocate frame info array */
        apng_frame_info_t* frames = calloc(num_frames, sizeof(apng_frame_info_t));
        if (!frames) return NULL;

        /* second pass: collect fcTL + IDAT/fdAT data per frame */
        int current_frame = -1; /* index into frames[] */
        int first_frame_is_default = 0; /* fcTL before first IDAT? */
        int seen_idat = 0;

        pos = 8;
        while (pos + 12 <= file_len) {
                uint32_t clen = apng_read_be32(file_data + pos);
                const unsigned char* ctype = file_data + pos + 4;
                const unsigned char* cdata = file_data + pos + 8;
                if (pos + 12 + clen > file_len) break;

                if (memcmp(ctype, "fcTL", 4) == 0 && clen >= 26) {
                        current_frame++;
                        if (current_frame >= (int)num_frames) break;

                        if (!seen_idat && current_frame == 0)
                                first_frame_is_default = 1;

                        apng_frame_info_t* f = &frames[current_frame];
                        f->width     = apng_read_be32(cdata + 4);
                        f->height    = apng_read_be32(cdata + 8);
                        f->x_offset  = apng_read_be32(cdata + 12);
                        f->y_offset  = apng_read_be32(cdata + 16);
                        f->delay_num = apng_read_be16(cdata + 20);
                        f->delay_den = apng_read_be16(cdata + 22);
                        f->dispose_op = cdata[24];
                        f->blend_op   = cdata[25];
                } else if (memcmp(ctype, "IDAT", 4) == 0) {
                        seen_idat = 1;
                        if (first_frame_is_default && current_frame == 0) {
                                apng_frame_append_idat(&frames[0], cdata, clen);
                        }
                } else if (memcmp(ctype, "fdAT", 4) == 0 && clen > 4) {
                        /* fdAT: first 4 bytes are sequence number, rest is
                         * IDAT-equivalent data */
                        if (current_frame >= 0 && current_frame < (int)num_frames) {
                                apng_frame_append_idat(&frames[current_frame],
                                                       cdata + 4, clen - 4);
                        }
                } else if (memcmp(ctype, "IEND", 4) == 0) {
                        break;
                }

                pos += 12 + clen;
        }

        /* the actual number of frames we collected may be less than num_frames
         * (e.g., truncated file) */
        int actual_frames = current_frame + 1;
        if (actual_frames < 1) {
                KROSSHAIR_LOG("[APNG] no frames found\n");
                for (uint32_t i = 0; i < num_frames; i++)
                        free(frames[i].idat_data);
                free(frames);
                return NULL;
        }
        if (actual_frames < (int)num_frames) {
                KROSSHAIR_LOG("[APNG] warning: expected %u frames but found %d\n",
                              num_frames, actual_frames);
        }

        /* build vertical atlas: each row is canvas_w x canvas_h */
        size_t frame_stride = (size_t)canvas_w * canvas_h * 4;
        unsigned char* atlas = calloc(actual_frames, frame_stride);
        int* delays = malloc(sizeof(int) * actual_frames);
        unsigned char* canvas = calloc(1, frame_stride);
        unsigned char* prev_canvas = NULL; /* for dispose_op = APNG_DISPOSE_OP_PREVIOUS */
        if (!atlas || !delays || !canvas) {
                free(atlas);
                free(delays);
                free(canvas);
                for (uint32_t i = 0; i < num_frames; i++)
                        free(frames[i].idat_data);
                free(frames);
                return NULL;
        }

        for (int i = 0; i < actual_frames; i++) {
                apng_frame_info_t* f = &frames[i];

                /* compute delay in ms */
                uint16_t den = f->delay_den ? f->delay_den : 100;
                int delay_ms = (int)((uint32_t)f->delay_num * 1000 / den);
                if (delay_ms <= 0) delay_ms = 100;
                delays[i] = delay_ms;

                /* save canvas for APNG_DISPOSE_OP_PREVIOUS before compositing */
                if (f->dispose_op == 2) { /* APNG_DISPOSE_OP_PREVIOUS */
                        if (!prev_canvas) prev_canvas = malloc(frame_stride);
                        if (prev_canvas) memcpy(prev_canvas, canvas, frame_stride);
                }

                /* decode this frame's pixels */
                if (f->idat_size == 0) {
                        KROSSHAIR_LOG("[APNG] frame %d has no image data, skipping\n", i);
                        memcpy(atlas + i * frame_stride, canvas, frame_stride);
                        continue;
                }

                int fw, fh;
                stbi_uc* fpix = apng_decode_frame(f, ihdr_data, &fw, &fh);
                if (!fpix) {
                        KROSSHAIR_LOG("[APNG] failed to decode frame %d\n", i);
                        memcpy(atlas + i * frame_stride, canvas, frame_stride);
                        continue;
                }

                /* apply blend_op */
                if (f->blend_op == 0) { /* APNG_BLEND_OP_SOURCE */
                        apng_blend_source(canvas, fpix, canvas_w, canvas_h,
                                          f->x_offset, f->y_offset,
                                          f->width, f->height);
                } else { /* APNG_BLEND_OP_OVER */
                        apng_blend_over(canvas, fpix, canvas_w, canvas_h,
                                        f->x_offset, f->y_offset,
                                        f->width, f->height);
                }
                stbi_image_free(fpix);

                /* copy composited canvas to atlas row */
                memcpy(atlas + i * frame_stride, canvas, frame_stride);

                /* apply dispose_op (affects canvas for NEXT frame) */
                if (f->dispose_op == 1) { /* APNG_DISPOSE_OP_BACKGROUND */
                        apng_clear_region(canvas, canvas_w, canvas_h,
                                          f->x_offset, f->y_offset,
                                          f->width, f->height);
                } else if (f->dispose_op == 2) { /* APNG_DISPOSE_OP_PREVIOUS */
                        if (prev_canvas) memcpy(canvas, prev_canvas, frame_stride);
                }
                /* dispose_op 0 (APNG_DISPOSE_OP_NONE): leave canvas as-is */
        }

        free(canvas);
        free(prev_canvas);
        for (uint32_t i = 0; i < num_frames; i++)
                free(frames[i].idat_data);
        free(frames);

        *out_width  = (int)canvas_w;
        *out_height = (int)canvas_h;
        *out_frames = actual_frames;
        *out_delays = delays;

        KROSSHAIR_LOG("[APNG] decoded %d frames, canvas %ux%u\n",
                      actual_frames, canvas_w, canvas_h);

        return atlas;
}

/* ────────────────── end APNG loader ─────────────────── */

static void ensure_swapchain_crosshair(swapchain_data_t* data,
                                       VkCommandBuffer cmd_buffer)
{
        device_data_t* device_data = data->device_data;

        char* crosshair_path = get_crosshair_path();
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
                const char* ext = strrchr(crosshair_path, '.');
                int is_gif = ext && (strcasecmp(ext, ".gif") == 0);
                int is_apng = ext && (strcasecmp(ext, ".apng") == 0);

                /* .png files might also be APNG -- detect by checking for
                 * acTL chunk if the extension is .png */
                int is_png = ext && (strcasecmp(ext, ".png") == 0);

                if (is_gif) {
                        FILE* f = fopen(crosshair_path, "rb");
                        if (!f) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] failed to open GIF: %s\n",
                                              crosshair_path);
                                free(crosshair_path);
                                return;
                        }

                        fseek(f, 0, SEEK_END);
                        long file_len = ftell(f);
                        fseek(f, 0, SEEK_SET);

                        if (file_len <= 0 || file_len > (long)INT_MAX) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] invalid GIF file size: %ld\n",
                                              file_len);
                                fclose(f);
                                free(crosshair_path);
                                return;
                        }

                        unsigned char* file_buf = malloc((size_t)file_len);
                        if (!file_buf) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] failed to allocate GIF read buffer\n");
                                fclose(f);
                                free(crosshair_path);
                                return;
                        }

                        size_t bytes_read = fread(file_buf, 1, (size_t)file_len, f);
                        fclose(f);

                        if ((long)bytes_read != file_len) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] short read on GIF: %zu/%ld\n",
                                              bytes_read, file_len);
                                free(file_buf);
                                free(crosshair_path);
                                return;
                        }

                        int* delays = NULL;
                        int frames = 0;
                        int gif_len = (int)file_len;
                        stbi_uc* gif_data = stbi_load_gif_from_memory(
                            file_buf, gif_len, &delays, &tex_width, &tex_height,
                            &frames, &tex_channels, STBI_rgb_alpha);
                        free(file_buf);

                        if (!gif_data || frames < 1) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] failed to decode GIF: %s (%s)\n",
                                              crosshair_path,
                                              gif_data ? "no frames" : "decode error");
                                if (gif_data) stbi_image_free(gif_data);
                                if (delays) free(delays);
                                free(crosshair_path);
                                return;
                        }

                        /*
                         * Build a vertical texture atlas: all frames stacked
                         * top-to-bottom in a single image.  The UV coordinates
                         * are adjusted per-frame to select the right slice.
                         *
                         * gif_data from stbi is already laid out as
                         * [frame0][frame1]...[frameN] contiguously, each
                         * frame being (tex_width * tex_height * 4) bytes,
                         * which is exactly the atlas layout we need.
                         */
                        int frame_height = tex_height;
                        int atlas_height = tex_height * frames;
                        image_size = (VkDeviceSize)tex_width * atlas_height * 4;
                        pixels = gif_data;
                        /* tex_height now refers to the full atlas */
                        tex_height = atlas_height;

                        /* store animation state */
                        data->gif_frame_count  = frames;
                        data->gif_frame_height = frame_height;
                        data->gif_current_frame = 0;
                        clock_gettime(CLOCK_MONOTONIC, &data->gif_last_frame_time);

                        if (delays) {
                                data->gif_delays = malloc(sizeof(int) * frames);
                                for (int gi = 0; gi < frames; gi++) {
                                        /* stbi already converts GIF centisecond
                                         * delays to milliseconds internally
                                         * (10 * cs).  delay 0 means "as fast
                                         * as possible", default to ~100ms */
                                        data->gif_delays[gi] =
                                            delays[gi] > 0 ? delays[gi] : 100;
                                }
                                free(delays);
                        } else {
                                data->gif_delays = malloc(sizeof(int) * frames);
                                for (int gi = 0; gi < frames; gi++)
                                        data->gif_delays[gi] = 100;
                        }

                        KROSSHAIR_LOG("[KROSSHAIR] loaded GIF atlas: %dx%d (%d frames, frame_h=%d)\n",
                                      tex_width, atlas_height, frames, frame_height);
                } else if (is_apng || is_png) {
                        /* try to load as APNG; if it's a plain PNG the
                         * loader will return NULL (no acTL) and we fall
                         * through to stbi_load below */
                        FILE* f = fopen(crosshair_path, "rb");
                        if (!f) {
                                KROSSHAIR_LOG("[KROSSHAIR_ERROR] failed to open: %s\n",
                                              crosshair_path);
                                free(crosshair_path);
                                return;
                        }

                        fseek(f, 0, SEEK_END);
                        long file_len = ftell(f);
                        fseek(f, 0, SEEK_SET);

                        unsigned char* file_buf = NULL;
                        if (file_len > 0 && file_len <= (long)INT_MAX) {
                                file_buf = malloc((size_t)file_len);
                                if (file_buf) {
                                        size_t bytes_read = fread(file_buf, 1, (size_t)file_len, f);
                                        if ((long)bytes_read != file_len) {
                                                free(file_buf);
                                                file_buf = NULL;
                                        }
                                }
                        }
                        fclose(f);

                        int* apng_delays = NULL;
                        int apng_frames = 0;
                        int apng_w = 0, apng_h = 0;
                        unsigned char* apng_data = NULL;

                        if (file_buf) {
                                apng_data = load_apng((const unsigned char*)file_buf,
                                                      (size_t)file_len, &apng_w, &apng_h,
                                                      &apng_frames, &apng_delays);
                                free(file_buf);
                        }

                        if (apng_data && apng_frames > 1) {
                                /* animated APNG -- same atlas approach as GIF */
                                tex_width = apng_w;
                                int frame_height = apng_h;
                                int atlas_height = apng_h * apng_frames;
                                tex_height = atlas_height;
                                image_size = (VkDeviceSize)tex_width * atlas_height * 4;
                                pixels = (stbi_uc*)apng_data;

                                data->gif_frame_count   = apng_frames;
                                data->gif_frame_height  = frame_height;
                                data->gif_current_frame = 0;
                                clock_gettime(CLOCK_MONOTONIC, &data->gif_last_frame_time);

                                data->gif_delays = apng_delays;

                                KROSSHAIR_LOG("[KROSSHAIR] loaded APNG atlas: %dx%d (%d frames, frame_h=%d)\n",
                                              tex_width, atlas_height, apng_frames, frame_height);
                        } else {
                                /* not animated APNG (or single frame) --
                                 * fall back to regular stbi_load for
                                 * proper PNG handling */
                                if (apng_data) free(apng_data);
                                if (apng_delays) free(apng_delays);

                                pixels = stbi_load(crosshair_path, &tex_width, &tex_height,
                                                   &tex_channels, STBI_rgb_alpha);
                                image_size = tex_width * tex_height * 4;
                        }
                } else {
                        pixels = stbi_load(crosshair_path, &tex_width, &tex_height,
                                           &tex_channels, STBI_rgb_alpha);
                        image_size = tex_width * tex_height * 4;
                }

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

        data->crosshair_tex_width = tex_width;

        if (data->gif_frame_count > 1) {
                /* for GIF, use frame dimensions for quad size,
                 * UV selects first frame from atlas */
                float uv_step = 1.0f / (float)data->gif_frame_count;
                setup_vertices_uv((float)data->width, (float)data->height,
                                  (float)data->gif_frame_height,
                                  (float)tex_width, 1.0f,
                                  0.0f, uv_step);
        } else {
                setup_vertices((float)data->width, (float)data->height,
                               (float)tex_height, (float)tex_width, 1.0f);
        }

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
                VkResult fence_status = device_data->vtable.GetFenceStatus(
                    device_data->device, draw->fence);
                if (fence_status == VK_SUCCESS) {
                        VK_CHECK(device_data->vtable.ResetFences(
                            device_data->device, 1, &draw->fence));
                        return;
                }
                if (fence_status == VK_NOT_READY) {
                        VkResult wait_result = device_data->vtable.WaitForFences(
                            device_data->device, 1, &draw->fence, VK_TRUE, 100000000);
                        if (wait_result == VK_SUCCESS) {
                                VK_CHECK(device_data->vtable.ResetFences(
                                    device_data->device, 1, &draw->fence));
                                return;
                        }
                }
                destroy_draw(data, draw);
                data->draw = NULL;
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

static void destroy_draw(swapchain_data_t* data, krosshair_draw_t* draw)
{
        if (!draw) return;

        device_data_t* device_data = data->device_data;

        if (draw->vertex_buffer != VK_NULL_HANDLE) {
                device_data->vtable.DestroyBuffer(device_data->device,
                                                  draw->vertex_buffer, NULL);
        }
        if (draw->vertex_buffer_mem != VK_NULL_HANDLE) {
                device_data->vtable.FreeMemory(device_data->device,
                                               draw->vertex_buffer_mem, NULL);
        }
        if (draw->index_buffer != VK_NULL_HANDLE) {
                device_data->vtable.DestroyBuffer(device_data->device,
                                                  draw->index_buffer, NULL);
        }
        if (draw->index_buffer_mem != VK_NULL_HANDLE) {
                device_data->vtable.FreeMemory(device_data->device,
                                               draw->index_buffer_mem, NULL);
        }
        if (draw->semaphore != VK_NULL_HANDLE) {
                device_data->vtable.DestroySemaphore(device_data->device,
                                                     draw->semaphore, NULL);
        }
        if (draw->crossengine_semaphore != VK_NULL_HANDLE) {
                device_data->vtable.DestroySemaphore(device_data->device,
                                                     draw->crossengine_semaphore, NULL);
        }
        if (draw->fence != VK_NULL_HANDLE) {
                device_data->vtable.DestroyFence(device_data->device,
                                                draw->fence, NULL);
        }

        free(draw);
}

static krosshair_draw_t* render_swapchain_display(
    swapchain_data_t* data, queue_data_t* present_queue,
    const VkSemaphore* wait_semaphores, unsigned n_wait_semaphores,
    unsigned image_index)
{
        device_data_t* device_data = data->device_data;

        create_draw(data);
        krosshair_draw_t* draw = data->draw;
        if (!draw) {
                KROSSHAIR_LOG("[KROSSHAIR] create_draw failed, no draw available\n");
                return NULL;
        }
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

        /* advance animation frame if needed (GIF or APNG) */
        if (data->gif_frame_count > 1 && data->gif_delays) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);

                long elapsed_ms =
                    (now.tv_sec - data->gif_last_frame_time.tv_sec) * 1000 +
                    (now.tv_nsec - data->gif_last_frame_time.tv_nsec) / 1000000;

                int advanced = 0;
                int delay = data->gif_delays[data->gif_current_frame];

                /* consume all elapsed time, advancing multiple frames if
                 * the present rate is lower than the animation rate */
                while (elapsed_ms >= delay) {
                        /* accumulate: add delay to last_frame_time instead
                         * of resetting to now, so leftover time carries
                         * over and the animation stays in sync */
                        data->gif_last_frame_time.tv_nsec += (long)delay * 1000000L;
                        while (data->gif_last_frame_time.tv_nsec >= 1000000000L) {
                                data->gif_last_frame_time.tv_sec++;
                                data->gif_last_frame_time.tv_nsec -= 1000000000L;
                        }

                        data->gif_current_frame =
                            (data->gif_current_frame + 1) % data->gif_frame_count;
                        delay = data->gif_delays[data->gif_current_frame];
                        advanced = 1;

                        /* recalculate elapsed from updated base */
                        elapsed_ms =
                            (now.tv_sec - data->gif_last_frame_time.tv_sec) * 1000 +
                            (now.tv_nsec - data->gif_last_frame_time.tv_nsec) / 1000000;
                }

                if (advanced) {
                        float uv_step = 1.0f / (float)data->gif_frame_count;
                        float uv_top  = uv_step * (float)data->gif_current_frame;
                        float uv_bot  = uv_top + uv_step;
                        setup_vertices_uv(
                            (float)data->width, (float)data->height,
                            (float)data->gif_frame_height,
                            (float)data->crosshair_tex_width, 1.0f,
                            uv_top, uv_bot);

                        /* force vertex buffer re-upload with new UVs */
                        draw->vertex_buffer_initialized = 0;
                }
        }

        size_t vertex_size = sizeof(vertices);
        size_t index_size  = sizeof(indices);
        if (draw->vertex_buffer_size < vertex_size) {
                create_or_resize_buffer(device_data, &draw->vertex_buffer,
                                        &draw->vertex_buffer_mem,
                                        &draw->vertex_buffer_size, vertex_size,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                draw->vertex_buffer_initialized = 0;
        }
        if (draw->index_buffer_size < index_size) {
                create_or_resize_buffer(device_data, &draw->index_buffer,
                                        &draw->index_buffer_mem,
                                        &draw->index_buffer_size, index_size,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
                draw->index_buffer_initialized = 0;
        }

        if (!draw->vertex_buffer_initialized) {
                void* vtx_dst = NULL;
                VK_CHECK(device_data->vtable.MapMemory(
                    device_data->device, draw->vertex_buffer_mem, 0,
                    draw->vertex_buffer_size, 0, &vtx_dst));
                memcpy(vtx_dst, vertices, sizeof(vertices));

                VkMappedMemoryRange vtx_range = {};
                vtx_range.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                vtx_range.memory              = draw->vertex_buffer_mem;
                vtx_range.size                = VK_WHOLE_SIZE;
                VK_CHECK(device_data->vtable.FlushMappedMemoryRanges(
                    device_data->device, 1, &vtx_range));
                device_data->vtable.UnmapMemory(device_data->device,
                                                draw->vertex_buffer_mem);
                draw->vertex_buffer_initialized = 1;
        }

        if (!draw->index_buffer_initialized) {
                void* idx_dst = NULL;
                VK_CHECK(device_data->vtable.MapMemory(
                    device_data->device, draw->index_buffer_mem, 0,
                    draw->index_buffer_size, 0, &idx_dst));
                memcpy(idx_dst, indices, sizeof(indices));

                VkMappedMemoryRange idx_range = {};
                idx_range.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                idx_range.memory              = draw->index_buffer_mem;
                idx_range.size                = VK_WHOLE_SIZE;
                VK_CHECK(device_data->vtable.FlushMappedMemoryRanges(
                    device_data->device, 1, &idx_range));
                device_data->vtable.UnmapMemory(device_data->device,
                                                draw->index_buffer_mem);
                draw->index_buffer_initialized = 1;
        }

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
        device_data->vtable.CmdDrawIndexed(draw->cmd_buffer,
                                            sizeof(indices) / sizeof(indices[0]),
                                            1, 0, 0, 0);

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

        VkResult end_result = device_data->vtable.EndCommandBuffer(draw->cmd_buffer);
        if (end_result != VK_SUCCESS) {
                KROSSHAIR_LOG("[KROSSHAIR] EndCommandBuffer failed: %d\n", end_result);
                return NULL;
        }

        /* when presenting on a different queue than where we're drawing the
         * crosshair *AND* when the application does not provide a semaphore to
         * vkQueuePresent, insert our own cross-engine synchronization
         * semaphore.
         * */
        VkResult submit_result = VK_SUCCESS;
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

                submit_result = device_data->vtable.QueueSubmit(
                    present_queue->queue, 1, &submit_info, VK_NULL_HANDLE);
                if (submit_result != VK_SUCCESS) {
                        KROSSHAIR_LOG("[KROSSHAIR] crossengine QueueSubmit failed: %d\n",
                                      submit_result);
                        return NULL;
                }

                submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.commandBufferCount = 1;
                submit_info.pWaitDstStageMask  = &stages_wait;
                submit_info.pCommandBuffers    = &draw->cmd_buffer;
                submit_info.waitSemaphoreCount = 1;
                submit_info.pWaitSemaphores    = &draw->crossengine_semaphore;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores    = &draw->semaphore;

                submit_result = device_data->vtable.QueueSubmit(
                    device_data->graphic_queue->queue, 1, &submit_info,
                    draw->fence);
                if (submit_result != VK_SUCCESS) {
                        KROSSHAIR_LOG("[KROSSHAIR] graphic QueueSubmit failed: %d\n",
                                      submit_result);
                        return NULL;
                }
        } else {
                /* wait in the fragment stage until the swapchain image is ready
                 */
                VkPipelineStageFlags* stages_wait =
                    malloc(sizeof(*stages_wait) * (n_wait_semaphores ? n_wait_semaphores : 1));
                for (unsigned s = 0; s < n_wait_semaphores; s++)
                        stages_wait[s] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                VkSubmitInfo submit_info       = {};
                submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.commandBufferCount = 1;
                submit_info.pCommandBuffers    = &draw->cmd_buffer;
                submit_info.pWaitDstStageMask  = stages_wait;
                submit_info.waitSemaphoreCount = n_wait_semaphores;
                submit_info.pWaitSemaphores    = wait_semaphores;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores    = &draw->semaphore;

                submit_result = device_data->vtable.QueueSubmit(
                    device_data->graphic_queue->queue, 1, &submit_info,
                    draw->fence);
                free(stages_wait);
                if (submit_result != VK_SUCCESS) {
                        KROSSHAIR_LOG("[KROSSHAIR] QueueSubmit failed: %d (wait_sems=%u)\n",
                                      submit_result, n_wait_semaphores);
                        return NULL;
                }
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
        data->n_images = n_images;

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

        /* save reference to old swapchain data - we must NOT destroy it before
         * the create call because the driver needs oldSwapchain to be valid */
        swapchain_data_t* old_swapchain_data = NULL;
        if (pCreateInfo->oldSwapchain != VK_NULL_HANDLE) {
                old_swapchain_data =
                    FIND_OBJ(swapchain_data_t, pCreateInfo->oldSwapchain);
        }

        VkResult result = device_data->vtable.CreateSwapchainKHR(
            device, &create_info, pAllocator, pSwapchain);
        if (result != VK_SUCCESS) return result;

        /* now that the new swapchain is created, clean up the old one's
         * internal resources (Vulkan retires oldSwapchain automatically) */
        if (old_swapchain_data) {
                destroy_swapchain_data(old_swapchain_data);
                unmap_object(HKEY(old_swapchain_data->swapchain));
                free(old_swapchain_data);
        }

        swapchain_data_t* swapchain_data =
            new_swapchain_data(*pSwapchain, device_data);

        setup_swapchain_data(swapchain_data, pCreateInfo);

        KROSSHAIR_LOG("[KROSSHAIR] CreateSwapchainKHR: created %lu (%ux%u, n_images=%u, old=%lu)\n",
                      (unsigned long)*pSwapchain, pCreateInfo->imageExtent.width,
                      pCreateInfo->imageExtent.height, swapchain_data->n_images,
                      (unsigned long)pCreateInfo->oldSwapchain);

        return result;
}

static void overlay_DestroySwapchainKHR(VkDevice device,
                                        VkSwapchainKHR swapchain,
                                        const VkAllocationCallbacks* pAllocator)
{
        device_data_t* device_data = FIND_OBJ(device_data_t, device);
        swapchain_data_t* data     = FIND_OBJ(swapchain_data_t, swapchain);

        if (data) {
                destroy_swapchain_data(data);
                unmap_object(HKEY(data->swapchain));
                free(data);
        }

        device_data->vtable.DestroySwapchainKHR(device_data->device,
                                                swapchain, pAllocator);
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

        if (!queue_data) {
                KROSSHAIR_LOG("[KROSSHAIR] QueuePresent: queue_data is NULL!\n");
                return VK_SUCCESS;
        }

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

                krosshair_draw_t* draw = NULL;
                if (swapchain_data) {
                        draw = before_present(
                            swapchain_data, queue_data,
                            pPresentInfo->pWaitSemaphores,
                            i == 0 ? pPresentInfo->waitSemaphoreCount : 0,
                            image_index);
                }

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
