/* Real depth attachment writes, comparisons, clipping and discard placement. */
#include "depth-context.h"

int main(int argc,char **argv){
    if(argc!=4)return 1;glslang_initialize_process();Context c;
    VkShaderModule modules[2];for(int i=0;i<2;++i){auto spv=compile(argv[i+1],i?GLSLANG_STAGE_FRAGMENT:GLSLANG_STAGE_VERTEX);VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=spv.size()*4;ci.pCode=spv.data();VK_OK(vkCreateShaderModule(c.device,&ci,nullptr,&modules[i]));}
    {
    Pipelines d16(c,modules[0],modules[1],false),d24(c,modules[0],modules[1],true);
    printf("{\"device\":\"%s\",\"d16_format\":%d,\"d24_host_format\":%d,\"host_depth_clamp\":%u,\"cases\":[",c.properties.deviceName,d16.format,d24.format,c.enabled_features.depthClamp);
    const unsigned scales[][2]={{1,1},{4,2}};const char *shapes[]={"ordinary","side-clip","near-clip","far-clip","negative-w","tiny-w","all-negative-w","degenerate"};bool first=true;
    for(auto &scale:scales)for(int is24=0;is24<2;++is24)for(int wmode=0;wmode<2;++wmode)for(int bias=0;bias<2;++bias)for(int shape=0;shape<8;++shape)for(int mask=0;mask<2;++mask)for(int guest_clip=0;guest_clip<2;++guest_clip){
        auto &p=is24?d24:d16;unsigned width=32*scale[0],height=32*scale[1],n=width*height;VkDeviceSize color_bytes=n*16,depth_bytes=n*(is24?4:2);
        auto color=c.image(width,height,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        auto depth=c.image(width,height,p.format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,is24?VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT:VK_IMAGE_ASPECT_DEPTH_BIT);
        auto readback=c.buffer(color_bytes+depth_bytes);VkImageView views[]={color.view,depth.view};VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=p.pass;f.attachmentCount=2;f.pAttachments=views;f.width=width;f.height=height;f.layers=1;VkFramebuffer fb;VK_OK(vkCreateFramebuffer(c.device,&f,nullptr,&fb));
        float maxz=is24?16777215.f:65535.f;Push push{{{2.25f,3,.12f*maxz,.1f*maxz},{29,4.25f,.73f*maxz,.65f*maxz},{5,28,.42f*maxz,.32f*maxz}},{float(scale[0]),float(scale[1]),maxz,float(bias)},{bias?.75f:0,float(wmode),float(is24),0},{float(mask),float(guest_clip),0,0}};
        if(shape==1){push.vertex[0][0]=-12;push.vertex[1][0]=42;push.vertex[2][1]=40;}
        if(shape==2)push.vertex[0][2]=-.5f*maxz;
        if(shape==3)push.vertex[1][2]=1.5f*maxz;
        if(shape==4)push.vertex[0][3]=-.1f*maxz;
        if(shape==5)push.vertex[0][3]=1e-6f;
        if(shape==6)for(auto &v:push.vertex)v[3]=-std::abs(v[3]);
        if(shape==7)for(int i=1;i<3;++i){push.vertex[i][0]=push.vertex[0][0];push.vertex[i][1]=push.vertex[0][1];}
        std::string name="s"+std::to_string(scale[0])+"x"+std::to_string(scale[1])+"-d"+std::to_string(is24?24:16)+(wmode?"-w":"-z")+"-b"+std::to_string(bias)+"-"+shapes[shape]+"-mask"+std::to_string(mask)+"-cull"+std::to_string(guest_clip);
        std::ofstream writes(std::string(argv[3])+"/"+name+".writes",std::ios::binary),comparisons(std::string(argv[3])+"/"+name+".compare",std::ios::binary);
        auto draw=[&](int arm,VkPipeline pipe){push.extra[3]=float(arm);vkCmdBindPipeline(c.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe);vkCmdPushConstants(c.cmd,p.layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);vkCmdDraw(c.cmd,3,1,0,0);};
        auto render=[&](int arm,int compare){
            c.begin();VkClearValue clear[2]{};for(float &v:clear[0].color.float32)v=-1;clear[1].depthStencil={1,0};VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=p.pass;rp.framebuffer=fb;rp.renderArea.extent={width,height};rp.clearValueCount=2;rp.pClearValues=clear;vkCmdBeginRenderPass(c.cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};vkCmdSetViewport(c.cmd,0,1,&viewport);vkCmdSetScissor(c.cmd,0,1,&scissor);
            if(compare>=0){draw(4,p.seed);draw(arm,p.compare[compare]);}else draw(arm,p.write);vkCmdEndRenderPass(c.cmd);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};vkCmdCopyImageToBuffer(c.cmd,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);copy.bufferOffset=color_bytes;copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;vkCmdCopyImageToBuffer(c.cmd,depth.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=readback.buffer;barrier.size=VK_WHOLE_SIZE;vkCmdPipelineBarrier(c.cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);c.submit();
        };
        for(int arm=0;arm<5;++arm){render(arm,-1);writes.write(static_cast<char *>(readback.mapped),color_bytes+depth_bytes);}
        std::vector<uint8_t> survived(n);for(int arm=0;arm<5;++arm)for(int op=0;op<3;++op){render(arm,op);auto values=static_cast<float *>(readback.mapped);for(unsigned i=0;i<n;++i)survived[i]=values[4*i+2]>.5f;comparisons.write(reinterpret_cast<char *>(survived.data()),n);}
        if(!first)printf(",");first=false;printf("{\"name\":\"%s\",\"width\":%u,\"height\":%u,\"scale\":[%u,%u],\"d24\":%d,\"wmode\":%d,\"bias\":%d,\"shape\":\"%s\",\"mask\":%d,\"guest_depth_cull\":%d,\"vertex\":[",name.c_str(),width,height,scale[0],scale[1],is24,wmode,bias,shapes[shape],mask,guest_clip);for(int i=0;i<3;++i)printf("%s[%.9g,%.9g,%.9g,%.9g]",i?",":"",push.vertex[i][0],push.vertex[i][1],push.vertex[i][2],push.vertex[i][3]);printf("]}");fflush(stdout);
        vkDestroyFramebuffer(c.device,fb,nullptr);c.destroy(readback);c.destroy(color);c.destroy(depth);
    }
    }
    for(auto m:modules)vkDestroyShaderModule(c.device,m,nullptr);glslang_finalize_process();printf("],\"validation_errors\":%u}\n",validation_errors);return validation_errors?2:0;
}
