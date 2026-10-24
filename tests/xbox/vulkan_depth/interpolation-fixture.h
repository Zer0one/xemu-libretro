#pragma once
#include <cmath>
#include <string>
#include <vector>

struct InterpolationFixture {
    unsigned sx,sy,clamp,zmode,homogeneous;
    float vertex[3][4],config[4],extra[4],clip[4];
    std::string name;
};
static std::vector<InterpolationFixture> interpolation_fixtures()
{
    std::vector<InterpolationFixture> result;
    const unsigned scales[][2]={{1,1},{4,2}};
    const char *zlabels[]={"base","within","far","near"};
    const char *hlabels[]={"original","power2","normalized","neutral"};
    const float maxz=16777215.f;
    for(auto &s:scales)for(unsigned clamp=0;clamp<2;++clamp)for(unsigned homogeneous=0;homogeneous<4;++homogeneous)for(unsigned zmode=0;zmode<4;++zmode){
        InterpolationFixture f{};f.sx=s[0];f.sy=s[1];f.clamp=clamp;f.zmode=zmode;f.homogeneous=homogeneous;
        const float v[3][4]={{2.25f,3,.12f*maxz,.1f*maxz},{29,4.25f,.73f*maxz,.65f*maxz},{5,28,.42f*maxz,.32f*maxz}};
        for(int i=0;i<3;++i)for(int j=0;j<4;++j)f.vertex[i][j]=v[i][j];
        if(zmode==1)f.vertex[1][2]=.9f*maxz;
        if(zmode==2)f.vertex[1][2]=1.5f*maxz;
        if(zmode==3)f.vertex[0][2]=-.5f*maxz;
        f.config[0]=float(f.sx);f.config[1]=float(f.sy);f.config[2]=maxz;
        f.extra[1]=1;f.extra[2]=1;
        f.clip[2]=(homogeneous==0||homogeneous==3)?1.f:(homogeneous==1?std::ldexp(1.f,-16):1.f/maxz);
        f.clip[3]=homogeneous==3?1.f:0.f;
        f.name="s"+std::to_string(f.sx)+"x"+std::to_string(f.sy)+"-clamp"+std::to_string(clamp)+"-"+hlabels[homogeneous]+"-"+zlabels[zmode];
        result.push_back(f);
    }
    return result;
}
