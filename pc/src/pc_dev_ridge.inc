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
static RidgeMesh ridge_mesh[3][2];
static int ridge_ready;
static const float ridge_depth[]={0,10,30,40,60,160};
static const float ridge_y[]={40,160,160,280,280,280};
static const float ridge_t[]={2048,512,0,-1536,-2048,-4608};
typedef struct {float x,z,nx,nz,s;} RidgePath;
static RidgePath ridge_path(float s,int cap) {
    RidgePath p;
    float turn=cap==2?480:400;
    if(!cap || s<=turn) {p=(RidgePath){0,s,1,0,s};}
    else if(s<turn+251.327412f) {
        float a=(s-turn)/160;
        p=(RidgePath){160-160*cosf(a),turn+160*sinf(a),cosf(a),-sinf(a),s};
    } else {p=(RidgePath){160+s-turn-251.327412f,turn+160,0,-1,s};}
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
        float turn=cap==2?480:400;
        for(int i=0;i<=(cap==2?6:5);i++)path[n++]=ridge_path(i*80,cap);
        for(int i=1;i<=8;i++)path[n++]=ridge_path(turn+i*251.327412f/8,cap);
        for(int i=1;i<=6;i++)path[n++]=ridge_path(turn+251.327412f+i*80,cap);
    } else for(int i=0;i<=8;i++)path[n++]=ridge_path(i*80,0);
    gSPDisplayList(g++,ridge_rock_material);
    int grass_start=0;
    if(cap==2)for(int i=0;i<n-1;i++) {
        int first=m->nv;ridge_quad(m,path[i],path[i+1],0,mirror);
        for(int j=0;j<4;j++)m->vertices[first+j].v.ob[1]=(j<2?-40:40)*16;
    }
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
    float xs[]={160,640,640,160},zs[]={0,0,cap==2?480:cap?400:640,cap==2?480:cap?400:640};
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
        for(int cap=0;cap<3;cap++)for(int mirror=0;mirror<2;mirror++)ridge_build(cap,mirror);
        ridge_ready=1;
    }
    return ridge_mesh[z==5][side<0].commands;
}

/* Original-scale outer walls continue through the beach and ocean rows.
 * Add an 80-unit rock foundation down to the sea floor; the native cliff
 * section itself is unchanged. Matching native collision comes from the
 * generator's straight border combination, not a resampled coastal cap. */
