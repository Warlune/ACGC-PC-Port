/* Cached orthographic terrain target and final-resolution HUD composite. */
#include "pc_minimap.h"
#include "pc_minimap_symbols.h"
#include "pc_minimap_style.h"
#include "pc_gx_internal.h"
#include "pc_settings.h"
#include "m_field_make.h"

#define TILE 64
static GLuint fbo, color, depth, program, vao;
static GLuint smooth_fbo,smooth_color,symbol_fbo,symbol_color;
static int filter_pending,filtered_ready,capture_tile;
static int symbol_count,symbol_dirty=1,symbol_width,symbol_height;
static PCMapSymbol map_symbols[PC_MAP_MAX_SYMBOLS];
void pc_minimap_set_symbols(const PCMapSymbol* items,int count) {
    if(!items || count<0)count=0;
    if(count>PC_MAP_MAX_SYMBOLS)count=PC_MAP_MAX_SYMBOLS;
    if(count!=symbol_count || (count && memcmp(map_symbols,items,count*sizeof(*items)))) {
        if(count)memcpy(map_symbols,items,count*sizeof(*items));
        symbol_count=count;symbol_dirty=1;
    }
}
static GLint view_uniform;
static GLint rect_uniform,tint_uniform,textured_uniform,atlas_uniform,screen_uniform;
static int ready, visible;
int g_pc_minimap_capture;
static PCMinimapLayout overlay;
static GLint saved_fbo, saved_read_fbo, saved_viewport[4], saved_scissor[4];
static GLboolean saved_scissor_on;
static int saved_wide;
static GLfloat saved_clear[4];

void pc_minimap_set_overlay(const PCMinimapLayout* layout) {
    visible=layout!=NULL;if(layout) {
        if(layout->view_x!=overlay.view_x || layout->view_z!=overlay.view_z ||
           layout->view_w!=overlay.view_w || layout->view_h!=overlay.view_h)symbol_dirty=1;
        overlay=*layout;
    }
}
void pc_minimap_reset(void) {ready=filtered_ready=filter_pending=0;symbol_dirty=1;}

