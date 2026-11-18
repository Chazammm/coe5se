// Standalone Windows regression tests; use the same compiler and libraries as src/build.sh.
#include "../graphics.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
_Static_assert(sizeof(ArraysVertex)==64,"keep the original map vertex layout");

static int choices[4],callback_calls;
static PluginApi test_api;
// Resolves the four graphics settings from the current test mask.
static const int *test_setting(const char *name){
    const char *names[]={"graphics_changes","smoother_map_rendering","frame_controls","scene_sprite_batching"};
    for(int i=0;i<4;i++)if(!strcmp(name,names[i]))return &choices[i];
    return NULL;
}
// Accepts diagnostics without requiring a running plugin host.
static void test_log(const void *owner,const char *line){(void)owner;(void)line;}
// Counts native prepared-draw invocations.
static void test_draw(void){callback_calls++;}
// Reports all fixed-function capabilities disabled.
static unsigned char mock_disabled(unsigned cap){(void)cap;return 0;}
// Checks that the depth guard forwards the original capability.
static void mock_enable_depth(unsigned cap){CHECK(cap==0x0b71);}
static int source_width=8,source_height=128,source_min=0x2703,source_max=1000,missing_level=-1,bad_format;
static int source_rgb;
static unsigned bound_2d=20,bound_array=70,uploads,allocated_levels;
// Supplies rectangular native texture levels, including malformed fixtures.
static void mock_level(unsigned target,int level,unsigned name,int *out){
    (void)target;*out=0;
    if(name==0x1000)*out=level==missing_level?0:ta_level_size(source_width,level);
    if(name==0x1001)*out=ta_level_size(source_height,level);
    if(name==0x1003)*out=bad_format || source_rgb?0x8051:0x8058;
    if(name>=0x805c && name<=0x805f)*out=source_rgb && name==0x805f?0:8;
}
// Supplies native integer sampling parameters.
static void mock_parameter(unsigned target,unsigned name,int *out){
    (void)target;*out=0;
    if(name==0x2801)*out=source_min;
    if(name==0x2800)*out=0x2601;
    if(name==0x2802 || name==0x2803)*out=0x812f;
    if(name==0x813d)*out=source_max;
}
// Supplies the native sampler's floating-point defaults.
static void mock_parameter_float(unsigned target,unsigned name,float *out){
    (void)target;*out=name==0x813a?-1000.0f:name==0x813b?1000.0f:0.0f;
}
// Returns current bindings and the fixture's layer limit.
static void mock_integer(unsigned name,int *out){*out=name==0x8069?(int)bound_2d:name==0x8c1d?(int)bound_array:name==0x88ff?16:0;}
// Tracks the source and array texture bindings.
static void mock_bind(unsigned target,unsigned texture){if(target==0x0de1)bound_2d=texture;else bound_array=texture;}
// Allocates one deterministic array texture name.
static void mock_generate(int count,unsigned *ids){CHECK(count==1);*ids=99;}
// Accepts array sampler setup without a GL context.
static void mock_set_parameter(unsigned target,unsigned name,int value){(void)target;(void)name;(void)value;}
// Accepts a client-state save without a GL context.
static void mock_push(unsigned mask){(void)mask;}
// Accepts the corresponding client-state restore.
static void mock_pop(void){}
// Accepts pixel packing setup without a GL context.
static void mock_store(unsigned name,int value){(void)name;(void)value;}
// Checks each allocated array level against the native rectangular mip chain.
static void mock_image(unsigned target,int level,int format,int width,int height,int layers,int border,unsigned channels,unsigned type,const void *pixels){
    (void)target;CHECK(format==0x8058 && layers==16 && !border && channels==0x1908 && type==0x1401 && !pixels);
    CHECK(width==ta_level_size(source_width,level) && height==ta_level_size(source_height,level));allocated_levels++;
}
// Produces distinct exact RGBA bytes for each source level.
static void mock_get_image(unsigned target,int level,unsigned format,unsigned type,void *pixels){
    CHECK(target==0x0de1 && format==0x1908 && type==0x1401);
    memset(pixels,level+17,(size_t)ta_level_size(source_width,level)*ta_level_size(source_height,level)*4);
}
// Verifies every uploaded pixel and rectangular level dimension.
static void mock_subimage(unsigned target,int level,int x,int y,int z,int width,int height,int depth,unsigned format,unsigned type,const void *pixels){
    CHECK(target==0x8c1a && !x && !y && !z && depth==1 && format==0x1908 && type==0x1401);
    CHECK(width==ta_level_size(source_width,level) && height==ta_level_size(source_height,level));
    const unsigned char *bytes=pixels;for(size_t i=0;i<(size_t)width*height*4;i++)CHECK(bytes[i]==level+17);uploads++;
}
// Resets array allocation state between independent fixtures.
static void reset_cache(void){
    memset(&ta_sprite_cache,0,sizeof(ta_sprite_cache));uploads=allocated_levels=0;
    ta_sprite_cache.entries=ta_entries;ta_sprite_cache.size=TA_CACHE_SIZE;ta_sprite_cache.layer_limit=256;
}
// Checks exact rectangular mip uploads and rejection of unsupported source data.
static void test_textures(void){
    ta_get_level=mock_level;ta_get_parameter=mock_parameter;ta_get_parameter_float=mock_parameter_float;
    ta_get_int=mock_integer;ta_bind=mock_bind;ta_generate=mock_generate;ta_parameter=mock_set_parameter;
    ta_push_client=mock_push;ta_pop_client=mock_pop;ta_pixel_store=mock_store;
    ta_image3d=mock_image;ta_get_image=mock_get_image;ta_subimage3d=mock_subimage;
    ta_has_aniso=ta_has_swizzle=0;ta_scene_replace=1;
    int w,h,last;unsigned min,mag;FpsTextureLayer layer;
    CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,0) && w==8 && h==128 && last==7);
    reset_cache();CHECK(ta_clone(&ta_sprite_cache,20,&layer));CHECK(layer.width==8 && layer.height==128 && layer.mip_count==8 && uploads==8 && allocated_levels==8);
    CHECK(bound_2d==20 && bound_array==70);
    source_width=32;source_height=64;reset_cache();CHECK(ta_clone(&ta_sprite_cache,20,&layer) && uploads==7 && layer.mip_count==7);
    missing_level=3;reset_cache();CHECK(!ta_clone(&ta_sprite_cache,20,&layer) && !uploads && !allocated_levels);missing_level=-1;
    bad_format=1;CHECK(!ta_source_parameters(&w,&h,&last,&min,&mag,0));bad_format=0;
    source_min=0x2601;missing_level=1;CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,0) && last==0);missing_level=-1;
    source_min=0x2703;source_max=2;CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,0) && last==2);source_max=1000;
    source_width=30;CHECK(!ta_source_parameters(&w,&h,&last,&min,&mag,0));source_width=32;
    ta_scene_replace=0;CHECK(!ta_source_parameters(&w,&h,&last,&min,&mag,0));
    source_width=source_height=64;CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,0) && last==6);
    source_rgb=1;
    CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,1) && last==6);
    source_min=0x2601;missing_level=1;
    CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,1) && last==0);
    source_min=0x2703;missing_level=-1;
    CHECK(!ta_source_parameters(&w,&h,&last,&min,&mag,0));source_rgb=0;
    source_width=source_height=128;CHECK(ta_source_parameters(&w,&h,&last,&min,&mag,0) && last==7);
}
static HGLRC WINAPI mock_context(void){return (HGLRC)(uintptr_t)1;}
static void mock_other_parameter(unsigned target,unsigned name,int value){
    CHECK((target==0x8c1a || target==0x0de1) && name==0x2800 && value==0x2601);
}
// A rejection from map drawing must not poison that texture in a scene.
static void test_texture_cache_policy(void){
    reset_cache();ta_current_context=mock_context;ta_context=mock_context();
    ta_context_attempted=ta_context_ok=ta_state_checked=ta_state_ok=texture_array_enabled=1;
    ta_track_state=ta_scene_replace=0;source_width=8;source_height=128;
    FpsTextureLayer layer;
    CHECK(!array_texture_prepare(20,&layer) && !uploads);
    ta_scene_replace=1;
    CHECK(array_texture_prepare(20,&layer) && uploads==8 && layer.width==8 && layer.height==128);
    CHECK(array_texture_prepare(20,&layer) && uploads==8);
    arrays_texture=21;arrays_get_int=mock_integer;arrays_real_glTexParameteri=mock_other_parameter;
    arrays_glTexParameteri(0x8c1a,0x2800,0x2601);
    ta_state_checked=1;
    CHECK(array_texture_prepare(20,&layer) && uploads==8);
    arrays_glTexParameteri(0x0de1,0x2800,0x2601);
    ta_state_checked=1;
    CHECK(array_texture_prepare(20,&layer) && uploads==16);
    array_texture_remove(20);reset_cache();ta_scene_replace=texture_array_enabled=0;
}
// A capacity rejection is retried when a texture is deleted and frees a layer.
static void test_cache_retries_after_delete(void){
    reset_cache();ta_current_context=mock_context;ta_context=mock_context();
    ta_context_attempted=ta_context_ok=ta_state_checked=ta_state_ok=texture_array_enabled=1;
    ta_track_state=ta_scene_replace=0;source_width=source_height=64;
    ta_sprite_cache.page_count=1;ta_sprite_cache.bytes=TA_BUDGET;
    ta_sprite_cache.pages[0]=(FpsTextureArrayPage){.texture=99,.width=64,.height=64,.last_level=6,.min_filter=0x2703,.mag_filter=0x2601,.layers=1,.used=1,.occupied={1}};
    unsigned slot=(20u*2654435761u)&(TA_CACHE_SIZE-1);
    ta_entries[slot]=(FpsTextureArrayEntry){.source=20,.decision=1,.value={.array_texture=99}};
    FpsTextureLayer layer;
    CHECK(!array_texture_prepare(21,&layer) && !uploads);
    CHECK(!array_texture_prepare(21,&layer) && ta_sprite_cache.attempts==1);
    array_texture_remove(20);
    CHECK(array_texture_prepare(21,&layer) && uploads==7 && layer.array_texture==99 && layer.layer==0);
    array_texture_remove(21);reset_cache();texture_array_enabled=0;
}
static void mock_delete_stale(int count,const unsigned *textures){CHECK(count==1 && textures[0]==27);}
static void test_terrain_cache_trim(void){
    FpsTextureArrayEntry entries[8]={0};
    FpsTextureArrayCache cache={.entries=entries,.size=8,.pass=32,.pressure=1,.page_count=2,.bytes=32768};
    cache.pages[0]=(FpsTextureArrayPage){.texture=27,.width=64,.height=64,.layers=1,.used=1,.occupied={1}};
    cache.pages[1]=(FpsTextureArrayPage){.texture=28,.width=64,.height=64,.layers=1,.used=1,.occupied={1}};
    entries[1]=(FpsTextureArrayEntry){.source=1,.last_pass=16,.decision=1,.value={.array_texture=27}};
    entries[2]=(FpsTextureArrayEntry){.source=2,.last_pass=31,.decision=1,.value={.array_texture=28,.page=1}};
    entries[3]=(FpsTextureArrayEntry){.source=3,.last_pass=31,.decision=-1,.reason=FPS_CACHE_LIMIT};
    ta_delete=mock_delete_stale;
    ta_trim_terrain_cache(&cache);
    CHECK(!entries[1].source && entries[2].value.array_texture==28 && entries[2].value.page==0);
    CHECK(entries[3].source==3 && entries[3].capacity_epoch!=cache.capacity_epoch);
    CHECK(cache.page_count==1 && cache.pages[0].texture==28 && cache.pages[0].used==1 && cache.bytes==16384 && !cache.pressure);
}
static unsigned changed_gl_integer,enabled_client_array;
static void mock_terrain_state(unsigned name,int *out){
    *out=name==0x84e0||name==0x84e1?0x84c0:0;
    if(name==changed_gl_integer)*out=9;
}
static unsigned char mock_terrain_client_array(unsigned cap){return cap==enabled_client_array;}
static void mock_client_active_texture(unsigned unit){(void)unit;}
static void test_terrain_submission_guard(void){
    void (*original)(unsigned,int *)=ta_get_int;
    unsigned char (*original_enabled)(unsigned)=arrays_is_enabled;
    void (*original_active)(unsigned)=arrays_client_active;
    ta_get_int=mock_terrain_state;arrays_is_enabled=mock_terrain_client_array;arrays_client_active=mock_client_active_texture;
    ta_has_sampler=arrays_has_vbo=arrays_has_vao=1;
    CHECK(terrain_submission_available());
    const unsigned bindings[]={0x8b8d,0x84e0,0x8ca6,0x8caa,0x8919,0x8894,0x8895,0x85b5,0x84e1};
    for(size_t i=0;i<sizeof(bindings)/sizeof(bindings[0]);i++){
        changed_gl_integer=bindings[i];CHECK(!terrain_submission_available());
    }
    changed_gl_integer=0;
    for(unsigned cap=0x8074;cap<=0x8078;cap+=2){enabled_client_array=cap;CHECK(!terrain_submission_available());}
    enabled_client_array=0;CHECK(terrain_submission_available());
    ta_get_int=original;
    arrays_is_enabled=original_enabled;arrays_client_active=original_active;
    ta_has_sampler=arrays_has_vbo=arrays_has_vao=0;
}

