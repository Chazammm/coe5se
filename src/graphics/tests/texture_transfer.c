// Check transfer failures, native error ownership and reserve cleanup in Windows OpenGL.
#include "../graphics.c"
#include <assert.h>

// Simulate an ignored driver copy or upload without changing the caller's error flag.
static void WINAPI skip_copy(unsigned a,unsigned b,int c,int d,int e,int f,unsigned g,unsigned h,
                             int i,int j,int k,int l,int m,int n,int o) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;
    (void)i;(void)j;(void)k;(void)l;(void)m;(void)n;(void)o;
}
static void skip_upload(unsigned a,int b,int c,int d,int e,int f,int g,int h,unsigned i,unsigned j,const void *k) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k;
}

// The same source must recover on the next copy and preserve an external GL error.
static void check_copy(unsigned source) {
    FpsTextureLayer layer={0};
    MapCopyImage driver_copy=map_copy_image;
    void (*driver_upload)(unsigned,int,int,int,int,int,int,int,unsigned,unsigned,const void *)=ta_subimage3d;
    map_copy_image=skip_copy;
    ta_subimage3d=skip_upload;
    glEnable(0xffff);
    map_copy_ticks=0;
    assert(!ta_prepare(&ta_terrain_cache,source,&layer,1));
    assert(ta_reject_reason==FPS_TEXTURE_TRANSFER);
    assert(ta_terrain_cache.pages[0].used==0 && glGetError()==GL_INVALID_ENUM);
    map_copy_image=driver_copy;
    long long attempts=ta_terrain_cache.attempts;
    assert(!ta_prepare(&ta_terrain_cache,source,&layer,1) && ta_terrain_cache.attempts==attempts);
    ta_terrain_cache.pass+=64;
    map_copy_ticks=0;
    assert(ta_prepare(&ta_terrain_cache,source,&layer,1));
    ta_release_layer(&ta_terrain_cache,&layer);
    memset(ta_terrain_cache.entries,0,ta_terrain_cache.size*sizeof *ta_terrain_cache.entries);
    glBindTexture(GL_TEXTURE_2D,source);
    unsigned char image[64*64*4];
    memset(image,0,sizeof image);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    map_copy_image=skip_copy;
    ta_subimage3d=driver_upload;
    assert(ta_clone(&ta_terrain_cache,source,&layer));
    ta_release_layer(&ta_terrain_cache,&layer);
    map_copy_image=driver_copy;
    assert(glGetError()==GL_NO_ERROR);
}

// A remaining private name can be retired only after its original context returns.
static void check_context(HDC dc,HGLRC owner) {
    HGLRC other=wglCreateContext(dc);
    GLuint name;
    glGenTextures(1,&name);glBindTexture(GL_TEXTURE_2D,name);
    strcpy(map_reserve_tag,"terrain regression");
    map_label(GL_TEXTURE,name,-1,map_reserve_tag);
    map_reserve_context=owner;
    map_reserve[0]=(MapReservedTile){.texture=name,.pixels=calloc(1,MAP_TILE_BYTES)};
    map_reserve[1]=(MapReservedTile){.pixels=calloc(1,MAP_TILE_BYTES)};
    map_reserve_count=2;map_reserve_pending=1;
    assert(other && wglMakeCurrent(dc,other));
    map_release_detached_pixels();
    assert(!map_reserve[0].pixels && !map_reserve[1].pixels && map_reserve_count==1 && !map_reserve_pending);
    GLuint unrelated=0;
    glGenTextures(1,&unrelated);glBindTexture(GL_TEXTURE_2D,unrelated);
    map_reserve_end();
    assert(glIsTexture(unrelated) && map_reserve_count==1);
    assert(wglMakeCurrent(dc,owner));
    map_reserve_end();
    assert(!map_reserve_count && !glIsTexture(name));
    assert(wglDeleteContext(other));
}

// Exercise the direct-state and framebuffer layer readers on the same compatibility driver.
int main(void) {
    HWND win=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,8,8,NULL,NULL,NULL,NULL);
    HDC dc=GetDC(win);
    PIXELFORMATDESCRIPTOR pf={.nSize=sizeof pf,.nVersion=1,
        .dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL,.iPixelType=PFD_TYPE_RGBA,.cColorBits=32};
    assert(win && dc && SetPixelFormat(dc,ChoosePixelFormat(dc,&pf),&pf));
    HGLRC context=wglCreateContext(dc);
    assert(context && wglMakeCurrent(dc,context));
    map_textures_enabled=1;
    assert(install_texture_array() && ta_initialize_context() && map_copy_image);
    ta_terrain_cache.layer_limit=1;
    static FpsTextureArrayEntry entries[16];
    ta_terrain_cache.entries=entries;
    ta_terrain_cache.size=16;
    glEnable(GL_TEXTURE_2D);
    map_texture_pass=MAP_TERRAIN;
    unsigned source;
    glGenTextures(1,&source);glBindTexture(GL_TEXTURE_2D,source);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
    unsigned char image[64*64*4];
    memset(image,19,sizeof image);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    FpsTextureLayer seed={0};
    assert(ta_clone(&ta_terrain_cache,source,&seed));
    assert(glGetError()==GL_NO_ERROR);
    assert(map_copy_layer(source,seed.array_texture,99,64,64,0));
    assert(glGetError()==GL_INVALID_VALUE && map_copy_image);
    ta_release_layer(&ta_terrain_cache,&seed);
    memset(image,77,sizeof image);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    check_copy(source);
    ta_read_subimage=NULL;
    ta_init_layer_reader(3,0,"");
    glBindTexture(GL_TEXTURE_2D,source);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    check_copy(source);
    check_context(dc,context);
    map_reserve_context=context;
    map_reserve[0]=(MapReservedTile){.texture=source,.pixels=calloc(1,MAP_TILE_BYTES)};
    map_reserve_count=1;
    HGLRC replacement=wglCreateContext(dc);
    assert(map_reserve[0].pixels && replacement && wglMakeCurrent(dc,replacement));
    assert(wglDeleteContext(context));
    assert(!ta_initialize_context());
    map_release_detached_pixels();
    assert(!map_reserve[0].pixels && map_reserve_count==1 && !map_reserve_pending);
    wglMakeCurrent(NULL,NULL);wglDeleteContext(replacement);ReleaseDC(win,dc);DestroyWindow(win);
    puts("Transfer recovery, native GL errors, both layer readers and context cleanup passed.");
}