static void create_target(void) {
    GLint texture,renderbuffer;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,BLOCK_X_NUM*TILE,PC_MINIMAP_ROWS*TILE,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,texture);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,BLOCK_X_NUM*TILE,PC_MINIMAP_ROWS*TILE);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr,"[Minimap] Terrain target incomplete\n");
}
void pc_minimap_capture_begin(int tile) {
    pc_gx_draw_pending();capture_tile=tile;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&saved_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&saved_read_fbo);
    glGetIntegerv(GL_VIEWPORT,saved_viewport);glGetIntegerv(GL_SCISSOR_BOX,saved_scissor);
    glGetFloatv(GL_COLOR_CLEAR_VALUE,saved_clear);saved_scissor_on=glIsEnabled(GL_SCISSOR_TEST);
    saved_wide=g_pc_widescreen_stretch;g_pc_widescreen_stretch=1;
    if(!fbo)create_target();else glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearColor(0.06f,0.18f,0.28f,1);glClearDepth(1);
    if(!ready) {glDisable(GL_SCISSOR_TEST);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);ready=1;}
    int x=(tile%BLOCK_X_NUM)*TILE,y=(PC_MINIMAP_ROWS-1-tile/BLOCK_X_NUM)*TILE;
    glViewport(x,y,TILE,TILE);glEnable(GL_SCISSOR_TEST);glScissor(x,y,TILE,TILE);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    g_pc_minimap_capture=1;
    pc_gx_viewport_state_invalidate();DIRTY(PC_GX_DIRTY_ALL);
}
void pc_minimap_capture_end(void) {
    pc_gx_draw_pending();
    if(capture_tile==BLOCK_X_NUM*PC_MINIMAP_ROWS-1)filter_pending=1;
    g_pc_minimap_capture=0;g_pc_widescreen_stretch=saved_wide;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,saved_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,saved_read_fbo);
    glViewport(saved_viewport[0],saved_viewport[1],saved_viewport[2],saved_viewport[3]);
    glScissor(saved_scissor[0],saved_scissor[1],saved_scissor[2],saved_scissor[3]);
    if(!saved_scissor_on)glDisable(GL_SCISSOR_TEST);
    glClearColor(saved_clear[0],saved_clear[1],saved_clear[2],saved_clear[3]);
    pc_gx_viewport_state_invalidate();pc_gx_texture_bind_cache_invalidate();DIRTY(PC_GX_DIRTY_ALL);
}
static GLuint shader(GLenum type,const char* source) {
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,NULL);glCompileShader(s);
    GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),NULL,log);fprintf(stderr,"[Minimap] %s\n",log);}
    return s;
}
static void create_program(void) {
    const char* vs="#version 330 core\nuniform vec4 rect;uniform vec2 screen;out vec2 uv;"
        "void main(){vec2 p[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));"
        "vec2 q=p[gl_VertexID];uv=vec2(q.x,1-q.y);vec2 xy=rect.xy+q*rect.zw;"
        "gl_Position=vec4(xy.x/screen.x*2-1,1-xy.y/screen.y*2,0,1);}";
    const char* fs=
        "#version 330 core\n"
        "in vec2 uv;out vec4 outColor;uniform sampler2D atlas;uniform vec4 tint;uniform int textured;uniform vec4 mapView;\n"
        "float rect(vec2 p,vec2 a,vec2 b){return step(a.x,p.x)*step(p.x,b.x)*step(a.y,p.y)*step(p.y,b.y);}\n"
        "float line(vec2 p,vec2 a,vec2 b,float w){vec2 d=b-a;float t=clamp(dot(p-a,d)/dot(d,d),0,1);return 1-smoothstep(w,w+0.02,length(p-a-t*d));}\n"
        "float ring(vec2 p,vec2 c,float r,float w){return 1-smoothstep(w,w+0.02,abs(length(p-c)-r));}\n"
        "bool water(vec3 c){return c.b>c.r*1.15 && c.b>c.g*1.05;}\n"
        "vec3 soften(vec2 p){vec2 texel=1.0/vec2(textureSize(atlas,0));vec3 center=texture(atlas,p).rgb,sum=vec3(0);float total=0;float k[5]=float[5](1,4,6,4,1);bool wet=water(center);\n"
        "for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){vec3 c=texture(atlas,p+vec2(x,y)*texel).rgb;float w=k[x+2]*k[y+2];if(water(c)!=wet)w=0;sum+=c*w;total+=w;}return sum/max(total,1);}\n"
        "float glyph(vec2 p,int kind){float a=0;\n"
        "if(kind==0||kind==1){a=max(rect(p,vec2(.26,.43),vec2(.74,.77)),float(p.y>.20&&p.y<.48&&abs(p.x-.5)<(p.y-.17)*1.12));a*=1-rect(p,vec2(.44,.59),vec2(.57,.79));}\n"
        "else if(kind==2){a=max(rect(p,vec2(.26,.40),vec2(.74,.77)),ring(p,vec2(.5,.38),.15,.035)*step(p.y,.41));a*=1-rect(p,vec2(.34,.49),vec2(.66,.69));}\n"
        "else if(kind==3){a=rect(p,vec2(.22,.32),vec2(.78,.72))*(1-rect(p,vec2(.28,.38),vec2(.72,.66)));a=max(a,max(line(p,vec2(.24,.35),vec2(.5,.55),.025),line(p,vec2(.5,.55),vec2(.76,.35),.025)));}\n"
        "else if(kind==4){a=max(rect(p,vec2(.28,.24),vec2(.72,.67)),max(ring(p,vec2(.35,.75),.055,.035),ring(p,vec2(.65,.75),.055,.035)));a*=1-rect(p,vec2(.36,.32),vec2(.64,.48));}\n"
        "else if(kind==5){a=max(rect(p,vec2(.20,.73),vec2(.80,.80)),float(p.y>.20&&p.y<.42&&abs(p.x-.5)<(p.y-.16)*1.55));for(int i=0;i<3;i++)a=max(a,rect(p,vec2(.28+float(i)*.18,.46),vec2(.36+float(i)*.18,.74)));}\n"
        "else if(kind==6){a=float(p.y>.22&&p.y<.72&&abs(p.x-.5)<min(.27,(.84-p.y)*.65));a*=1-rect(p,vec2(.46,.36),vec2(.54,.61));a=max(a,rect(p,vec2(.36,.45),vec2(.64,.53)));}\n"
        "else if(kind==7){a=max(ring(p,vec2(.32,.69),.10,.035),ring(p,vec2(.67,.69),.10,.035));a=max(a,max(line(p,vec2(.38,.63),vec2(.72,.23),.035),line(p,vec2(.61,.63),vec2(.27,.23),.035)));}\n"
        "else if(kind==8){a=max(ring(p,vec2(.5,.52),.24,.045),rect(p,vec2(.32,.49),vec2(.68,.55)));}\n"
        "else if(kind==9){a=max(ring(p,vec2(.5,.28),.07,.03),line(p,vec2(.5,.34),vec2(.5,.76),.035));a=max(a,max(line(p,vec2(.27,.6),vec2(.5,.77),.035),line(p,vec2(.5,.77),vec2(.73,.6),.035)));a=max(a,line(p,vec2(.3,.47),vec2(.7,.47),.035));}\n"
        "else if(kind==10){a=max(rect(p,vec2(.3,.36),vec2(.7,.78))*(1-rect(p,vec2(.37,.43),vec2(.63,.7))),rect(p,vec2(.24,.28),vec2(.76,.36)));}\n"
        "else if(kind==11){a=float(p.y>.23&&p.y<.78&&abs(p.x-.5)<(p.y-.20)*.56);a*=1-float(p.y>.50&&abs(p.x-.5)<(p.y-.48)*.32);}\n"
        "else if(kind==12){a=max(rect(p,vec2(.38,.29),vec2(.62,.76)),rect(p,vec2(.28,.24),vec2(.72,.36)));a=max(a,max(line(p,vec2(.2,.24),vec2(.07,.18),.025),line(p,vec2(.8,.24),vec2(.93,.18),.025)));}\n"
        "return clamp(a,0,1);}\n"
        "void main(){if(textured==0){outColor=tint;return;}\n"
        "if(textured==1){vec4 c=texture(atlas,(mapView.xy+uv*mapView.zw));outColor=vec4(c.rgb,c.a*tint.a);return;}\n"
        "if(textured==2){outColor=vec4(soften(uv),1);return;}\n"
        "if(textured==3){vec4 c=texture(atlas,uv);outColor=vec4(c.rgb,c.a*tint.a);return;}\n"
        "vec2 p=vec2(uv.x,1-uv.y);float d=length(p-vec2(.5));float coverage=1-smoothstep(.46,.49,d);if(coverage<=0)discard;\n"
        "vec3 base=d>.41?vec3(.025,.04,.055):tint.rgb;outColor=vec4(mix(base,vec3(1),glyph(p,textured-10)),coverage*tint.a);}\n"
    ;
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    view_uniform=glGetUniformLocation(program,"mapView");
    rect_uniform=glGetUniformLocation(program,"rect");
    tint_uniform=glGetUniformLocation(program,"tint");
    textured_uniform=glGetUniformLocation(program,"textured");
    atlas_uniform=glGetUniformLocation(program,"atlas");
    screen_uniform=glGetUniformLocation(program,"screen");
    glDeleteShader(v);glDeleteShader(f);glGenVertexArrays(1,&vao);
}
static void quad(float x,float y,float w,float h,int textured,float r,float g,float b,float a) {
    glUniform4f(rect_uniform,x,y,w,h);
    glUniform4f(tint_uniform,r,g,b,a);
    glUniform1i(textured_uniform,textured);glDrawArrays(GL_TRIANGLES,0,6);
}
static void color_target(GLuint* target,GLuint* texture,int w,int h) {
    if(!*texture)glGenTextures(1,texture);
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(!*target)glGenFramebuffers(1,target);
    glBindFramebuffer(GL_FRAMEBUFFER,*target);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,*texture,0);
}
static void prepare_smooth(void) {
    if(!filter_pending)return;
    int w=BLOCK_X_NUM*TILE,h=PC_MINIMAP_ROWS*TILE;
    if(!smooth_fbo)color_target(&smooth_fbo,&smooth_color,w,h);
    else glBindFramebuffer(GL_FRAMEBUFFER,smooth_fbo);
    glViewport(0,0,w,h);glDisable(GL_BLEND);glBindTexture(GL_TEXTURE_2D,color);
    glUniform2f(screen_uniform,w,h);quad(0,0,w,h,2,1,1,1,1);
    filtered_ready=1;filter_pending=0;
}
static void prepare_symbols(int w,int h,float pixel_scale) {
    if(w!=symbol_width || h!=symbol_height) {
        color_target(&symbol_fbo,&symbol_color,w,h);
        symbol_width=w;symbol_height=h;symbol_dirty=1;
    }
    if(!symbol_dirty)return;
    glBindFramebuffer(GL_FRAMEBUFFER,symbol_fbo);glViewport(0,0,w,h);
    glDisable(GL_BLEND);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D,0);glUniform2f(screen_uniform,w,h);
    struct Placed {float x,y,size,anchor_x,anchor_y;int kind;} placed[PC_MAP_MAX_SYMBOLS];
    int n=0;
    /* Landmarks first, player houses next, villager houses last. The nearest
     * free badge position wins; leaders retain the real geographical anchor. */
    static const float offsets[][2]={{0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1},{2,0},{-2,0},{0,2},{0,-2}};
    for(int pass=0;pass<3;pass++)for(int i=0;i<symbol_count;i++) {
        PCMapSymbol s=map_symbols[i];
        if(s.kind<0 || s.kind>=PC_MAP_SYMBOL_TYPES)continue;
        if((s.kind>=PC_MAP_SHOP?0:s.kind==PC_MAP_HOME?1:2)!=pass)continue;
        float vx=overlay.view_w>0?overlay.view_x:0,vz=overlay.view_h>0?overlay.view_z:PC_MAP_NORTH_CROP;
        float vw=overlay.view_w>0?overlay.view_w:BLOCK_X_NUM,vh=overlay.view_h>0?overlay.view_h:PC_MINIMAP_ROWS-PC_MAP_NORTH_CROP;
        if(s.z<vz || s.z>=vz+vh || s.x<vx || s.x>=vx+vw)continue;
        float size=fminf(fmaxf(pixel_scale*3.4f,12),22);
        if(s.kind==PC_MAP_VILLAGER)size*=0.78f;
        float ax=(s.x-vx)/vw*w,ay=(s.z-vz)/vh*h;
        float best=1e30f,bx=ax,by=ay;
        for(int candidate=0;candidate<(int)(sizeof(offsets)/sizeof(offsets[0]));candidate++) {
            float cx=fminf(fmaxf(ax+offsets[candidate][0]*(size+2),size/2),w-size/2);
            float cy=fminf(fmaxf(ay+offsets[candidate][1]*(size+2),size/2),h-size/2);
            float score=(cx-ax)*(cx-ax)+(cy-ay)*(cy-ay);
            for(int j=0;j<n;j++)if(fabsf(placed[j].x-cx)<(size+placed[j].size)/2+1 &&
                fabsf(placed[j].y-cy)<(size+placed[j].size)/2+1)score+=100000;
            if(score<best){best=score;bx=cx;by=cy;}
        }
        placed[n++]=(struct Placed){bx,by,size,ax,ay,s.kind};
    }
    for(int i=0;i<n;i++) {
        struct Placed p=placed[i];
        if(fabsf(p.x-p.anchor_x)+fabsf(p.y-p.anchor_y)<4)continue;
        quad(fminf(p.x,p.anchor_x),p.anchor_y,fmaxf(fabsf(p.x-p.anchor_x),1),1,0,0.9f,0.95f,1,0.7f);
        quad(p.x,fminf(p.y,p.anchor_y),1,fmaxf(fabsf(p.y-p.anchor_y),1),0,0.9f,0.95f,1,0.7f);
        quad(p.anchor_x-1,p.anchor_y-1,2,2,0,1,1,1,1);
    }
    static const float colors[PC_MAP_SYMBOL_TYPES][3]={
        {.65f,.43f,.10f},{.22f,.35f,.46f},{.12f,.47f,.25f},{.14f,.35f,.65f},
        {.58f,.22f,.18f},{.46f,.27f,.61f},{.08f,.42f,.52f},{.59f,.22f,.46f},
        {.12f,.43f,.43f},{.12f,.35f,.55f},{.40f,.39f,.20f},{.56f,.34f,.15f},{.53f,.29f,.14f}
    };
    for(int i=0;i<n;i++) {
        struct Placed p=placed[i];const float* c=colors[p.kind];
        quad(p.x-p.size/2,p.y-p.size/2,p.size,p.size,10+p.kind,c[0],c[1],c[2],1);
    }
    symbol_dirty=0;
}
static void export_planning_map(void) {
#ifdef ACO_DEV_EAST
    static int exported;
    const char* path=getenv("ACO_MAP_EXPORT");
    if(!path || exported || !filtered_ready)return;
    int w=BLOCK_X_NUM*TILE,h=PC_MINIMAP_ROWS*TILE;
    unsigned char* pixels=malloc(w*h*4);
    if(!pixels)return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,smooth_fbo);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
    if(surface) {if(SDL_SaveBMP(surface,path)==0)exported=1;SDL_FreeSurface(surface);}
    if(exported) {
        char* labels=malloc(strlen(path)+6);
        if(labels) {
            sprintf(labels,"%s.json",path);
            FILE* f=fopen(labels,"w");
            if(f) {
                fprintf(f,"[\n");
                for(int i=0;i<symbol_count;i++)fprintf(f,"%s{\"x\":%g,\"z\":%g,\"kind\":%d}",
                    i?",\n":"",map_symbols[i].x,map_symbols[i].z,map_symbols[i].kind);
                fprintf(f,"\n]\n");fclose(f);
            }
            free(labels);
        }
    }
    free(pixels);
