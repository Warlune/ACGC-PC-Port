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
    PCMapSymbol symbols[]={{1.5f,2,PC_MAP_SHOP},{BLOCK_X_NUM/2.0f,2,PC_MAP_POST},{BLOCK_X_NUM-1.5f,2,PC_MAP_STATION}};
    pc_minimap_set_symbols(symbols,3);g_pc_settings.minimap_opacity=10;
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    pc_minimap_set_overlay(&map);pc_minimap_present();
    for(int i=0;i<3;i++) {
        int px=(int)(symbols[i].x/BLOCK_X_NUM*640);
        int py=480-(int)((symbols[i].z-PC_MAP_NORTH_CROP)/(PC_MINIMAP_ROWS-PC_MAP_NORTH_CROP)*480);
        int bright=0;
        for(int y=py-12;y<=py+12;y++)for(int x=px-12;x<=px+12;x++) {
            pixel(x,y,rgb);bright+=rgb[0]>215 && rgb[1]>215 && rgb[2]>215;
        }
        require(bright>3,"landmark glyph remains bright at 10% terrain opacity in each region");
    }
    pc_minimap_set_symbols(NULL,0);
    /* A local viewport must not sample either distant world corner. */
    PCMinimapLayout local=map;
    local.view_x=2;local.view_z=2;local.view_w=3;local.view_h=3;
    g_pc_settings.minimap_opacity=100;
    pc_minimap_set_overlay(&local);pc_minimap_present();pixel(4,476,rgb);
    require(rgb[0]<250 && rgb[2]<250,"local HUD excludes remote world-corner terrain");
    /* Changing the viewport must invalidate cached landmark positions. */
    PCMapSymbol nearby[]={{2.75f,2.75f,PC_MAP_SHOP},{BLOCK_X_NUM-1.5f,2,PC_MAP_STATION}};
    pc_minimap_set_symbols(nearby,2);pc_minimap_set_overlay(&local);pc_minimap_present();
    int local_bright=0;
    for(int y=350;y<370;y++)for(int x=150;x<170;x++) {
        pixel(x,y,rgb);local_bright+=rgb[0]>215 && rgb[1]>215 && rgb[2]>215;
    }
    require(local_bright>3,"local landmark uses local viewport coordinates");
    pc_minimap_set_symbols(NULL,0);pc_minimap_set_overlay(&map);
    /* A high-frequency ground pattern should become a stable blended color. */
    unsigned char pattern[TILE*TILE*4];
    for(int y=0;y<TILE;y++)for(int x=0;x<TILE;x++) {
        int p=(y*TILE+x)*4,high=(x+y)&1;
        pattern[p]=high?255:153;pattern[p+1]=high?204:102;pattern[p+2]=high?51:25;pattern[p+3]=255;
    }
    glBindTexture(GL_TEXTURE_2D,color);glTexSubImage2D(GL_TEXTURE_2D,0,0,7*TILE,TILE,TILE,GL_RGBA,GL_UNSIGNED_BYTE,pattern);
    filter_pending=1;pc_minimap_set_overlay(&map);pc_minimap_present();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,smooth_fbo);
    unsigned char adjacent[4];pixel(20,7*TILE+20,rgb);pixel(21,7*TILE+20,adjacent);
    require(abs(rgb[0]-adjacent[0])<5 && rgb[0]>190 && rgb[0]<218,"busy ground pattern blended");
    /* Water and land must stay distinct at a shoreline, rather than smearing. */
    for(int y=0;y<TILE;y++)for(int x=0;x<TILE;x++) {
        int p=(y*TILE+x)*4;
        pattern[p]=x<32?204:15;pattern[p+1]=x<32?153:60;pattern[p+2]=x<32?38:210;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glBindTexture(GL_TEXTURE_2D,color);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,7*TILE,TILE,TILE,GL_RGBA,GL_UNSIGNED_BYTE,pattern);
    filter_pending=1;pc_minimap_set_overlay(&map);pc_minimap_present();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,smooth_fbo);
    pixel(31,7*TILE+32,rgb);pixel(32,7*TILE+32,adjacent);
    require(rgb[0]>190 && rgb[2]<60 && adjacent[2]>195 && adjacent[0]<30,"shoreline preserved by smoothing");
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);pc_minimap_set_overlay(NULL);pc_minimap_present();
    pixel(4,476,rgb);require(rgb[1]==255 && rgb[0]==0,"hidden overlay leaves scene intact");
    require(glGetError()==GL_NO_ERROR,"no GL errors");
    pc_minimap_shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS: GL geography/state isolation, ground smoothing, preserved shorelines, bright landmark glyphs, marker, opacity and hide");
    return 0;
}
