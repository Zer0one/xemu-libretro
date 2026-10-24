#include "probe-common.h"

struct Push { float vertex[3][4]; float config[4]; float extra[4]; };
int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "Usage: probe vertex.glsl fragment.glsl output-directory\n"); return 1; }
    glslang_initialize_process();
    auto vertex_spv = compile(argv[1], GLSLANG_STAGE_VERTEX);
    auto fragment_spv = compile(argv[2], GLSLANG_STAGE_FRAGMENT);
    const char *extensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "xemu-depth-readback"; app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
    ici.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; ici.enabledExtensionCount = 2; ici.ppEnabledExtensionNames = extensions;
    VkInstance instance; VK_OK(vkCreateInstance(&ici, nullptr, &instance));
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug.pfnUserCallback = debug_callback;
    VkDebugUtilsMessengerEXT messenger{};
    auto create_debug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (create_debug) VK_OK(create_debug(instance, &debug, nullptr, &messenger));
    unsigned count = 0; VK_OK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (!count) { fprintf(stderr, "No Vulkan GPU\n"); return 1; }
    std::vector<VkPhysicalDevice> gpus(count); VK_OK(vkEnumeratePhysicalDevices(instance, &count, gpus.data()));
    VkPhysicalDevice gpu = gpus[0]; VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(gpu, &props);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families.data());
    unsigned family = 0; while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
    if (family == count) return 1;
    float priority = 1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qci.queueFamilyIndex = family; qci.queueCount = 1; qci.pQueuePriorities = &priority;
    VK_OK(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> device_exts(count); VK_OK(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, device_exts.data()));
    std::vector<const char *> enabled;
    for (auto &ext : device_exts) if (!strcmp(ext.extensionName, "VK_KHR_portability_subset")) enabled.push_back("VK_KHR_portability_subset");
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = unsigned(enabled.size()); dci.ppEnabledExtensionNames = enabled.data();
    VkDevice device; VK_OK(vkCreateDevice(gpu, &dci, nullptr, &device));
    VkQueue queue; vkGetDeviceQueue(device, family, 0, &queue);
    VkShaderModule modules[2];
    for (int i = 0; i < 2; ++i) {
        auto &spv = i ? fragment_spv : vertex_spv;
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; ci.codeSize = spv.size() * 4; ci.pCode = spv.data();
        VK_OK(vkCreateShaderModule(device, &ci, nullptr, &modules[i]));
    }
    VkAttachmentDescription attachment[2]{}; VkAttachmentReference refs[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        attachment[i].format = VK_FORMAT_R32G32B32A32_SFLOAT; attachment[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachment[i].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        refs[i] = {i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount = 2; sub.pColorAttachments = refs;
    VkSubpassDependency dependency{}; dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rpci.attachmentCount = 2; rpci.pAttachments = attachment;
    rpci.subpassCount = 1; rpci.pSubpasses = &sub; rpci.dependencyCount = 1; rpci.pDependencies = &dependency;
    VkRenderPass renderpass; VK_OK(vkCreateRenderPass(device, &rpci, nullptr, &renderpass));
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &range;
    VkPipelineLayout layout; VK_OK(vkCreatePipelineLayout(device, &plci, nullptr, &layout));
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (int i = 0; i < 2; ++i) { stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT; stages[i].module = modules[i]; stages[i].pName = "main"; }
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.lineWidth = 1; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend[2]{}; for (auto &b : blend) b.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 2; cb.pAttachments = blend;
    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pci.stageCount = 2; pci.pStages = stages;
    pci.pVertexInputState = &vi; pci.pInputAssemblyState = &ia; pci.pViewportState = &vp; pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms; pci.pColorBlendState = &cb; pci.pDynamicState = &dynamic; pci.layout = layout; pci.renderPass = renderpass;
    VkPipeline pipeline; VK_OK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline));
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpci.queueFamilyIndex = family; cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool; VK_OK(vkCreateCommandPool(device, &cpci, nullptr, &pool));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cai.commandPool = pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer command; VK_OK(vkAllocateCommandBuffers(device, &cai, &command));
    printf("{\"device\":\"%s\",\"driver_version\":%u,\"cases\":[", props.deviceName, props.driverVersion);
    bool first = true;
    const unsigned scales[][2] = {{1,1},{2,2},{4,4},{4,2}};
    for (auto &scale : scales) for (int d24 = 0; d24 < 2; ++d24) for (int wmode = 0; wmode < 2; ++wmode) for (int bias = 0; bias < 3; ++bias) {
        unsigned width = 32 * scale[0], height = 32 * scale[1]; VkDeviceSize plane_bytes = width * height * 4 * sizeof(float);
        VkImage images[2]; VkDeviceMemory image_memory[2]; VkImageView views[2];
        for (int i = 0; i < 2; ++i) {
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.imageType = VK_IMAGE_TYPE_2D; ci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            ci.extent = {width,height,1}; ci.mipLevels = ci.arrayLayers = 1; ci.samples = VK_SAMPLE_COUNT_1_BIT;
            ci.tiling = VK_IMAGE_TILING_OPTIMAL; ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            VK_OK(vkCreateImage(device, &ci, nullptr, &images[i])); VkMemoryRequirements req; vkGetImageMemoryRequirements(device, images[i], &req);
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize = req.size; alloc.memoryTypeIndex = memory_type(gpu, req.memoryTypeBits, 0);
            VK_OK(vkAllocateMemory(device, &alloc, nullptr, &image_memory[i])); VK_OK(vkBindImageMemory(device, images[i], image_memory[i], 0));
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image = images[i]; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = ci.format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; VK_OK(vkCreateImageView(device, &view, nullptr, &views[i]));
        }
        VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fbci.renderPass = renderpass; fbci.attachmentCount = 2; fbci.pAttachments = views; fbci.width = width; fbci.height = height; fbci.layers = 1;
        VkFramebuffer framebuffer; VK_OK(vkCreateFramebuffer(device, &fbci, nullptr, &framebuffer));
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bci.size = plane_bytes * 2; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer buffer; VK_OK(vkCreateBuffer(device, &bci, nullptr, &buffer)); VkMemoryRequirements breq; vkGetBufferMemoryRequirements(device, buffer, &breq);
        VkMemoryAllocateInfo ba{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ba.allocationSize = breq.size; ba.memoryTypeIndex = memory_type(gpu, breq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDeviceMemory buffer_memory; VK_OK(vkAllocateMemory(device, &ba, nullptr, &buffer_memory)); VK_OK(vkBindBufferMemory(device, buffer, buffer_memory, 0));
        float maxz = d24 ? 16777215.f : 65535.f;
        Push push{{{2.25f,3.f,.12f*maxz,.10f*maxz},{29.f,4.25f,.73f*maxz,.65f*maxz},{5.f,28.f,.42f*maxz,.32f*maxz}},
                  {float(scale[0]),float(scale[1]),maxz,bias == 0 ? 0.f : (bias == 1 ? 1.f : -0.25f)},
                  {bias == 0 ? 0.f : 0.75f,float(wmode),float(d24),0.f}};
        VK_OK(vkResetCommandBuffer(command, 0)); VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; VK_OK(vkBeginCommandBuffer(command, &begin));
        VkClearValue clear[2]{}; for (auto &c : clear) for (float &f : c.color.float32) f = -1;
        VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rp.renderPass = renderpass; rp.framebuffer = framebuffer; rp.renderArea.extent = {width,height}; rp.clearValueCount = 2; rp.pClearValues = clear;
        vkCmdBeginRenderPass(command, &rp, VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkViewport viewport{0,0,float(width),float(height),0,1}; VkRect2D scissor{{0,0},{width,height}};
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
        vkCmdPushConstants(command,layout,range.stageFlags,0,sizeof(push),&push); vkCmdDraw(command,3,1,0,0); vkCmdEndRenderPass(command);
        for (int i = 0; i < 2; ++i) {
            VkBufferImageCopy copy{}; copy.bufferOffset = plane_bytes * i; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent = {width,height,1};
            vkCmdCopyImageToBuffer(command,images[i],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
        }
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT; barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.buffer = buffer; barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr); VK_OK(vkEndCommandBuffer(command));
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fence; VK_OK(vkCreateFence(device,&fci,nullptr,&fence));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        VK_OK(vkQueueSubmit(queue,1,&submit,fence)); VK_OK(vkWaitForFences(device,1,&fence,VK_TRUE,10000000000ull));
        std::string name = "scale-" + std::to_string(scale[0]) + "x" + std::to_string(scale[1]) + "-d" + (d24 ? "24" : "16") + (wmode ? "-w" : "-z") + "-bias" + std::to_string(bias) + ".f32";
        void *mapped; VK_OK(vkMapMemory(device,buffer_memory,0,plane_bytes*2,0,&mapped));
        std::ofstream output(std::string(argv[3])+"/"+name,std::ios::binary); output.write(static_cast<const char *>(mapped),plane_bytes*2); output.close(); vkUnmapMemory(device,buffer_memory);
        if (!first) printf(","); first = false;
        printf("{\"file\":\"%s\",\"width\":%u,\"height\":%u,\"scale\":[%u,%u],\"d24\":%d,\"wmode\":%d,\"bias\":%d}",name.c_str(),width,height,scale[0],scale[1],d24,wmode,bias);
        vkDestroyFence(device,fence,nullptr); vkDestroyBuffer(device,buffer,nullptr); vkFreeMemory(device,buffer_memory,nullptr); vkDestroyFramebuffer(device,framebuffer,nullptr);
        for (int i=0;i<2;++i) { vkDestroyImageView(device,views[i],nullptr); vkDestroyImage(device,images[i],nullptr); vkFreeMemory(device,image_memory[i],nullptr); }
    }
    vkDestroyCommandPool(device,pool,nullptr); vkDestroyPipeline(device,pipeline,nullptr); vkDestroyPipelineLayout(device,layout,nullptr); vkDestroyRenderPass(device,renderpass,nullptr);
    for (auto module : modules) vkDestroyShaderModule(device,module,nullptr);
    vkDestroyDevice(device,nullptr);
    if (messenger) reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"))(instance,messenger,nullptr);
    vkDestroyInstance(instance,nullptr); glslang_finalize_process();
    printf("],\"validation_errors\":%u}\n",validation_errors);
    return validation_errors ? 2 : 0;
}