#endif
}

void pc_minimap_present(void) {
    if(!visible || !ready || !g_pc_settings.minimap || g_pc_settings.minimap_opacity<=0)return;
    GLint old_fbo,old_read_fbo,viewport[4],old_active,old_texture,old_program,old_vao;
    GLint src_rgb,dst_rgb,src_alpha,dst_alpha,equation_rgb,equation_alpha;
    GLboolean mask[4],depth_on=glIsEnabled(GL_DEPTH_TEST),blend_on=glIsEnabled(GL_BLEND);
    GLboolean scissor_on=glIsEnabled(GL_SCISSOR_TEST),cull_on=glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&old_fbo);glGetIntegerv(GL_VIEWPORT,viewport);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old_read_fbo);
    glGetIntegerv(GL_CURRENT_PROGRAM,&old_program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&old_vao);
    glGetIntegerv(GL_BLEND_SRC_RGB,&src_rgb);glGetIntegerv(GL_BLEND_DST_RGB,&dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&src_alpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&equation_rgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equation_alpha);
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetIntegerv(GL_ACTIVE_TEXTURE,&old_active);
    glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);
    if(!program)create_program();
    glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(program);glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D,color);glUniform1i(atlas_uniform,0);
    float sx=g_pc_window_w/320.0f,sy=g_pc_window_h/240.0f;
    float x=overlay.left*sx,y=overlay.top*sy,w=overlay.width*sx,h=overlay.height*sy;
    prepare_smooth();export_planning_map();prepare_symbols((int)ceilf(w),(int)ceilf(h),sy);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,g_pc_window_w,g_pc_window_h);
    glEnable(GL_BLEND);glUniform2f(screen_uniform,g_pc_window_w,g_pc_window_h);
    glBindTexture(GL_TEXTURE_2D,filtered_ready?smooth_color:color);
    float a=fminf(g_pc_settings.minimap_opacity,100)/100.0f;
    quad(x-2*sy,y-2*sy,w+4*sy,h+4*sy,0,0.03f,0.05f,0.06f,a);
    float vx=overlay.view_w>0?overlay.view_x:0,vz=overlay.view_h>0?overlay.view_z:PC_MAP_NORTH_CROP;
    float vw=overlay.view_w>0?overlay.view_w:BLOCK_X_NUM,vh=overlay.view_h>0?overlay.view_h:PC_MINIMAP_ROWS-PC_MAP_NORTH_CROP;
    glUniform4f(view_uniform,vx/BLOCK_X_NUM,1-(vz+vh)/PC_MINIMAP_ROWS,vw/BLOCK_X_NUM,vh/PC_MINIMAP_ROWS);
    quad(x,y,w,h,1,1,1,1,a);
    /* Town boundaries aid orientation without replacing any terrain. */
    if(BLOCK_X_NUM>7)for(int col=ACO_TOWN_X0+7;col<ACO_TOWN_X0+21;col+=7)
        if(col>vx && col<vx+vw)quad(x+w*(col-vx)/vw,y,sy*0.35f,h,0,1,1,1,a*0.35f);
    /* Symbols stay legible while the terrain remains faint. Fully transparent
     * or disabled maps still hide everything through the guard above. */
    glBindTexture(GL_TEXTURE_2D,symbol_color);
    quad(x,y,w,h,3,1,1,1,fmaxf(a,0.94f));
    float px=overlay.marker_x*sx,py=overlay.marker_y*sy;
    quad(px-2*sy,py-2*sy,4*sy,4*sy,0,0.04f,0.06f,0.08f,0.98f);
    quad(px-sy,py-sy,2*sy,2*sy,0,1,0.95f,0.45f,1);
    glBindTexture(GL_TEXTURE_2D,old_texture);glActiveTexture(old_active);
    glBindVertexArray(old_vao);glUseProgram(old_program);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,old_fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,old_read_fbo);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);
    glBlendFuncSeparate(src_rgb,dst_rgb,src_alpha,dst_alpha);
    glBlendEquationSeparate(equation_rgb,equation_alpha);
    if(depth_on)glEnable(GL_DEPTH_TEST);
    if(!blend_on)glDisable(GL_BLEND);
    if(scissor_on)glEnable(GL_SCISSOR_TEST);
    if(cull_on)glEnable(GL_CULL_FACE);
    pc_gx_texture_bind_cache_invalidate();pc_gx_viewport_state_invalidate();
    g_gx.current_shader=0;DIRTY(PC_GX_DIRTY_ALL);
    visible=0;
}
void pc_minimap_shutdown(void) {
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    glDeleteProgram(program);glDeleteVertexArrays(1,&vao);
    glDeleteFramebuffers(1,&smooth_fbo);glDeleteTextures(1,&smooth_color);
    glDeleteFramebuffers(1,&symbol_fbo);glDeleteTextures(1,&symbol_color);
    smooth_fbo=smooth_color=symbol_fbo=symbol_color=0;
    filtered_ready=filter_pending=symbol_width=symbol_height=symbol_count=0;symbol_dirty=1;
    fbo=color=depth=program=vao=0;ready=visible=0;
}