// A native mod can bind a pixel buffer after the per-pass state check.
static void mock_pixel_buffer_state(unsigned name,int *out){mock_integer(name,out);if(name==changed_gl_integer)*out=9;}
static void test_pixel_buffer_during_copy(void){
    reset_cache();source_width=source_height=64;
    ta_current_context=mock_context;ta_context=mock_context();
    ta_context_attempted=ta_context_ok=ta_state_checked=ta_state_ok=texture_array_enabled=1;
    ta_get_int=mock_pixel_buffer_state;
    FpsTextureLayer layer;
    changed_gl_integer=0x88ed;CHECK(!array_texture_prepare(20,&layer) && !uploads && !ta_sprite_cache.attempts);
    changed_gl_integer=0x88ef;CHECK(!array_texture_prepare(20,&layer) && !uploads && !ta_sprite_cache.attempts);
    changed_gl_integer=0;CHECK(array_texture_prepare(20,&layer) && uploads==7 && ta_sprite_cache.attempts==1);
    array_texture_remove(20);reset_cache();texture_array_enabled=0;ta_get_int=mock_integer;
}
// Checks parent/child switches, API guards and single native callback execution.
static void test_feature_guards(void){
    test_api.setting=test_setting;test_api.logLine=test_log;plugin_api=&test_api;
    for(int mask=0;mask<16;mask++){
        for(int i=0;i<4;i++)choices[i]=!!(mask&(1<<i));
        unsigned wanted=choices[0]?(choices[1]?1u:0u)|(choices[2]?2u:0u)|(choices[3]?4u:0u):0;
        CHECK(mod_requested_features(&test_api)==wanted);
    }
    mod_scenes_enabled=0;coe5seGraphicsDrawScene(test_draw);CHECK(callback_calls==1);
    // No game code or OpenGL calls are needed when the selected scene has no captured primitives.
    arrays_enabled=texture_array_enabled=0;ta_current_context=NULL;arrays_context_ok=0;
    mod_scenes_enabled=1;mod_scene_kind=1;coe5seGraphicsDrawScene(test_draw);CHECK(callback_calls==2);
    mod_scene_drawing=1;coe5seGraphicsDrawScene(test_draw);CHECK(callback_calls==3);mod_scene_drawing=0;
    CHECK(!coe5seGraphicsInit(NULL,"."));test_api.version=PLUGIN_API_VERSION-1;CHECK(!coe5seGraphicsInit(&test_api,"."));
    test_api.version=PLUGIN_API_VERSION;memset(choices,0,sizeof(choices));CHECK(coe5seGraphicsInit(&test_api,".") && !mod_features);
    choices[0]=choices[3]=1;CHECK(coe5seGraphicsInit(&test_api,".") && !mod_features);
    arrays_is_enabled=mock_disabled;arrays_real_glEnable=mock_enable_depth;
    arrays_scene_scope=arrays_projection_capture=1;arrays_glEnable(0x0b71);CHECK(!arrays_projection_capture);arrays_scene_scope=0;
}
// Checks patch masks and refusal of changed call sites or imports without writes.
static void test_native_patches(void){
    const size_t bytes=0x350000;
    detail_base=VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(detail_base);
    unsigned char *before=malloc(bytes);CHECK(before);
    for(unsigned mask=0;mask<8;mask++){
        memset(detail_base,0,bytes);mod_features=mask;
        for(size_t i=0;i<sizeof(mod_calls)/sizeof(mod_calls[0]);i++)CHECK(write_relative_call(detail_base+mod_calls[i].site,detail_base+mod_calls[i].target));
        memcpy(before,detail_base,bytes);CHECK(mod_verify_calls());CHECK(mod_install_calls());
        for(size_t i=0;i<sizeof(mod_calls)/sizeof(mod_calls[0]);i++)CHECK((memcmp(before+mod_calls[i].site,detail_base+mod_calls[i].site,5)!=0)==!!(mod_calls[i].group&mask));
    }
    memset(detail_base,0,bytes);mod_features=7;
    for(size_t i=0;i<sizeof(mod_calls)/sizeof(mod_calls[0]);i++)CHECK(write_relative_call(detail_base+mod_calls[i].site,detail_base+mod_calls[i].target));
    for(size_t i=0;i<sizeof(mod_calls)/sizeof(mod_calls[0]);i++)if(mod_calls[i].group==MOD_SCENES){
        unsigned char saved=detail_base[mod_calls[i].site];detail_base[mod_calls[i].site]=0x90;memcpy(before,detail_base,bytes);
        CHECK(!mod_install_calls() && !memcmp(before,detail_base,bytes));mod_features=3;CHECK(mod_verify_calls());mod_features=7;
        detail_base[mod_calls[i].site]=saved;
    }
    HMODULE gl=LoadLibraryA("opengl32.dll"),glu=LoadLibraryA("glu32.dll");CHECK(gl && glu);
    void *expected_glu=(void *)GetProcAddress(glu,"gluBuild2DMipmaps");CHECK(expected_glu);
    for(size_t i=0;i<sizeof(arrays_imports)/sizeof(arrays_imports[0]);i++){
        void *pointer=(void *)GetProcAddress(gl,arrays_imports[i].name);CHECK(pointer);memcpy(detail_base+arrays_imports[i].rva,&pointer,sizeof(pointer));
    }
    memcpy(detail_base+RVA_RDATA_SECTION,&expected_glu,sizeof(expected_glu));CHECK(fps_check_gl_imports(detail_base,gl,expected_glu,NULL));
    for(size_t i=0;i<sizeof(arrays_imports)/sizeof(arrays_imports[0]);i++){
        void *pointer=NULL,*foreign=(void *)test_draw;unsigned char *slot=detail_base+arrays_imports[i].rva;
        memcpy(&pointer,slot,sizeof(pointer));memcpy(slot,&foreign,sizeof(foreign));memcpy(before,detail_base,bytes);
        CHECK(!fps_check_gl_imports(detail_base,gl,expected_glu,NULL) && !memcmp(before,detail_base,bytes));memcpy(slot,&pointer,sizeof(pointer));
    }
    void *foreign=(void *)test_draw;memcpy(detail_base+RVA_RDATA_SECTION,&foreign,sizeof(foreign));memcpy(before,detail_base,bytes);
    CHECK(!fps_check_gl_imports(detail_base,gl,expected_glu,NULL) && !memcmp(before,detail_base,bytes));
    free(before);VirtualFree(detail_base,0,MEM_RELEASE);
}
static int import_writes_before_failure;
static int mock_replace_import(uintptr_t rva,void *hook,void *saved){
    if(!import_writes_before_failure){import_writes_before_failure=-1;return 0;}
    if(import_writes_before_failure>0)import_writes_before_failure--;
    void **slot=gl_import_slot(rva);
    memcpy(saved,slot,sizeof(void *));memcpy(slot,&hook,sizeof(hook));return 1;
}
// Fault injection covers a failure at each slot and a later third-party redirect.
static void test_import_installation_rollback(void){
    detail_base=VirtualAlloc(NULL,0x350000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(detail_base);
    void *original=(void *)test_draw,*foreign=(void *)mock_pop;
    for(size_t i=0;i<ARRAY_IMPORT_COUNT;i++)*gl_import_slot(arrays_imports[i].rva)=original;
    *gl_import_slot(FPS_GLU_SLOT)=original;
    for(int fail=0;fail<=ARRAY_IMPORT_COUNT;fail++){
        FpsArrayInstall install={0};import_writes_before_failure=fail;
        CHECK(!arrays_install_imports(&install,mock_replace_import) && install.count==(size_t)fail);
        CHECK(arrays_restore_imports(&install,mock_replace_import) && !install.count);
        for(size_t i=0;i<ARRAY_IMPORT_COUNT;i++)CHECK(*gl_import_slot(arrays_imports[i].rva)==original);
        CHECK(*gl_import_slot(FPS_GLU_SLOT)==original);
    }
    FpsArrayInstall install={0};import_writes_before_failure=-1;
    CHECK(arrays_install_imports(&install,mock_replace_import) && install.count==ARRAY_IMPORT_COUNT+1);
    *gl_import_slot(arrays_imports[0].rva)=foreign;
    CHECK(arrays_restore_imports(&install,mock_replace_import));
    CHECK(*gl_import_slot(arrays_imports[0].rva)==foreign);
    for(size_t i=1;i<ARRAY_IMPORT_COUNT;i++)CHECK(*gl_import_slot(arrays_imports[i].rva)==original);
    CHECK(*gl_import_slot(FPS_GLU_SLOT)==original);
    VirtualFree(detail_base,0,MEM_RELEASE);
}
// Checks that VFX retain the source texture, coordinates and exact float colors.
static void test_vfx_native_vertices(void){
    arrays_enabled=arrays_native_texture=decor_scope=texture_array_enabled=1;
    arrays_allowed_cached=1;arrays_color_known=arrays_uv_known=1;arrays_count=0;arrays_open=arrays_native_open=0;
    arrays_color[0]=0.23f;arrays_color[1]=0.64f;arrays_color[2]=0.89f;arrays_color[3]=0.37f;arrays_texture=77;
    arrays_begin_probe(7);CHECK(arrays_open && !arrays_layer_current && !arrays_output_layered && arrays_output_texture==77);
    for(int i=0;i<4;i++){arrays_tex2_probe((float)(i&1),(float)(i>>1));arrays_vertex3_probe((float)i,12.5f,-0.2f);}
    arrays_end_probe();CHECK(arrays_count==4 && !arrays_open);
    for(int i=0;i<4;i++){
        ArraysVertex *v=&arrays_vertices[i];CHECK(v->xyz[0]==(float)i && v->xyz[1]==12.5f && v->xyz[2]==-0.2f);
        CHECK(v->source_texture==77 && !memcmp(v->rgba,arrays_color,sizeof(v->rgba)));
        CHECK(v->uv[0]==(float)(i&1) && v->uv[1]==(float)(i>>1) && v->uv[2]==0 && v->uv[3]==1);
        CHECK(!memcmp(v->uv,v->source_uv,sizeof(v->uv)));
    }
    arrays_count=0;arrays_enabled=arrays_native_texture=decor_scope=texture_array_enabled=0;
}
static unsigned fallback_program,fallback_unit,fallback_begins,fallback_ends,fallback_vertices,fallback_draws,fallback_pops;
static void mock_program(unsigned program){fallback_program=program;}
static void mock_active_texture(unsigned unit){fallback_unit=unit;}
static void mock_fallback_pop(void){fallback_pops++;}
static void mock_fallback_begin(unsigned mode){
    CHECK(mode==7 && !fallback_program && fallback_unit==0x84c0 && !ta_drawing && !arrays_client_session);
    CHECK(fallback_pops==1 && bound_2d==20);fallback_begins++;
}
static void mock_fallback_end(void){fallback_ends++;}
static void mock_fallback_coord(float x,float y,float z,float w){(void)x;(void)y;(void)z;(void)w;}
static void mock_fallback_vertex(float x,float y,float z){(void)x;(void)y;(void)z;fallback_vertices++;}
static void mock_native_array_draw(unsigned mode,int first,int count){
    CHECK(mode==7 && !first && count==4 && !fallback_program && fallback_unit==0x84c0 && !ta_drawing);
    fallback_draws++;
}
// Both native routes must restore GL state after a retained layered draw.
static void test_native_after_layered(void){
    texture_array_enabled=0;ta_multi_page=1;ta_has_sampler=0;ta_env_mode=0x2100;
    ta_use_program=mock_program;ta_active_texture=mock_active_texture;ta_saved_program=0;ta_saved_unit=0x84c0;
    arrays_pop_client=mock_fallback_pop;real_gl_bind=mock_bind;real_gl_begin=mock_fallback_begin;real_gl_end=mock_fallback_end;
    real_gl_color=arrays_tex4=mock_fallback_coord;arrays_vertex3=mock_fallback_vertex;arrays_draw=mock_native_array_draw;
    arrays_texture=20;arrays_output_texture=99;arrays_projection_session=0;
    arrays_color_known=arrays_uv_known=0;
    memset(arrays_vertices,0,4*sizeof(arrays_vertices[0]));
    for(int i=0;i<4;i++)arrays_vertices[i].source_texture=20;
    arrays_count=4;arrays_output_layered=ta_drawing=arrays_client_session=1;fallback_program=7;
    arrays_flush();
    CHECK(fallback_begins==1 && fallback_ends==1 && fallback_vertices==4 && !arrays_client_session && !ta_drawing);
    arrays_count=4;arrays_output_layered=0;arrays_output_texture=20;
    ta_drawing=arrays_client_session=1;fallback_program=7;
    arrays_flush();
    CHECK(fallback_draws==1 && fallback_pops==2 && !arrays_client_session && !ta_drawing);
}
// Runs all standalone renderer and hook regressions.
int main(void){
    test_textures();test_texture_cache_policy();test_cache_retries_after_delete();test_terrain_cache_trim();
    test_terrain_submission_guard();test_pixel_buffer_during_copy();test_feature_guards();test_native_patches();test_import_installation_rollback();
    test_vfx_native_vertices();test_native_after_layered();
    puts("PASS: cache and mip handling, late GL state guards, import rollback, 16 settings combinations, 8 patch groups, native VFX vertices and restoration.");
    return 0;
}
