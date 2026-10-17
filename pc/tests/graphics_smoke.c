/* Driver-backed integration test: exercises real FBO/MSAA resolve, scale,
 * spatial filters and GL state restoration without loading game assets. */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_graphics.h"

SDL_Window* g_pc_window;
int g_pc_window_w=96, g_pc_window_h=72;
PCSettings g_pc_settings;
void pc_gx_draw_pending(void) {}

static void require(int ok, const char* message) {
    if (!ok) { fprintf(stderr,"FAIL: %s (GL=0x%x, SDL=%s)\n",message,glGetError(),SDL_GetError()); exit(1); }
}
static void expect_color(const unsigned char* p) {
    require(abs(p[0]-64)<=2 && abs(p[1]-128)<=2 && abs(p[2]-191)<=2,"resolved/presented color");
}

int main(void) {
    SDL_SetMainReady();
    require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL init");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    g_pc_window=SDL_CreateWindow("Graphics smoke test",0,0,g_pc_window_w,g_pc_window_h,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    require(g_pc_window!=NULL,"window");
    SDL_GLContext context=SDL_GL_CreateContext(g_pc_window);
    require(context!=NULL,"GL context");
    require(gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)!=0,"GL loader");
    g_pc_settings.render_scale=100;
    g_pc_settings.msaa=4;
    g_pc_settings.texture_filtering=1;
    g_pc_settings.anisotropy=16;
    pc_graphics_init();
    GLuint sampler_test;
    glGenTextures(1,&sampler_test);
    glBindTexture(GL_TEXTURE_2D,sampler_test);
    pc_graphics_texture_sampler();
    require(pc_graphics_anisotropy()<=pc_graphics_max_anisotropy(),"anisotropy capability clamp");
    if (pc_graphics_max_anisotropy()>1) {
        GLfloat anisotropy;
        glGetTexParameterfv(GL_TEXTURE_2D,0x84FE,&anisotropy);
        require((int)anisotropy==pc_graphics_anisotropy(),"anisotropic GL sampler state");
    }
    glDeleteTextures(1,&sampler_test);
    int count=0;
    const int scales[]={50,75,100,150,200};
    const int aa[]={0,2,4,8};
    for(int s=0;s<5;s++) for(int a=0;a<4;a++) for(int filter=0;filter<3;filter++) {
        g_pc_settings.render_scale=scales[s];
        g_pc_settings.msaa=aa[a];
        g_pc_settings.upscale_filter=filter;
        g_pc_settings.fxaa=count%2;
        g_pc_settings.sharpness=count%5*25;
        pc_graphics_begin_frame();
        require(g_pc_render_w==g_pc_window_w*scales[s]/100,"internal render width");
        require(pc_graphics_samples()<=pc_graphics_max_samples(),"MSAA capability clamp");
        glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glClearColor(0.25f,0.5f,0.75f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        unsigned char pixel[4];
        pc_graphics_readback_begin();
        glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        expect_color(pixel);
        pc_graphics_readback_end();
        /* Asymmetric scene verifies orientation and complete screen coverage. */
        glEnable(GL_SCISSOR_TEST);
        glScissor(0,0,g_pc_render_w/2,g_pc_render_h/2);
        glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(g_pc_render_w/2,g_pc_render_h/2,g_pc_render_w,g_pc_render_h);
        glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
        /* Simulate restrictive game render state at presentation time. */
        glEnable(GL_SCISSOR_TEST); glScissor(0,0,1,1);
        glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glEnable(GL_CULL_FACE);
        glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
        glViewport(2,3,17,19);
        pc_graphics_present();
        require(glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE),"GL enable state restoration");
        GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);
        require(vp[0]==2&&vp[1]==3&&vp[2]==17&&vp[3]==19,"viewport restoration");
        GLboolean masks[4]; glGetBooleanv(GL_COLOR_WRITEMASK,masks);
        require(!masks[0]&&masks[1]&&!masks[2]&&masks[3],"color mask restoration");
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
        glReadBuffer(GL_BACK);
        glReadPixels(g_pc_window_w/4,g_pc_window_h/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        require(pixel[0]>250&&pixel[1]<3&&pixel[2]<3,"bottom-left orientation and color");
        glReadPixels(g_pc_window_w*3/4,g_pc_window_h*3/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        require(pixel[1]>250&&pixel[0]<3&&pixel[2]<3,"top-right orientation and color");
        glReadPixels(g_pc_window_w/4,g_pc_window_h*3/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        expect_color(pixel);
        pc_graphics_readback_end();
        require(glGetError()==GL_NO_ERROR,"OpenGL error-free pass");
        count++;
    }
    g_pc_window_w=128; g_pc_window_h=96;
    SDL_SetWindowSize(g_pc_window,g_pc_window_w,g_pc_window_h);
    pc_graphics_begin_frame();
    require(g_pc_render_w==256&&g_pc_render_h==192,"target recreation on resize");
    pc_graphics_shutdown();
    require(glGetError()==GL_NO_ERROR,"cleanup");
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(g_pc_window); SDL_Quit();
    printf("PASS: %d driver-backed graphics combinations plus resize and state restoration\n",count);
    return 0;
}
