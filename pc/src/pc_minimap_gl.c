/* Cached orthographic terrain target and final-resolution HUD composite. */
#include "pc_minimap.h"
#include "pc_gx_internal.h"
#include "pc_settings.h"
#include "m_field_make.h"

#define TILE 64
static GLuint fbo, color, depth, program, vao;
static GLint rect_uniform,tint_uniform,textured_uniform,atlas_uniform,screen_uniform;
static int ready, visible;
int g_pc_minimap_capture;
static PCMinimapLayout overlay;
static GLint saved_fbo, saved_read_fbo, saved_viewport[4], saved_scissor[4];
static GLboolean saved_scissor_on;
static int saved_wide;
static GLfloat saved_clear[4];

void pc_minimap_set_overlay(const PCMinimapLayout* layout) {
    visible=layout!=NULL;if(layout)overlay=*layout;
}
void pc_minimap_reset(void) {ready=0;}

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
    pc_gx_draw_pending();
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
    pc_gx_draw_pending();g_pc_minimap_capture=0;g_pc_widescreen_stretch=saved_wide;
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
    const char* fs="#version 330 core\nin vec2 uv;out vec4 outColor;uniform sampler2D atlas;"
        "uniform vec4 tint;uniform int textured;"
        "void main(){outColor=textured!=0?vec4(texture(atlas,uv).rgb,tint.a):tint;}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
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
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,g_pc_window_w,g_pc_window_h);
    glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(program);glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D,color);glUniform1i(atlas_uniform,0);
    glUniform2f(screen_uniform,g_pc_window_w,g_pc_window_h);
    float sx=g_pc_window_w/320.0f,sy=g_pc_window_h/240.0f;
    float x=overlay.left*sx,y=overlay.top*sy,w=overlay.width*sx,h=overlay.height*sy;
    float a=fminf(g_pc_settings.minimap_opacity,100)/100.0f;
    quad(x-2*sy,y-2*sy,w+4*sy,h+4*sy,0,0.03f,0.05f,0.06f,a);
    quad(x,y,w,h,1,1,1,1,a);
    /* Town boundaries aid orientation without replacing any terrain. */
    if(BLOCK_X_NUM>7)for(int col=7;col<BLOCK_X_NUM;col+=7)
        quad(x+w*col/BLOCK_X_NUM,y,sy*0.35f,h,0,1,1,1,a*0.35f);
    float px=overlay.marker_x*sx,py=overlay.marker_y*sy;
    quad(px-2*sy,py-2*sy,4*sy,4*sy,0,0.04f,0.06f,0.08f,a);
    quad(px-sy,py-sy,2*sy,2*sy,0,1,0.95f,0.45f,a);
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
    fbo=color=depth=program=vao=0;ready=visible=0;
}
