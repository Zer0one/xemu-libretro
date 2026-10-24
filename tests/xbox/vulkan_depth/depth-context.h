#pragma once
#include "probe-common.h"
#include <array>
#include <cmath>
#include <cstdint>

struct Push { float vertex[3][4], config[4], extra[4], clip[4]; };
struct Image { VkImage image; VkDeviceMemory memory; VkImageView view; };
struct Buffer { VkBuffer buffer; VkDeviceMemory memory; void *mapped; };
struct Context {
    VkInstance instance; VkPhysicalDevice gpu; VkDevice device; VkQueue queue;
    VkDebugUtilsMessengerEXT messenger{}; VkCommandPool pool; VkCommandBuffer cmd; VkFence fence;
    VkPhysicalDeviceProperties properties; VkPhysicalDeviceFeatures enabled_features{}; unsigned family;
    Context() {
        const char *exts[]={VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="xemu-depth-comparison";app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=exts;
        VK_OK(vkCreateInstance(&ci,nullptr,&instance));
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;debug.pfnUserCallback=debug_callback;
        auto fn=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));
        if(fn) VK_OK(fn(instance,&debug,nullptr,&messenger));
        unsigned count=0;VK_OK(vkEnumeratePhysicalDevices(instance,&count,nullptr));if(!count)std::exit(1);
        std::vector<VkPhysicalDevice> devices(count);VK_OK(vkEnumeratePhysicalDevices(instance,&count,devices.data()));gpu=devices[0];vkGetPhysicalDeviceProperties(gpu,&properties);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,families.data());
        family=0;while(family<count&&!(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))++family;if(family==count)std::exit(1);
        float priority=1;VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;
        VK_OK(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&count,nullptr));std::vector<VkExtensionProperties> available(count);VK_OK(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&count,available.data()));
        std::vector<const char *> enabled;for(auto&e:available)if(!strcmp(e.extensionName,"VK_KHR_portability_subset"))enabled.push_back("VK_KHR_portability_subset");
        VkPhysicalDeviceFeatures available_features;vkGetPhysicalDeviceFeatures(gpu,&available_features);enabled_features.depthClamp=available_features.depthClamp;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};d.pEnabledFeatures=&enabled_features;d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;d.enabledExtensionCount=unsigned(enabled.size());d.ppEnabledExtensionNames=enabled.data();VK_OK(vkCreateDevice(gpu,&d,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;VK_OK(vkCreateCommandPool(device,&cp,nullptr,&pool));
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.commandBufferCount=1;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;VK_OK(vkAllocateCommandBuffers(device,&ca,&cmd));
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK_OK(vkCreateFence(device,&fc,nullptr,&fence));
    }
    ~Context(){vkDestroyFence(device,fence,nullptr);vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);if(messenger)reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"))(instance,messenger,nullptr);vkDestroyInstance(instance,nullptr);}
    Image image(unsigned width,unsigned height,VkFormat fmt,VkImageUsageFlags usage,VkImageAspectFlags aspect){
        Image out{};VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=fmt;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
        VK_OK(vkCreateImage(device,&ci,nullptr,&out.image));VkMemoryRequirements req;vkGetImageMemoryRequirements(device,out.image,&req);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=memory_type(gpu,req.memoryTypeBits,0);VK_OK(vkAllocateMemory(device,&a,nullptr,&out.memory));VK_OK(vkBindImageMemory(device,out.image,out.memory,0));
        VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=out.image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=fmt;v.subresourceRange={aspect,0,1,0,1};VK_OK(vkCreateImageView(device,&v,nullptr,&out.view));return out;
    }
    Buffer buffer(VkDeviceSize bytes){Buffer out{};VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=bytes;ci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;VK_OK(vkCreateBuffer(device,&ci,nullptr,&out.buffer));VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,out.buffer,&req);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=memory_type(gpu,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);VK_OK(vkAllocateMemory(device,&a,nullptr,&out.memory));VK_OK(vkBindBufferMemory(device,out.buffer,out.memory,0));VK_OK(vkMapMemory(device,out.memory,0,bytes,0,&out.mapped));return out;}
    void destroy(Image &i){vkDestroyImageView(device,i.view,nullptr);vkDestroyImage(device,i.image,nullptr);vkFreeMemory(device,i.memory,nullptr);}
    void destroy(Buffer &b){vkUnmapMemory(device,b.memory);vkDestroyBuffer(device,b.buffer,nullptr);vkFreeMemory(device,b.memory,nullptr);}
    void begin(){VK_OK(vkResetFences(device,1,&fence));VK_OK(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};VK_OK(vkBeginCommandBuffer(cmd,&b));}
    void submit(){VK_OK(vkEndCommandBuffer(cmd));VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cmd;VK_OK(vkQueueSubmit(queue,1,&s,fence));VK_OK(vkWaitForFences(device,1,&fence,VK_TRUE,10000000000ull));}
};
struct Pipelines {
    Context &c;VkRenderPass pass;VkPipelineLayout layout;VkPipeline write,seed;std::array<VkPipeline,3> compare;
    VkFormat format;bool integer16;
    Pipelines(Context &ctx,VkShaderModule vertex,VkShaderModule fragment,bool d24,bool clamp=true):c(ctx),integer16(!d24){
        auto supports=[&](VkFormat f){VkImageFormatProperties p;return vkGetPhysicalDeviceImageFormatProperties(c.gpu,f,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,0,&p)==VK_SUCCESS;};
        format=d24?(supports(VK_FORMAT_D24_UNORM_S8_UINT)?VK_FORMAT_D24_UNORM_S8_UINT:VK_FORMAT_D32_SFLOAT_S8_UINT):VK_FORMAT_D16_UNORM;
        if(!supports(format)){fprintf(stderr,"Required depth format unavailable\n");std::exit(1);}
        // D24 native packing would need a separate decoder; Apple uses D32S8.
        if(format==VK_FORMAT_D24_UNORM_S8_UINT){fprintf(stderr,"Native D24 decoder not implemented in this Apple probe\n");std::exit(1);}
        VkAttachmentDescription a[2]{};for(auto &i:a){i.samples=VK_SAMPLE_COUNT_1_BIT;i.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;i.storeOp=VK_ATTACHMENT_STORE_OP_STORE;i.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;i.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;i.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;}
        a[0].format=VK_FORMAT_R32G32B32A32_SFLOAT;a[1].format=format;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth;
        VkSubpassDependency dep{};dep.srcSubpass=0;dep.dstSubpass=VK_SUBPASS_EXTERNAL;dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;dep.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;dep.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;dep.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=2;rp.pAttachments=a;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;VK_OK(vkCreateRenderPass(c.device,&rp,nullptr,&pass));
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push)};VkPipelineLayoutCreateInfo l{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};l.pushConstantRangeCount=1;l.pPushConstantRanges=&range;VK_OK(vkCreatePipelineLayout(c.device,&l,nullptr,&layout));
        VkPipelineShaderStageCreateInfo stages[2]{};for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=i?fragment:vertex;stages[i].pName="main";}
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;rs.depthClampEnable=clamp?c.enabled_features.depthClamp:VK_FALSE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&blend;
        VkDynamicState dyn[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dyn;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_ALWAYS;
        VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pci.stageCount=2;pci.pStages=stages;pci.pVertexInputState=&vi;pci.pInputAssemblyState=&ia;pci.pViewportState=&vp;pci.pRasterizationState=&rs;pci.pMultisampleState=&ms;pci.pColorBlendState=&cb;pci.pDynamicState=&dynamic;pci.pDepthStencilState=&ds;pci.layout=layout;pci.renderPass=pass;
        VK_OK(vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&pci,nullptr,&write));blend.colorWriteMask=0;VK_OK(vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&pci,nullptr,&seed));blend.colorWriteMask=15;ds.depthWriteEnable=VK_FALSE;
        VkCompareOp ops[]={VK_COMPARE_OP_EQUAL,VK_COMPARE_OP_LESS,VK_COMPARE_OP_GREATER};for(int i=0;i<3;++i){ds.depthCompareOp=ops[i];VK_OK(vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&pci,nullptr,&compare[i]));}
    }
    ~Pipelines(){for(auto p:compare)vkDestroyPipeline(c.device,p,nullptr);vkDestroyPipeline(c.device,seed,nullptr);vkDestroyPipeline(c.device,write,nullptr);vkDestroyPipelineLayout(c.device,layout,nullptr);vkDestroyRenderPass(c.device,pass,nullptr);}
};
