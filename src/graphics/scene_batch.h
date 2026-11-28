#pragma once
static _Thread_local unsigned mod_scene_vfx_pass;
static void coe5seGraphicsDrawScene(void (*draw)(void)) __attribute__((unused));

// @platform-split scene-batching: Windows field, particle and projectile calls are mapped; extra Mac scene routes remain disabled.
#ifdef _WIN32
static _Thread_local int mod_scene_field_arguments[3];

static void mod_scene_field_bridge(void) {
    ((void (*)(int,int,int))(detail_base+RVA_BATTLE_FIELD_DRAW))(mod_scene_field_arguments[0],mod_scene_field_arguments[1],mod_scene_field_arguments[2]);
}

static void mod_scene_field(int a,int b,int c) {
    int previous[3];
    memcpy(previous,mod_scene_field_arguments,sizeof(previous));
    mod_scene_field_arguments[0]=a;mod_scene_field_arguments[1]=b;mod_scene_field_arguments[2]=c;
    coe5seGraphicsDrawScene(mod_scene_field_bridge);
    memcpy(mod_scene_field_arguments,previous,sizeof(previous));
}

static _Thread_local uintptr_t mod_scene_particle_arguments[2];
static _Thread_local int mod_scene_particle_layer;
static void mod_scene_particle_bridge(void) {
    ((void (*)(uintptr_t,uintptr_t,int))(detail_base+RVA_PARTICLES_DRAW))(mod_scene_particle_arguments[0],mod_scene_particle_arguments[1],mod_scene_particle_layer);
}
static void mod_scene_particles(uintptr_t a,uintptr_t b,int layer) {
    if(!mod_scenes_enabled || !mod_scene_kind || *(int *)(detail_base+RVA_PARTICLES_LAST)<0){
        ((void (*)(uintptr_t,uintptr_t,int))(detail_base+RVA_PARTICLES_DRAW))(a,b,layer);return;
    }
    uintptr_t previous[2]={mod_scene_particle_arguments[0],mod_scene_particle_arguments[1]};
    int previous_layer=mod_scene_particle_layer;unsigned previous_pass=mod_scene_vfx_pass;
    mod_scene_particle_arguments[0]=a;mod_scene_particle_arguments[1]=b;mod_scene_particle_layer=layer;mod_scene_vfx_pass=1;
    coe5seGraphicsDrawScene(mod_scene_particle_bridge);
    mod_scene_particle_arguments[0]=previous[0];mod_scene_particle_arguments[1]=previous[1];mod_scene_particle_layer=previous_layer;mod_scene_vfx_pass=previous_pass;
}
static void mod_scene_projectiles(void) {
    if(!mod_scenes_enabled || !mod_scene_kind || *(int *)(detail_base+RVA_FLY_WEAPONS_LAST)<0){
        ((void (*)(void))(detail_base+RVA_FPS_SCENE_PROJECTILES_DRAW))();return;
    }
    unsigned previous_pass=mod_scene_vfx_pass;mod_scene_vfx_pass=2;
    coe5seGraphicsDrawScene((void (*)(void))(detail_base+RVA_FPS_SCENE_PROJECTILES_DRAW));
    mod_scene_vfx_pass=previous_pass;
}
#endif

static int mod_scene_wait(int (*next)(int,int),int kind,int a,int b) {
    int previous=mod_scene_kind;
    mod_scene_kind=kind;
    int result=next(a,b);
    mod_scene_kind=previous;
    return result;
}

#define SCENE_WAIT(name,kind) \
    static int (*mod_next_##name)(int,int); \
    static int mod_wait_##name(int a,int b){return mod_scene_wait(mod_next_##name,kind,a,b);}
SCENE_WAIT(battle,1)
SCENE_WAIT(battle_editor,1)
SCENE_WAIT(army,2)
SCENE_WAIT(view_army,2)
SCENE_WAIT(square_info,2)
SCENE_WAIT(unit_list,2)
SCENE_WAIT(recruit,2)
#undef SCENE_WAIT

static void mod_install_scene_waits(const PluginApi *api) {
    if(!api->wrap){mod_log("fps_mod scene_batching disabled reason=wrapper_api_unavailable");return;}
    unsigned installed=0;
#define SCENE_WRAP(site,name) \
    do {if(api->wrap(site,(void *)mod_wait_##name,(void **)&mod_next_##name))installed++;}while(0)
    SCENE_WRAP(SITE_BATTLE_WAIT,battle);
    SCENE_WRAP(SITE_BATTLE_EDITOR_WAIT,battle_editor);
    SCENE_WRAP(SITE_ARMY_WAIT,army);
    SCENE_WRAP(SITE_VIEW_ARMY_WAIT,view_army);
    SCENE_WRAP(SITE_SQUARE_INFO_WAIT,square_info);
    SCENE_WRAP(SITE_UNIT_LIST_WAIT,unit_list);
    SCENE_WRAP(SITE_RECRUIT_WAIT,recruit);
#undef SCENE_WRAP
    mod_scenes_enabled=installed!=0;
    char line[180];snprintf(line,sizeof(line),"fps_mod scene_batching enabled=%d wait_routes=%u original_order=1",mod_scenes_enabled,installed);mod_log(line);
}

static void coe5seGraphicsDrawScene(void (*draw)(void)) {
    if(!ta_game_thread || !mod_scenes_enabled || !mod_scene_kind || mod_scene_drawing){draw();return;}
    int previous_highest=in_highest,previous_scope=decor_scope,previous_scene_scope=arrays_scene_scope,previous_replace=ta_scene_replace,previous_native_texture=arrays_native_texture;
    arrays_end_frame();mod_scene_drawing=1;in_highest=decor_scope=arrays_scene_scope=ta_scene_replace=1;arrays_native_texture=mod_scene_vfx_pass!=0;
    array_texture_begin_frame();arrays_begin_frame();
    long long before_vertices=arrays_vertex_count;
    draw();
    arrays_end_frame();mod_scene_drawing=0;in_highest=previous_highest;decor_scope=previous_scope;arrays_scene_scope=previous_scene_scope;ta_scene_replace=previous_replace;arrays_native_texture=previous_native_texture;
    static unsigned reported[3];
    if(arrays_vertex_count>before_vertices && !(reported[mod_scene_vfx_pass]&(1u<<mod_scene_kind))){
        char line[240];snprintf(line,sizeof(line),"fps_mod scene_renderer_ready scene=%s quality=%d pass=%s original_uv=1 original_mips=1 original_order=1",mod_scene_kind==1?"battle":"location",*(int *)(detail_base+RVA_GRAPHICS_DETAIL),mod_scene_vfx_pass==1?"particles":(mod_scene_vfx_pass==2?"projectiles":"field"));mod_log(line);reported[mod_scene_vfx_pass]|=1u<<mod_scene_kind;
    }
    mod_log_graphics();mod_log_fallbacks();
}
