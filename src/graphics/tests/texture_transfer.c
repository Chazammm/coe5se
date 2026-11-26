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

// Native mods invalidate only their source. A cold miss on a full stack must be retryable.
static void check_edits_and_stack(unsigned source) {
    FpsTextureLayer layer,other_layer;
    GLuint other;
    glGenTextures(1,&other);glBindTexture(GL_TEXTURE_2D,other);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
    unsigned char pixels[64*64*4];
    memset(pixels,182,sizeof pixels);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    array_texture_remove(source);
    int depth=0,max_depth=0;
    glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&max_depth);
    for(int i=0;i<max_depth;i++)glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    assert(!ta_prepare(&ta_terrain_cache,source,&layer,1) && ta_reject_reason==FPS_CLIENT_STACK_FULL);
    glGetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH,&depth);
    assert(!terrain_submission_available() && depth==max_depth && glGetError()==GL_NO_ERROR);
    for(int i=0;i<max_depth;i++)glPopClientAttrib();
    map_copy_ticks=0;
    assert(ta_prepare(&ta_terrain_cache,source,&layer,1));
    map_copy_ticks=0;
    assert(ta_prepare(&ta_terrain_cache,other,&other_layer,1));
    ta_scene_replace=1;
    assert(ta_prepare(&ta_sprite_cache,other,&other_layer,1));
    ta_scene_replace=0;
    assert(!coe5seBeforeTextureUpdate(0));
    arrays_native_open=1;
    assert(!coe5seBeforeTextureUpdate(other));
    arrays_native_open=0;
    assert(ta_prepare(&ta_terrain_cache,other,&other_layer,0));
    assert(coe5seBeforeTextureUpdate(other));
    long long attempts=ta_terrain_cache.attempts;
    assert(ta_prepare(&ta_terrain_cache,source,&layer,0) && ta_terrain_cache.attempts==attempts);
    assert(!ta_prepare(&ta_terrain_cache,other,&other_layer,0));
    assert(!ta_prepare(&ta_sprite_cache,other,&other_layer,0));
    memset(pixels,97,sizeof pixels);
    glBindTexture(GL_TEXTURE_2D,other);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    // Leave the old pixels in the freed array layer on the unchecked path.
    void (*upload)(unsigned,int,int,int,int,int,int,int,unsigned,unsigned,const void *)=ta_subimage3d;
    map_texture_pass=0;ta_subimage3d=skip_upload;
    assert(ta_prepare(&ta_terrain_cache,other,&other_layer,1));
    ta_subimage3d=upload;map_texture_pass=MAP_TERRAIN;map_copy_ticks=0;
    assert(ta_prepare(&ta_terrain_cache,other,&other_layer,1));
    unsigned char actual[sizeof pixels];
    assert(ta_layer_matches(other_layer.array_texture,other_layer.layer,0,64,64,pixels,actual));
    assert(ta_prepare(&ta_terrain_cache,other,&other_layer,0));
    if(ta_has_restart){
        glEnable(0x8f9d);assert(!terrain_submission_available());glDisable(0x8f9d);
    }
    assert(glGetError()==GL_NO_ERROR);
    array_texture_remove(other);glDeleteTextures(1,&other);
}

// Return a private name only to its original context. Other contexts may reuse its number.
static void check_context(HDC dc,HGLRC owner) {
    HGLRC other=wglCreateContext(dc);
    assert(other && map_label && map_read_label);
    map_sync_reserve_context(owner);
    GLuint name;
    glGenTextures(1,&name);glBindTexture(GL_TEXTURE_2D,name);
    map_label(GL_TEXTURE,name,-1,map_reserve_tag);
    map_reserve[0]=(MapReservedTile){.texture=name,.pixels=calloc(1,MAP_TILE_BYTES)};
    map_reserve[1]=(MapReservedTile){.pixels=calloc(1,MAP_TILE_BYTES)};
    map_reserve_count=2;map_reserve_pending=1;
    assert(wglMakeCurrent(NULL,NULL));
    map_sync_reserve_context(NULL);
    assert(!map_reserve_count && !map_reserve_pending && map_retired_count==1);
    assert(wglMakeCurrent(dc,other));
    map_sync_reserve_context(other);
    assert(!map_reserve_count && !map_reserve_pending && map_retired_count==1);
    glBindTexture(GL_TEXTURE_2D,name);
    assert(glIsTexture(name));
    assert(wglMakeCurrent(dc,owner));
    map_sync_reserve_context(owner);
    assert(!map_retired_count && !glIsTexture(name));
    assert(wglMakeCurrent(dc,other) && glIsTexture(name));
    assert(wglMakeCurrent(dc,owner) && wglDeleteContext(other));
}

typedef struct { HDC dc; HGLRC replacement; } WorkerContext;

// A new rendering thread can prepare its own map tiles after the original context is destroyed.
static DWORD WINAPI check_replacement(void *arg) {
    WorkerContext *worker=arg;
    assert(wglMakeCurrent(worker->dc,worker->replacement));
    assert(TryAcquireSRWLockExclusive(&map_work_lock));
    assert(ta_initialize_context() && ta_sprite_cache.entries==ta_entries);
    map_sync_reserve_context(worker->replacement);
    assert(!map_reserve_count && !map_reserve_pending && map_retired_count==1);
    MapReservedTile *slot=&map_reserve[0];
    slot->pixels=calloc(1,MAP_TILE_BYTES);
    map_reserve_count=map_reserve_pending=1;
    assert(slot->pixels && map_prepare_slot(slot) && !map_reserve_pending);
    assert(map_name_matches(slot->texture,map_reserve_tag));
    map_release_slot(slot,0);
    assert(!map_reserve_count && glGetError()==GL_NO_ERROR);
    ReleaseSRWLockExclusive(&map_work_lock);
    assert(wglMakeCurrent(NULL,NULL));
    return 0;
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
    assert(mod_initialize_original_gl() && install_texture_array() && arrays_load_functions());
    assert(arrays_check_context() && ta_initialize_context() && map_copy_image);
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
    ta_terrain_cache.layer_limit=4;
    check_edits_and_stack(source);
    array_texture_remove(source);
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
    assert(map_reserve[0].pixels && replacement && wglMakeCurrent(NULL,NULL) && wglDeleteContext(context));
    HMODULE glu=LoadLibraryExW(L"glu32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    assert(glu);
    arrays_real_gluBuild2DMipmaps=(void *)GetProcAddress(glu,"gluBuild2DMipmaps");
    assert(arrays_real_gluBuild2DMipmaps);
    WorkerContext worker={dc,replacement};
    HANDLE thread=CreateThread(NULL,0,check_replacement,&worker,0,NULL);
    assert(thread && WaitForSingleObject(thread,15000)==WAIT_OBJECT_0);
    CloseHandle(thread);FreeLibrary(glu);
    assert(wglDeleteContext(replacement));
    ReleaseDC(win,dc);DestroyWindow(win);
    puts("Verified texture edits, retries, GL state, both layer readers and render-thread handoff.");
}
