/* Real-driver cache test with a deterministic pack fixture. The game decoder,
 * texture cache, selection logic and GPU texture lifetimes run unchanged. */
#include "../src/pc_gx_texture.c"

PCGXState g_gx;
PCSettings g_pc_settings;
SDL_Window* g_pc_window;
int g_pc_profile_enabled;
static GLuint replacement;
static int lookups;
static int pack_available=1;
static const void* unmatched;
void pc_gx_draw_pending(void) {}
void pc_gx_flush_if_begin_complete(void) {}
void pc_gx_texture_bind_cache_invalidate(void) {}
void pc_gx_tev_seq_reset(void) {}
GLuint pc_gx_efb_capture_find(u32 key) { (void)key; return 0; }
void pc_graphics_texture_sampler(void) {}
int pc_graphics_anisotropy(void) { return 1; }
void pc_profiler_add_count_texture_bind_slow(void) {}
void pc_profiler_add_time_slow(PCProfilerTimer timer, Uint64 start) { (void)timer; (void)start; }
int pc_texture_pack_active(void) { return pack_available; }
GLuint pc_texture_pack_lookup(const void* data,int bytes,int w,int h,unsigned fmt,
    const void* pal,int entries,int be,int* ow,int* oh) {
    (void)bytes;(void)fmt;(void)pal;(void)entries;(void)be;
    lookups++; *ow=w*4;*oh=h*4;
    return data==unmatched ? 0 : replacement;
}
static void require(int ok,const char* message) {
    if(!ok) {fprintf(stderr,"FAIL: %s (GL=0x%x)\n",message,glGetError());exit(1);}
}
static void expect_texture(GLuint tex,int width,int r,int b) {
    GLint actual;
    unsigned char pixels[32*32*4];
    glBindTexture(GL_TEXTURE_2D,tex);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&actual);
    require(actual==width,"original/replacement dimensions");
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    require(pixels[0]==r&&pixels[1]==0&&pixels[2]==b&&pixels[3]==255,"original/replacement pixel data");
}
int main(void) {
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL initialization");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    g_pc_window=SDL_CreateWindow("Texture comparison test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    SDL_GLContext context=SDL_GL_CreateContext(g_pc_window);
    require(context!=NULL&&gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress),"OpenGL context");
    unsigned char blue[32*32*4];
    for(int i=0;i<32*32;i++){blue[i*4]=0;blue[i*4+1]=0;blue[i*4+2]=255;blue[i*4+3]=255;}
    glGenTextures(1,&replacement);glBindTexture(GL_TEXTURE_2D,replacement);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,blue);
    unsigned char red[8*8*2],other[8*8*2];
    for(int i=0;i<sizeof(red);i+=2){red[i]=0xf8;red[i+1]=0;other[i]=0xf8;other[i+1]=0;}
    unmatched=other;
    u32 object[TEXOBJ_SIZE];
    g_pc_settings.texture_filtering=1;g_pc_settings.texture_replacements=1;
    pc_gx_texture_init();
    GXInitTexObj(object,red,8,8,GX_TF_RGB565,1,0,0);
    GXLoadTexObj(object,0);
    require(tex_cache_count==1&&lookups==1,"one resident original/pack pair");
    GLuint original=tex_cache[0].gl_tex;
    require(original!=replacement&&glIsTexture(original),"original retained when pack initially selected");
    expect_texture(original,8,255,0);expect_texture(g_gx.gl_textures[0],32,0,255);
    for(int i=0;i<20;i++) {
        pc_gx_texture_set_replacements(i%2);
        require(g_gx.gl_textures[0]==(i%2?replacement:original),"bound slot changes immediately");
        GXLoadTexObj(object,0);
        require(tex_cache_count==1&&lookups==1&&tex_cache_misses==1,"warm toggle avoids reload/decode");
    }
    object[TEXOBJ_WRAP_S]=2;object[TEXOBJ_MIN_FILTER]=0;
    GXLoadTexObj(object,0);
    GLuint pair[]={original,replacement};
    for(int i=0;i<2;i++) {
        GLint wrap,filter;glBindTexture(GL_TEXTURE_2D,pair[i]);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&wrap);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);
        require(wrap==GL_MIRRORED_REPEAT&&filter==GL_NEAREST,"identical sampler changes on both variants");
    }
    GXInitTexObj(object,other,8,8,GX_TF_RGB565,1,0,0);GXLoadTexObj(object,1);
    GLuint fallback=g_gx.gl_textures[1];
    pc_gx_texture_set_replacements(0);require(g_gx.gl_textures[1]==fallback,"unmatched original fallback");
    pc_gx_texture_set_replacements(1);require(g_gx.gl_textures[1]==fallback,"unmatched pack fallback");
    pc_gx_texture_set_replacements(2);
    GLuint generated=g_gx.gl_textures[1];
    require(generated!=fallback&&g_gx.gl_textures[0]==replacement,"everything uses runtime fallback and retains AI pack");
    expect_texture(generated,32,255,0);
    for(int i=0;i<30;i++) {
        int mode=i%3;
        pc_gx_texture_set_replacements(mode);
        require(g_gx.gl_textures[1]==(mode==2?generated:fallback),"three-mode immediate selection");
        GXLoadTexObj(object,1);
        require(lookups==2&&tex_cache_misses==2,"three-mode cache retained");
    }
    object[TEXOBJ_WRAP_S]=2;object[TEXOBJ_MIN_FILTER]=0;
    GXLoadTexObj(object,1);
    glBindTexture(GL_TEXTURE_2D,generated);
    GLint sampler;
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&sampler);
    require(sampler==GL_MIRRORED_REPEAT,"runtime sampler changes");
    /* Alpha and non-square geometry survive both passes without fringe colors. */
    u32 alpha[6]={0,0xff112233,0,0xff112233,0xff112233,0};
    u32* twice=upscale_2x(alpha,3,2,1,2);
    u32* four=upscale_2x(twice,6,4,1,2);
    require(twice&&four,"runtime RGBA allocation");
    for(int i=0;i<96;i++) require(four[i]==0 || four[i]==0xff112233,"exact alpha/color preservation");
    free(twice);free(four);
    require(upscale_address(-1,3,0)==0&&upscale_address(-1,3,1)==2&&
        upscale_address(-1,3,2)==0&&upscale_address(3,3,2)==2,"clamp/repeat/mirror neighbourhoods");
    /* Changing a dynamic palette must create a new upscaled cache entry. */
    unsigned char indices[32]={0}, pal[32]={0};
    pal[0]=0xf8;pal[1]=0;
    g_gx.tlut[0].data=pal;g_gx.tlut[0].format=1;g_gx.tlut[0].n_entries=16;g_gx.tlut[0].is_be=1;
    unmatched=indices;
    GXInitTexObjCI(object,indices,8,8,GX_TF_C4,1,1,0,0);
    pc_gx_texture_set_replacements(2);GXLoadTexObj(object,2);
    GLuint palette_first=g_gx.gl_textures[2];
    expect_texture(palette_first,32,255,0);
    pal[0]=0;pal[1]=0x1f;
    GXLoadTexObj(object,2);
    require(g_gx.gl_textures[2]!=palette_first,"changed palette gets distinct runtime texture");
    expect_texture(g_gx.gl_textures[2],32,0,255);
    unsigned char no_pack[sizeof(red)];memcpy(no_pack,red,sizeof(red));
    pack_available=0;
    int before=lookups;
    GXInitTexObj(object,no_pack,8,8,GX_TF_RGB565,0,0,0);GXLoadTexObj(object,3);
    expect_texture(g_gx.gl_textures[3],32,255,0);
    require(lookups==before,"everything works without installed pack");
    pc_gx_texture_cache_invalidate();
    require(!glIsTexture(original)&&glIsTexture(replacement),"cache deletes only owned originals");
    require(!glIsTexture(generated)&&!glIsTexture(palette_first),"runtime fallback ownership");
    /* Populate and evict repeatedly, then compare every retained lookup with
     * its full key. This catches broken bucket chains after array compaction. */
    for(unsigned i=0;i<6000;i++) {
        tex_cache_insert(0x1000+i*32,8,8,GX_TF_RGB565,0,i%17,i*7,i*13,0);
        require(tex_cache_find(0x1000+i*32,8,8,GX_TF_RGB565,0,i%17,i*7,i*13)!=NULL,
            "new indexed texture survives hash collisions");
    }
    unsigned first=6000-tex_cache_count;
    for(unsigned i=0;i<6000;i++) {
        TexCacheEntry* found=tex_cache_find(0x1000+i*32,8,8,GX_TF_RGB565,0,i%17,i*7,i*13);
        require((found!=NULL)==(i>=first),"eviction reindexes every retained key and drops evicted keys");
        require(!tex_cache_find(0x1000+i*32,8,8,GX_TF_RGB565,0,i%17,i*7+1,i*13),
            "palette identity remains part of indexed lookup");
    }
    pc_gx_texture_cache_invalidate();
    require(!tex_cache_find(0x1000+5999*32,8,8,GX_TF_RGB565,0,5999%17,5999*7,5999*13),
        "cache invalidation clears bucket index");
    require(glGetError()==GL_NO_ERROR,"OpenGL error-free comparison");
    glDeleteTextures(1,&replacement);SDL_GL_DeleteContext(context);SDL_DestroyWindow(g_pc_window);SDL_Quit();
    puts("PASS: three modes, warm toggles, sampler parity, runtime 4x, alpha, wrap, dynamic palettes and ownership");
    return 0;
}