extern Gfx grd_s_e2_1_model[],grd_s_e3_1_model[];
static Vtx coast_foundation[2][4];
static Gfx coast_walls[2][12];
void* pc_dev_outer_coast_model(int bx,int bz) {
    if(g_pc_dev_east && Save_Get(scene_no)==SCENE_FG && bz==6 &&
       (bx==ACO_TOWN_X0 || bx==ACO_TOWN_X0+20)) {
        /* The outer mountain turns through a native-scale rounded cap. Its
         * front reaches the same Z=640 foot as the horizontal ocean rim. */
        pc_dev_ridge_model(1,1); /* Initialize the shared modular pieces. */
        return ridge_mesh[2][bx==ACO_TOWN_X0].commands;
    }
    int north=bz==6 && (bx<ACO_TOWN_X0 || bx>ACO_TOWN_X0+20);
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || bz<6 || bz>15 ||
       (bx!=0 && bx!=BLOCK_X_NUM-1 && bz!=15 && !north))return NULL;
    static int initialized;
    if(!initialized) {
        for(int side=0;side<2;side++) {
            Vtx* v=coast_foundation[side];
            for(int i=0;i<4;i++) {
                memset(v+i,0,sizeof(*v));
                v[i].v.ob[0]=side?10240:0;
                v[i].v.ob[1]=(i<2?-40:40)*16;
                v[i].v.ob[2]=(i==1||i==2)?10240:0;
                v[i].v.tc[0]=(i==1||i==2)?16384:0;
                v[i].v.tc[1]=i<2?3072:2048;
                v[i].n.n[0]=side?81:-81;v[i].n.n[1]=88;v[i].n.a=232;
            }
            Gfx* g=coast_walls[side];
            gSPDisplayList(g++,side?grd_s_e2_1_model:grd_s_e3_1_model);
            gSPDisplayList(g++,ridge_rock_material);gSPVertex(g++,v,4,0);
            if(side){gSPNTrianglesInit_5b(g++,2,0,2,1,0,3,2,0,0,0);}
            else {gSPNTrianglesInit_5b(g++,2,0,1,2,0,2,3,0,0,0);}
            gSPEndDisplayList(g++);
        }
        initialized=1;
    }
    if(north) {
        static Gfx rim[12];static Vtx plateau[4];
        static Mtx translate,rotate;static int ready;
        if(!ready) {
            guTranslate(&translate,0,0,10240);guRotate(&rotate,90,0,1,0);
            Gfx* g=rim;
            gSPMatrix(g++,&translate,G_MTX_PUSH|G_MTX_MUL|G_MTX_MODELVIEW);
            gSPMatrix(g++,&rotate,G_MTX_NOPUSH|G_MTX_MUL|G_MTX_MODELVIEW);
            gSPDisplayList(g++,coast_walls[0]);
            gSPDisplayList(g++,ridge_rock_material);gSPDisplayList(g++,ridge_grass_material);
            for(int i=0;i<4;i++) {
                Vtx* v=plateau+i;memset(v,0,sizeof(*v));
                v->v.ob[0]=(i==1||i==2)?10240:2560;v->v.ob[1]=4480;v->v.ob[2]=i>=2?10240:0;
                v->v.tc[0]=(i==1||i==2)?12288:0;v->v.tc[1]=i>=2?16384:0;
                v->n.n[1]=120;v->n.a=232;
            }
            gSPVertex(g++,plateau,4,0);gSPNTrianglesInit_5b(g++,2,0,2,1,0,3,2,0,0,0);
            gSPPopMatrix(g++,G_MTX_MODELVIEW);gSPEndDisplayList(g++);ready=1;
        }
        return rim;
    }
    if(bz==15 && bx>0 && bx<BLOCK_X_NUM-1) {
        static Gfx bottom[16];static Vtx plateau[12];
        static Mtx translate,rotate;static int bottom_ready;
        if(!bottom_ready) {
            guTranslate(&translate,10240,0,0);
            guRotate(&rotate,-90,0,1,0);
            Gfx* g=bottom;
            gSPMatrix(g++,&translate,G_MTX_PUSH|G_MTX_MUL|G_MTX_MODELVIEW);
            gSPMatrix(g++,&rotate,G_MTX_NOPUSH|G_MTX_MUL|G_MTX_MODELVIEW);
            gSPDisplayList(g++,coast_walls[0]);
            gSPDisplayList(g++,ridge_rock_material);gSPDisplayList(g++,ridge_grass_material);
            /* Original-scale grass pieces continue behind the southern rim. */
            for(int piece=0;piece<3;piece++) {
                Vtx* v=plateau+piece*4;
                for(int i=0;i<4;i++) {
                    memset(v+i,0,sizeof(*v));
                    v[i].v.ob[0]=(160+piece*560+((i==1||i==2)?560:0))*16;
                    v[i].v.ob[1]=280*16;v[i].v.ob[2]=(i>=2?640:0)*16;
                    v[i].v.tc[0]=(i==1||i==2)?14336:0;v[i].v.tc[1]=i>=2?16384:0;
                    v[i].n.n[1]=120;v[i].n.a=232;
                }
                gSPVertex(g++,v,4,0);gSPNTrianglesInit_5b(g++,2,0,2,1,0,3,2,0,0,0);
            }
            gSPPopMatrix(g++,G_MTX_MODELVIEW);gSPEndDisplayList(g++);bottom_ready=1;
        }
        return bottom;
    }
    return coast_walls[bx==0];
}

/* All coast overrides are solid mountain geometry. */
void* pc_dev_outer_coast_water_model(int bx,int bz) {
    (void)bx;(void)bz;return NULL;
}

/* Continue the inaccessible plateau behind the outer cliffs. Each 160-unit
 * piece keeps the native grass UV density; two extra acres cover the widened
 * camera without changing the field size, cliff shape or collision. */
void* pc_dev_outer_ground_model(int bx,int bz,void* original) {
    static Vtx vertices[2][44];
    static Gfx ground[2][24],wrapped[16][2][5];
    static int ready;
    int left=bz<=5?ACO_TOWN_X0:0,right=bz<=5?ACO_TOWN_X0+20:BLOCK_X_NUM-1;
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || bz<1 || bz>15 ||
       (bz==6 && (bx<ACO_TOWN_X0 || bx>ACO_TOWN_X0+20)) ||
       (bx!=left && bx!=right))return original;
    if(!ready) {
        for(int side=0;side<2;side++) {
            Gfx* g=ground[side];
            for(int piece=0;piece<11;piece++) {
                Vtx* v=vertices[side]+piece*4;
                int start=(side?160:-1280)+piece*160;
                for(int i=0;i<4;i++) {
                    memset(v+i,0,sizeof(*v));
                    v[i].v.ob[0]=(start+((i==1||i==2)?160:0))*16;
                    v[i].v.ob[1]=280*16;
                    v[i].v.ob[2]=(i>=2?640:0)*16;
                    v[i].v.tc[0]=(i==1||i==2)?4096:0;
                    v[i].v.tc[1]=i>=2?16384:0;
                    v[i].n.n[1]=120;v[i].n.a=232;
                }
                gSPVertex(g++,v,4,0);
                gSPNTrianglesInit_5b(g++,2,0,2,1,0,3,2,0,0,0);
            }
            gSPEndDisplayList(g++);
        }
        ready=1;
    }
    int side=bx!=left;
    Gfx* g=wrapped[bz][side];
    if(original)gSPDisplayList(g++,original);
    gSPDisplayList(g++,ridge_rock_material);
    gSPDisplayList(g++,ridge_grass_material);
    gSPDisplayList(g++,ground[side]);
    gSPEndDisplayList(g++);
    return wrapped[bz][side];
}
