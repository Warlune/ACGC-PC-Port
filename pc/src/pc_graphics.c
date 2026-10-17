/* OpenGL presentation pipeline. No vendor SDK or temporal reconstruction is
 * implied: these are spatial scaling, FXAA, sharpening and real MSAA/SSAA. */
#include "pc_platform.h"
#include "pc_gx_internal.h"
#include "pc_graphics.h"
#include "pc_settings.h"

int g_pc_render_w = PC_SCREEN_WIDTH;
int g_pc_render_h = PC_SCREEN_HEIGHT;
static GLuint scene_fbo, resolve_fbo, scene_color, scene_depth, color_tex;
static GLuint program, vao;
static int target_w, target_h, samples, max_samples = 1, available;
static int rejected_w, rejected_h, rejected_samples;
static int max_anisotropy = 1;
#define PC_TEXTURE_MAX_ANISOTROPY 0x84FE
#define PC_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF

static const char* vertex_source =
    "#version 330 core\n"
    "out vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
    "uv=p; gl_Position=vec4(p*2.0-1.0,0,1); }";
static const char* fragment_source =
    "#version 330 core\n"
    "uniform sampler2D image; uniform vec2 texel; uniform int filterMode;"
    "uniform int useFxaa; uniform float sharpness; in vec2 uv; out vec4 color;\n"
    "float cubic(float x){ x=abs(x); return x<1.0?1.5*x*x*x-2.5*x*x+1.0:"
    "(x<2.0?-0.5*x*x*x+2.5*x*x-4.0*x+2.0:0.0); }\n"
    "vec3 sampleColor(vec2 p){ if(filterMode!=2) return texture(image,p).rgb;"
    "vec2 q=p/texel-0.5; vec2 b=floor(q); vec2 f=fract(q); vec3 c=vec3(0);"
    "for(int j=-1;j<=2;j++) for(int i=-1;i<=2;i++)"
    "c+=texture(image,(b+vec2(i,j)+0.5)*texel).rgb*cubic(float(i)-f.x)*cubic(float(j)-f.y);"
    "return clamp(c,0.0,1.0); }\n"
    "float luma(vec3 c){ return dot(c,vec3(0.299,0.587,0.114)); }\n"
    "void main(){ vec3 c=sampleColor(uv);"
    "vec3 n=sampleColor(uv+vec2(0,texel.y)), s=sampleColor(uv-vec2(0,texel.y));"
    "vec3 e=sampleColor(uv+vec2(texel.x,0)), w=sampleColor(uv-vec2(texel.x,0));"
    "if(useFxaa!=0){"
    "float nw=luma(sampleColor(uv+vec2(-1,1)*texel));"
    "float ne=luma(sampleColor(uv+vec2(1,1)*texel));"
    "float sw=luma(sampleColor(uv+vec2(-1,-1)*texel));"
    "float se=luma(sampleColor(uv+vec2(1,-1)*texel)); float m=luma(c);"
    "float lo=min(m,min(min(nw,ne),min(sw,se))), hi=max(m,max(max(nw,ne),max(sw,se)));"
    "if(hi-lo>max(0.0312,hi*0.125)){"
    "vec2 d=vec2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));"
    "float reduce=max((nw+ne+sw+se)*0.03125,0.0078125);"
    "d=clamp(d/(min(abs(d.x),abs(d.y))+reduce),vec2(-8),vec2(8))*texel;"
    "vec3 a=0.5*(sampleColor(uv+d*(1.0/3.0-0.5))+sampleColor(uv+d*(2.0/3.0-0.5)));"
    "vec3 b=a*0.5+0.25*(sampleColor(uv-d*0.5)+sampleColor(uv+d*0.5));"
    "float lb=luma(b); c=(lb<lo||lb>hi)?a:b; }}"
    "vec3 mn=min(c,min(min(n,s),min(e,w))), mx=max(c,max(max(n,s),max(e,w)));"
    "c=clamp(c+(c-(n+s+e+w)*0.25)*sharpness,mn,mx); color=vec4(clamp(c,0.0,1.0),1); }";

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    GLint ok;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "[Graphics] Shader failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void delete_targets(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &scene_fbo);
    glDeleteFramebuffers(1, &resolve_fbo);
    glDeleteRenderbuffers(1, &scene_color);
    glDeleteRenderbuffers(1, &scene_depth);
    glDeleteTextures(1, &color_tex);
    scene_fbo = resolve_fbo = scene_color = scene_depth = color_tex = 0;
    target_w = target_h = 0;
}

void pc_graphics_init(void) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_source);
    GLint linked = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    GLint extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensions);
    for (int i=0; i<extensions; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(ext, "GL_EXT_texture_filter_anisotropic")==0 || strcmp(ext, "GL_ARB_texture_filter_anisotropic")==0) {
            GLfloat maximum = 1.0f;
            glGetFloatv(PC_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
            max_anisotropy = (int)maximum;
            break;
        }
    }
    if (vs && fs) {
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (!linked) {
        fprintf(stderr, "[Graphics] Presentation unavailable; using original OpenGL path\n");
        return;
    }
    glGenVertexArrays(1, &vao);
    available = 1;
    printf("[Graphics] OpenGL spatial presentation ready; max MSAA=%d\n", max_samples);
    pc_graphics_begin_frame();
}

void pc_graphics_shutdown(void) {
    delete_targets();
    if (program) glDeleteProgram(program);
    if (vao) glDeleteVertexArrays(1, &vao);
    program = vao = 0;
    available = 0;
}

