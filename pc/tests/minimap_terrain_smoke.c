/* Real OpenGL test: atlas geography, target isolation, HUD alpha and marker. */
#include "../src/pc_minimap_gl.c"
PCGXState g_gx;
PCSettings g_pc_settings;
int g_pc_window_w=640,g_pc_window_h=480,g_pc_widescreen_stretch;
void pc_gx_draw_pending(void) {}
void pc_gx_texture_bind_cache_invalidate(void) {}
void pc_gx_viewport_state_invalidate(void) {}
void pc_gx_tev_seq_reset(void) {}
static void require(int ok,const char* message) {
    if(!ok){fprintf(stderr,"FAIL: %s GL=%x\n",message,glGetError());exit(1);}
}
static void pixel(int x,int y,unsigned char* rgb) {glReadPixels(x,y,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);}
int main(void) {
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window=SDL_CreateWindow("Terrain map test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    require(window!=NULL,"hidden GL window");SDL_GLContext context=SDL_GL_CreateContext(window);
    require(context!=NULL && gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress),"GL3.3");
    glViewport(7,9,320,240);glEnable(GL_SCISSOR_TEST);glScissor(3,4,300,200);
    pc_minimap_capture_begin(0);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);pc_minimap_capture_end();
    GLint v[4],s[4],bound;
    glGetIntegerv(GL_VIEWPORT,v);glGetIntegerv(GL_SCISSOR_BOX,s);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&bound);
    require(v[0]==7 && v[1]==9 && v[2]==320 && v[3]==240,"viewport restored");
    require(s[0]==3 && s[1]==4 && glIsEnabled(GL_SCISSOR_TEST),"scissor restored");
    require(bound==0 && g_pc_minimap_capture==0,"scene target restored");
    pc_minimap_capture_begin(BLOCK_X_NUM*PC_MINIMAP_ROWS-1);
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);pc_minimap_capture_end();
    PCMinimapLayout map={0,0,320,240,160,120};
    g_pc_settings.minimap=1;g_pc_settings.minimap_opacity=100;
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    pc_minimap_set_overlay(&map);pc_minimap_present();
    unsigned char rgb[4];pixel(4,476,rgb);require(rgb[0]>250 && rgb[2]<3,"northwest rendered at northwest");
    pixel(636,4,rgb);require(rgb[2]>250 && rgb[0]<3,"southeast rendered at southeast");
    pixel(320,240,rgb);require(rgb[0]>250 && rgb[1]>235 && rgb[2]<125,"player marker");
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);g_pc_settings.minimap_opacity=50;
    pc_minimap_set_overlay(&map);pc_minimap_present();pixel(4,476,rgb);
    require(rgb[0]>=126 && rgb[0]<=132,"opacity applied to terrain");
    glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);pc_minimap_set_overlay(NULL);pc_minimap_present();
    pixel(4,476,rgb);require(rgb[1]==255 && rgb[0]==0,"hidden overlay leaves scene intact");
    require(glGetError()==GL_NO_ERROR,"no GL errors");
    pc_minimap_shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS: real GL terrain atlas geography, framebuffer/viewport/scissor isolation, marker, opacity and hide");
    return 0;
}
