/* Modular internal ridge. Coordinates are world units, converted once to the
 * native 1/16 vertex format. No display-list scaling beyond that conversion.
 * The cliff profile and UV density match the original exterior cliff.
 * A side, a quarter-circle corner and a front section share their endpoints. */
#include "libforest/gbi_extensions.h"
extern u8 cliff_pal_dummy[], earth_pal_dummy[], grass_tex_dummy[], cliff_tex_dummy[];
static Gfx ridge_rock_material[]={
    gsSPTexture(0,0,0,0,G_ON),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2),
    gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,TEXEL0,PRIMITIVE,0,COMBINED,0,0,0,0,COMBINED),
    gsDPLoadTLUT_Dolphin(15,16,1,cliff_pal_dummy),
    gsDPLoadTextureBlock_4b_Dolphin(cliff_tex_dummy,G_IM_FMT_CI,64,64,15,GX_REPEAT,GX_REPEAT,0,0),
    gsDPSetPrimColor(0,128,255,255,255,255),
    gsSPLoadGeometryMode(G_ZBUFFER|G_SHADE|G_CULL_BACK|G_FOG|G_LIGHTING|G_SHADING_SMOOTH),
    gsSPEndDisplayList(),
};
static Gfx ridge_grass_material[]={
    gsDPLoadTLUT_Dolphin(15,16,1,earth_pal_dummy),
    gsDPLoadTextureBlock_4b_Dolphin(grass_tex_dummy,G_IM_FMT_CI,32,32,15,GX_REPEAT,GX_REPEAT,0,0),
    gsSPEndDisplayList(),
};
typedef struct {Vtx vertices[1024];Gfx commands[768];int nv;} RidgeMesh;
static RidgeMesh ridge_mesh[2][2];
static int ridge_ready;
static const float ridge_depth[]={0,10,30,40,60,160};
static const float ridge_y[]={40,160,160,280,280,280};
static const float ridge_t[]={2048,512,0,-1536,-2048,-4608};
typedef struct {float x,z,nx,nz,s;} RidgePath;
static RidgePath ridge_path(float s,int cap) {
    RidgePath p;
    if(!cap || s<=400) {p=(RidgePath){0,s,1,0,s};}
    else if(s<400+251.327412f) {
        float a=(s-400)/160;
        p=(RidgePath){160-160*cosf(a),400+160*sinf(a),cosf(a),-sinf(a),s};
    } else {p=(RidgePath){160+s-651.327412f,560,0,-1,s};}
    return p;
}
static void ridge_vertex(Vtx* v,RidgePath p,int level,int mirror,int grass) {
    float x=p.x+p.nx*ridge_depth[level],z=p.z+p.nz*ridge_depth[level];
    memset(v,0,sizeof(*v));
    v->v.ob[0]=(short)lroundf((mirror?640-x:x)*16);
    v->v.ob[1]=(short)(ridge_y[level]*16);v->v.ob[2]=(short)lroundf(z*16);
    v->v.tc[0]=(short)lroundf(grass?x*25.6f:p.s*25.6f);
    v->v.tc[1]=(short)lroundf(grass?z*25.6f:ridge_t[level]);
    v->n.n[0]=level>=4?0:(signed char)lroundf(p.nx*(mirror?81:-81));
    v->n.n[1]=level>=4?120:88;
    v->n.n[2]=level>=4?0:(signed char)lroundf(-p.nz*81);v->n.a=232;
}
static void ridge_quad(RidgeMesh* m,RidgePath a,RidgePath b,int level,int mirror) {
    Vtx* v=m->vertices+m->nv;m->nv+=4;
    ridge_vertex(v,a,level,mirror,level==4);ridge_vertex(v+1,b,level,mirror,level==4);
    ridge_vertex(v+2,b,level+1,mirror,level==4);ridge_vertex(v+3,a,level+1,mirror,level==4);
}
static void ridge_emit(RidgeMesh* m,Gfx** pp,int start,int mirror) {
    /* Seven quads per native vertex-cache batch instead of a draw per quad. */
    for(int first=start;first<m->nv;first+=28) {
        int count=m->nv-first;if(count>28)count=28;
        int triangles=count/2,indices[18][3]={{0}};
        for(int q=0;q<count/4;q++) {
            int b=q*4;
            int a[2][3]={{b,b+1,b+2},{b,b+2,b+3}};
            for(int t=0;t<2;t++) {
                indices[q*2+t][0]=a[t][0];
                indices[q*2+t][1]=a[t][mirror?2:1];
                indices[q*2+t][2]=a[t][mirror?1:2];
            }
        }
        gSPVertex((*pp)++,m->vertices+first,count,0);
        gSPNTrianglesInit_5b((*pp)++,triangles,
            indices[0][0],indices[0][1],indices[0][2],
            indices[1][0],indices[1][1],indices[1][2],
            indices[2][0],indices[2][1],indices[2][2]);
        for(int t=3;t<triangles;t+=4) {
            u32 a=gsSPNTriangleData1(indices[t][0],indices[t][1],indices[t][2],0);
            u32 b=gsSPNTriangleData1(indices[t+1][0],indices[t+1][1],indices[t+1][2],0);
            u32 c=gsSPNTriangleData1(indices[t+2][0],indices[t+2][1],indices[t+2][2],0);
            u32 d=gsSPNTriangleData1(indices[t+3][0],indices[t+3][1],indices[t+3][2],0);
            Gfx* packet=(*pp)++;
            packet->words.w0=(d<<17)|(c<<2)|(b>>13);
            packet->words.w1=(b<<19)|(a<<4);
        }
    }
}
static void ridge_build(int cap,int mirror) {
    RidgeMesh* m=&ridge_mesh[cap][mirror];Gfx* g=m->commands;
    RidgePath path[32];int n=0;
    if(cap) {
        for(int i=0;i<=5;i++)path[n++]=ridge_path(i*80,1);
        for(int i=1;i<=8;i++)path[n++]=ridge_path(400+i*251.327412f/8,1);
        for(int i=1;i<=6;i++)path[n++]=ridge_path(651.327412f+i*80,1);
    } else for(int i=0;i<=8;i++)path[n++]=ridge_path(i*80,0);
    gSPDisplayList(g++,ridge_rock_material);
    int grass_start=0;
    for(int level=0;level<5;level++) {
        if(level==4) {
            ridge_emit(m,&g,0,mirror);grass_start=m->nv;
            gSPDisplayList(g++,ridge_grass_material);
        }
        for(int i=0;i<n-1;i++)ridge_quad(m,path[i],path[i+1],level,mirror);
    }
    ridge_emit(m,&g,grass_start,mirror);
    /* Plateau behind the rim. Texture coordinates use world-space density. */
    Vtx* v=m->vertices+m->nv;m->nv+=4;
    float xs[]={160,640,640,160},zs[]={0,0,cap?400:640,cap?400:640};
    for(int i=0;i<4;i++) {
        memset(v+i,0,sizeof(*v));v[i].v.ob[0]=(short)((mirror?640-xs[i]:xs[i])*16);
        v[i].v.ob[1]=4480;v[i].v.ob[2]=(short)(zs[i]*16);
        v[i].v.tc[0]=(short)(xs[i]*25.6f);v[i].v.tc[1]=(short)(zs[i]*25.6f);
        v[i].n.n[1]=120;v[i].n.a=232;
    }
    gSPVertex(g++,v,4,0);
    if(mirror){gSPNTrianglesInit_5b(g++,2,0,1,2,0,2,3,0,0,0);}
    else {gSPNTrianglesInit_5b(g++,2,0,2,1,0,3,2,0,0,0);}
    gSPEndDisplayList(g++);
}
void* pc_dev_ridge_model(int side,int z) {
    if(!ridge_ready) {
        for(int cap=0;cap<2;cap++)for(int mirror=0;mirror<2;mirror++)ridge_build(cap,mirror);
        ridge_ready=1;
    }
    return ridge_mesh[z==5][side<0].commands;
}