static int create_targets(int w, int h, int aa) {
    GLint active, binding, rb;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &rb);
    delete_targets();
    glGenTextures(1, &color_tex);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &resolve_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resolve_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
    int ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1, &scene_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
    if (aa > 1) {
        glGenRenderbuffers(1, &scene_color);
        glBindRenderbuffer(GL_RENDERBUFFER, scene_color);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, aa, GL_RGBA8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, scene_color);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
    }
    glGenRenderbuffers(1, &scene_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, scene_depth);
    if (aa > 1) glRenderbufferStorageMultisample(GL_RENDERBUFFER, aa, GL_DEPTH24_STENCIL8, w, h);
    else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, scene_depth);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glBindTexture(GL_TEXTURE_2D, binding);
    glActiveTexture(active);
    if (ok) { target_w = w; target_h = h; samples = aa; }
    return ok;
}

void pc_graphics_begin_frame(void) {
    if (!available) {
        g_pc_render_w = g_pc_window_w;
        g_pc_render_h = g_pc_window_h;
        return;
    }
    GLint max_size, max_renderbuffer;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer);
    if (max_size > max_renderbuffer) max_size = max_renderbuffer;
    if (max_size > 8192) max_size = 8192;
    double scale = g_pc_settings.render_scale / 100.0;
    double limit = fmin((double)max_size / g_pc_window_w, (double)max_size / g_pc_window_h);
    if (scale > limit) scale = limit;
    int w = (int)(g_pc_window_w * scale + 0.5);
    int h = (int)(g_pc_window_h * scale + 0.5);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int aa = g_pc_settings.msaa;
    while (aa > max_samples) aa /= 2;
    if (aa < 2) aa = 0;
    /* Do not retry an allocation that already failed every frame. A changed
     * setting or output size permits another attempt. Keep the user's saved
     * preference and report the effective target in the support dialog. */
    if (w==rejected_w && h==rejected_h && aa==rejected_samples && target_w) {
        w=target_w; h=target_h; aa=samples;
    }
    if (w != target_w || h != target_h || aa != samples) {
        pc_gx_draw_pending();
        if (!create_targets(w, h, aa)) {
            rejected_w=w; rejected_h=h; rejected_samples=aa;
            fprintf(stderr, "[Graphics] Target allocation failed; reverting to native resolution without MSAA\n");
            w = g_pc_window_w; h = g_pc_window_h; aa = 0;
            if (!create_targets(w, h, aa)) {
                delete_targets();
                available = 0;
                g_pc_render_w = w; g_pc_render_h = h;
                return;
            }
        }
        printf("[Graphics] Render %dx%d -> %dx%d, MSAA %d\n", w, h, g_pc_window_w, g_pc_window_h, aa);
    }
    g_pc_render_w = target_w; g_pc_render_h = target_h;
    glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
    if (samples) glEnable(GL_MULTISAMPLE);
    else glDisable(GL_MULTISAMPLE);
}

void pc_graphics_readback_begin(void) {
    if (!available) return;
    if (samples) {
        GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo);
        glBlitFramebuffer(0, 0, target_w, target_h, 0, 0, target_w, target_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scene_fbo);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve_fbo);
}

void pc_graphics_readback_end(void) {
    if (available) glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
}

void pc_graphics_present(void) {
    if (!available || !target_w) return;
    GLint old_program, old_vao, old_active, old_tex, viewport[4];
    GLboolean mask[4];
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex);
    pc_graphics_readback_begin();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pc_window_w, g_pc_window_h);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(program);
    glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    GLint filter = g_pc_settings.upscale_filter == 0 ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform2f(glGetUniformLocation(program, "texel"), 1.0f / target_w, 1.0f / target_h);
    glUniform1i(glGetUniformLocation(program, "filterMode"), g_pc_settings.upscale_filter);
    glUniform1i(glGetUniformLocation(program, "useFxaa"), g_pc_settings.fxaa);
    glUniform1f(glGetUniformLocation(program, "sharpness"), g_pc_settings.sharpness / 100.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, old_tex);
    glActiveTexture(old_active);
    glBindVertexArray(old_vao); glUseProgram(old_program);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (blend) glEnable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    pc_graphics_readback_end();
}

int pc_graphics_samples(void) { return available ? samples : 0; }
int pc_graphics_max_samples(void) { return max_samples; }
int pc_graphics_max_anisotropy(void) { return max_anisotropy; }
int pc_graphics_anisotropy(void) {
    if (!g_pc_settings.texture_filtering) return 1;
    int value = g_pc_settings.anisotropy;
    if (value < 1) value = 1;
    return value > max_anisotropy ? max_anisotropy : value;
}
void pc_graphics_texture_sampler(void) {
    if (max_anisotropy > 1) glTexParameterf(GL_TEXTURE_2D, PC_TEXTURE_MAX_ANISOTROPY, (GLfloat)pc_graphics_anisotropy());
}

void pc_graphics_support_text(char* out, int size) {
    snprintf(out, size,
        "Active renderer: OpenGL 3.3\nGPU: %s\nInternal render: %dx%d\nOutput: %dx%d\n"
        "MSAA: %dx effective (driver maximum: %d)\n\n"
        "Available: MSAA, FXAA, render scaling / supersampling, nearest / bilinear / bicubic scaling, sharpening and anisotropic texture filtering.\n\n"
        "DLSS / DLAA: not implemented. Requires an additional DirectX or Vulkan renderer, temporal motion vectors and jitter. OpenGL will remain supported.\n"
        "Frame generation, ray tracing and HDR output: not implemented.\n\n"
        "Spatial scaling is not DLSS, FSR or XeSS. The in-game HUD is currently processed with the scene.",
        (const char*)glGetString(GL_RENDERER), g_pc_render_w, g_pc_render_h,
        g_pc_window_w, g_pc_window_h, pc_graphics_samples(), max_samples);
}
