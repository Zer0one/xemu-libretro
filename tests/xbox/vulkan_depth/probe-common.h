#pragma once
/* Offscreen Vulkan readback for the no-GS depth candidates. No emulator/UI. */
#include <vulkan/vulkan.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define VK_OK(expr) do { VkResult result = (expr); if (result != VK_SUCCESS) { \
    fprintf(stderr, "%s failed: %d\n", #expr, result); std::exit(1); } } while (0)
static unsigned validation_errors;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *data, void *)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++validation_errors;
    fprintf(stderr, "validation: %s\n", data->pMessage);
    return VK_FALSE;
}
static std::vector<unsigned> compile(const char *path, glslang_stage_t stage)
{
    std::ifstream file(path);
    if (!file) { fprintf(stderr, "Cannot read %s\n", path); std::exit(1); }
    std::string code((std::istreambuf_iterator<char>(file)), {});
    glslang_input_t input{};
    input.language = GLSLANG_SOURCE_GLSL; input.stage = stage;
    input.client = GLSLANG_CLIENT_VULKAN; input.client_version = GLSLANG_TARGET_VULKAN_1_1;
    input.target_language = GLSLANG_TARGET_SPV; input.target_language_version = GLSLANG_TARGET_SPV_1_3;
    input.code = code.c_str(); input.default_version = 450;
    input.resource = glslang_default_resource();
    auto shader = glslang_shader_create(&input);
    if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input)) {
        fprintf(stderr, "%s: %s\n", path, glslang_shader_get_info_log(shader)); std::exit(1);
    }
    auto program = glslang_program_create(); glslang_program_add_shader(program, shader);
    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        fprintf(stderr, "%s\n", glslang_program_get_info_log(program)); std::exit(1);
    }
    glslang_spv_options_t options{}; options.validate = true;
    glslang_program_SPIRV_generate_with_options(program, stage, &options);
    std::vector<unsigned> spv(glslang_program_SPIRV_get_size(program));
    glslang_program_SPIRV_get(program, spv.data());
    glslang_program_delete(program); glslang_shader_delete(shader);
    return spv;
}
static unsigned memory_type(VkPhysicalDevice gpu, unsigned bits, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties props; vkGetPhysicalDeviceMemoryProperties(gpu, &props);
    for (unsigned i = 0; i < props.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) return i;
    fprintf(stderr, "No compatible memory type\n"); std::exit(1);
}
