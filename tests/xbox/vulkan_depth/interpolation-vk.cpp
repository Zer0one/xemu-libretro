#include "depth-context.h"
#include "interpolation-fixture.h"
#include <type_traits>

int main(int argc,char **argv)
{
    if(argc!=4)return 1;glslang_initialize_process();Context c;
    VkShaderModule modules[2];for(int i=0;i<2;++i){auto spv=compile(argv[i+1],i?GLSLANG_STAGE_FRAGMENT:GLSLANG_STAGE_VERTEX);VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=spv.size()*4;ci.pCode=spv.data();VK_OK(vkCreateShaderModule(c.device,&ci,nullptr,&modules[i]));}
    {
    Pipelines unclamped(c,modules[0],modules[1],true,false),clamped(c,modules[0],modules[1],true,true);
    printf("{\"device\":\"%s\",\"api\":\"vulkan\",\"depth_clamp_supported\":%u,\"cases\":[",c.properties.deviceName,c.enabled_features.depthClamp);bool first=true;
    for(const auto &f:interpolation_fixtures()){
        auto &p=f.clamp?clamped:unclamped;unsigned width=f.sx*32,height=f.sy*32;VkDeviceSize color_bytes=width*height*16;
        auto color=c.image(width,height,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        auto depth=c.image(width,height,p.format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT);
        auto buffer=c.buffer(color_bytes);VkImageView views[]={color.view,depth.view};VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};ci.renderPass=p.pass;ci.attachmentCount=2;ci.pAttachments=views;ci.width=width;ci.height=height;ci.layers=1;VkFramebuffer fb;VK_OK(vkCreateFramebuffer(c.device,&ci,nullptr,&fb));
        Push push{};memcpy(push.vertex,f.vertex,sizeof(push.vertex));memcpy(push.config,f.config,sizeof(push.config));memcpy(push.extra,f.extra,sizeof(push.extra));memcpy(push.clip,f.clip,sizeof(push.clip));
        c.begin();VkClearValue clear[2]{};for(auto &v:clear[0].color.float32)v=-1;clear[1].depthStencil={1,0};VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=p.pass;rp.framebuffer=fb;rp.renderArea.extent={width,height};rp.clearValueCount=2;rp.pClearValues=clear;vkCmdBeginRenderPass(c.cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};vkCmdSetViewport(c.cmd,0,1,&viewport);vkCmdSetScissor(c.cmd,0,1,&scissor);vkCmdBindPipeline(c.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p.write);vkCmdPushConstants(c.cmd,p.layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);vkCmdDraw(c.cmd,3,1,0,0);vkCmdEndRenderPass(c.cmd);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};vkCmdCopyImageToBuffer(c.cmd,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=buffer.buffer;barrier.size=VK_WHOLE_SIZE;vkCmdPipelineBarrier(c.cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);c.submit();
        std::ofstream output(std::string(argv[3])+"/vk-"+f.name+".f32",std::ios::binary);output.write(static_cast<char *>(buffer.mapped),color_bytes);
        if(!first)printf(",");first=false;printf("{\"file\":\"vk-%s.f32\",\"name\":\"%s\",\"width\":%u,\"height\":%u,\"clamp\":%u,\"zmode\":%u,\"homogeneous\":%u,\"clip_scale\":%.9g}",f.name.c_str(),f.name.c_str(),width,height,f.clamp,f.zmode,f.homogeneous,f.clip[2]);
        vkDestroyFramebuffer(c.device,fb,nullptr);c.destroy(buffer);c.destroy(color);c.destroy(depth);
    }
    }
    for(auto m:modules)vkDestroyShaderModule(c.device,m,nullptr);glslang_finalize_process();printf("],\"validation_errors\":%u}\n",validation_errors);return validation_errors?2:0;
}
