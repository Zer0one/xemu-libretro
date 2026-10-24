/* Native OpenGL reference for the same offscreen interpolation fixtures. */
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include "interpolation-fixture.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

static void gl_check(const char *where)
{
    GLenum error=glGetError();if(error!=GL_NO_ERROR){fprintf(stderr,"%s GL error: 0x%x\n",where,error);std::exit(1);}
}
static GLuint shader(const char *path,GLenum stage)
{
    std::ifstream file(path);if(!file)std::exit(1);
    std::string text((std::istreambuf_iterator<char>(file)),{});const char *source=text.c_str();
    GLuint s=glCreateShader(stage);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint status;glGetShaderiv(s,GL_COMPILE_STATUS,&status);
    if(!status){char log[8192];glGetShaderInfoLog(s,sizeof(log),nullptr,log);fprintf(stderr,"%s: %s\n",path,log);std::exit(1);}return s;
}
int main(int argc,char **argv)
{
    if(argc!=4)return 1;
    CGLPixelFormatAttribute attributes[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,kCGLPFAAccelerated,(CGLPixelFormatAttribute)0};
    CGLPixelFormatObj format;GLint count;CGLError error=CGLChoosePixelFormat(attributes,&format,&count);
    if(error!=kCGLNoError){fprintf(stderr,"CGL format: %s\n",CGLErrorString(error));return 1;}
    CGLContextObj context;error=CGLCreateContext(format,nullptr,&context);CGLDestroyPixelFormat(format);
    if(error!=kCGLNoError){fprintf(stderr,"CGL context: %s\n",CGLErrorString(error));return 1;}CGLSetCurrentContext(context);
    GLuint vs=shader(argv[1],GL_VERTEX_SHADER),fs=shader(argv[2],GL_FRAGMENT_SHADER),program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);GLint status;glGetProgramiv(program,GL_LINK_STATUS,&status);
    if(!status){char log[8192];glGetProgramInfoLog(program,sizeof(log),nullptr,log);fprintf(stderr,"GL link: %s\n",log);return 1;}glUseProgram(program);
    GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DITHER);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);
    printf("{\"device\":\"%s\",\"version\":\"%s\",\"api\":\"opengl\",\"cases\":[",glGetString(GL_RENDERER),glGetString(GL_VERSION));bool first=true;
    for(const auto &f:interpolation_fixtures()){
        unsigned width=f.sx*32,height=f.sy*32;GLuint color,depth,fb;
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH32F_STENCIL8,width,height,0,GL_DEPTH_STENCIL,GL_FLOAT_32_UNSIGNED_INT_24_8_REV,nullptr);
        glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"Incomplete GL framebuffer\n");return 1;}
        glViewport(0,0,width,height);if(f.clamp)glEnable(GL_DEPTH_CLAMP);else glDisable(GL_DEPTH_CLAMP);
        glUniform4fv(glGetUniformLocation(program,"p.vertex[0]"),3,&f.vertex[0][0]);glUniform4fv(glGetUniformLocation(program,"p.config"),1,f.config);glUniform4fv(glGetUniformLocation(program,"p.extra"),1,f.extra);glUniform4fv(glGetUniformLocation(program,"p.clipControl"),1,f.clip);
        glClearColor(-1,-1,-1,-1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glFinish();
        std::vector<float> values(width*height*4);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,values.data());gl_check("draw/readback");
        std::ofstream output(std::string(argv[3])+"/gl-"+f.name+".f32",std::ios::binary);output.write(reinterpret_cast<const char *>(values.data()),values.size()*sizeof(float));
        if(!first)printf(",");first=false;printf("{\"file\":\"gl-%s.f32\",\"name\":\"%s\",\"width\":%u,\"height\":%u,\"clamp\":%u,\"zmode\":%u,\"homogeneous\":%u,\"clip_scale\":%.9g}",f.name.c_str(),f.name.c_str(),width,height,f.clamp,f.zmode,f.homogeneous,f.clip[2]);
        glDeleteFramebuffers(1,&fb);glDeleteTextures(1,&depth);glDeleteTextures(1,&color);
    }
    glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);printf("],\"gl_errors\":0}\n");return 0;
}
