static _Thread_local int decor_scope;
static void (*real_gl_color3)(float,float,float);
static void (*real_gl_begin)(unsigned);
static void (*real_gl_end)(void);
static void (*real_gl_bind)(unsigned,unsigned);
static void (*real_gl_enable)(unsigned);
static void (*real_gl_disable)(unsigned);
static void (*real_gl_blend)(unsigned,unsigned);
static void (*real_gl_alpha)(unsigned,float);
static void (*real_gl_depth_mask)(unsigned char);
static void (*real_gl_tex_env)(unsigned,unsigned,int);
static void (*real_gl_color)(float,float,float,float);
#ifdef _WIN32
static void **gl_import_slot(uintptr_t rva) {return (void **)(detail_base+rva);}
static int replace_gl_import(uintptr_t rva,void *hook,void *saved) {
    void **slot=gl_import_slot(rva);
    DWORD old,unused;
    if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&old)) return 0;
    void *original=InterlockedExchangePointer(slot,hook);
    if(!VirtualProtect(slot,sizeof(void *),old,&unused)) {
        InterlockedCompareExchangePointer(slot,original,hook);
        VirtualProtect(slot,sizeof(void *),old,&unused);
        return 0;
    }
    memcpy(saved,&original,sizeof(original));return 1;
}
#else
static void **gl_import_slot(uintptr_t rva) {return fps_stub_slot(detail_base+rva);}
static int replace_gl_import(uintptr_t rva,void *hook,void *saved) {
    void **slot=gl_import_slot(rva);
    if(!slot || !fps_host || !fps_host->makeDataWritable(slot,sizeof(void *))) return 0;
    memcpy(saved,slot,sizeof(void *));
    __atomic_store_n(slot,hook,__ATOMIC_SEQ_CST);return 1;
}
#endif
