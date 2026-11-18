#pragma once

#define SAVE_MSET_VERSION 1
#define HOST_SEED_PICK    "lan_script_seed"

static unsigned lanScriptSeedNow(void);
static void takeHostScriptSeed(unsigned seed);

typedef struct { char mod[MOD_NAME_MAX]; char name[SETTING_NAME_SIZE]; int value, refused; } HostSettingPick;
static HostSettingPick *hostSettingPicks;
static int hostSettingPickCount, hostSettingPickCapacity;

static int hostSettingValueOf(const ModSetting *s) {
    for (int i = 0; i < hostSettingPickCount; i++) {
        HostSettingPick *p = &hostSettingPicks[i];
        if (strcmp(p->mod, s->mod) || strcmp(p->name, s->name)) continue;
        int max = s->optionCount ? s->optionCount - 1 : 1;
        if (s->isKey ? keyValueIsValid(p->value) : p->value >= 0 && p->value <= max) return p->value;
        if (!p->refused++)
            refuseIniLine(s->isKey ? "key" : s->optionCount ? "choice" : "onoff", s->mod, "out_of_range", "name=%s value=%d max=%d from=host", s->name, p->value,
                          s->isKey ? -1 : max);
        return s->defaultValue;
    }
    return s->defaultValue;
}

static void addHostSettingsChunk(SaveList *list) {
    if (netRoleNow() != NET_ROLE_HOST) return;
    SaveBuffer b = {0};
    int count = 0;
    saveBufferInt(&b, SAVE_MSET_VERSION);
    size_t countAt = b.size;
    saveBufferInt(&b, 0);
    for (int i = 0; i < settingCount; i++) {
        const ModSetting *s = settings[i];
        if (!s->declared || s->rejected || s->value == s->defaultValue) continue;
        if (!strcmp(s->mod, COE5SE_MOD) || modClassNow(s->mod) != LUA_CLASS_CONTENT) continue;
        saveBufferString(&b, s->mod);
        saveBufferString(&b, s->name);
        saveBufferInt(&b, s->value);
        count++;
    }
    saveBufferString(&b, COE5SE_MOD);
    saveBufferString(&b, HOST_SEED_PICK);
    saveBufferInt(&b, (int)lanScriptSeedNow());
    count++;
    if (b.bytes) memcpy(b.bytes + countAt, (const unsigned char[4]){(unsigned char)count, (unsigned char)(count >> 8), (unsigned char)(count >> 16), (unsigned char)(count >> 24)}, 4);
    saveBufferInt(&b, (int)b.size + 8);
    saveBufferInt(&b, SAVE_TAG_MSET);
    saveListAdd(list, SAVE_TAG_MSET, &b);
}

static int hostSettingPicksMax(int size) {
    int fit = (size - 16) / 6;
    return fit < settingCount + 1 ? fit : settingCount + 1;
}

static const char *useHostSettingsPayload(const unsigned char *payload, int size) {
    if (size < 16) return "bad";
    SaveCursor c = {payload, payload + size - 8, 0};
    int version = saveCursorInt(&c), count = saveCursorInt(&c);
    if (c.failed || version != SAVE_MSET_VERSION || count < 0 || count > hostSettingPicksMax(size)) return "bad";
    HostSettingPick *picks = count ? calloc((size_t)count, sizeof *picks) : NULL;
    if (count && !picks) return "bad";
    for (int i = 0; i < count; i++) {
        saveCursorString(&c, picks[i].mod, sizeof picks[i].mod);
        saveCursorString(&c, picks[i].name, sizeof picks[i].name);
        picks[i].value = saveCursorInt(&c);
    }
    if (c.failed || c.at != c.end) {
        free(picks);
        return "bad";
    }
    free(hostSettingPicks);
    hostSettingPicks = picks;
    hostSettingPickCount = hostSettingPickCapacity = count;
    for (int i = 0; i < count; i++)
        if (!strcmp(picks[i].mod, COE5SE_MOD) && !strcmp(picks[i].name, HOST_SEED_PICK)) takeHostScriptSeed((unsigned)picks[i].value);
    return "found";
}

static void readHostSettingsAtOpen(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    int ints[3] = {0}, size = 0;
    long end = !fseek(f, 0, SEEK_END) ? ftell(f) : -1;
    unsigned char *payload = NULL;
    const char *result = "none";
    if (end >= 28 && !fseek(f, end - 12, SEEK_SET)) {
        for (int i = 0; i < 3; i++) readFileInt(f, &ints[i]);
        if (ints[0] == SAVE_TAG_END && !ints[1] && ints[2] == SAVE_LIST_MAGIC) {
            long at = end - 12;
            for (int step = 0; step < 6 && !payload && !strcmp(result, "none"); step++) {
                int trailer[2] = {0};
                if (at < 16 || fseek(f, at - 8, SEEK_SET) || !readFileInt(f, &trailer[0]) || !readFileInt(f, &trailer[1])) break;
                if (trailer[1] == SAVE_TAG_MSET) payload = readTrailedChunk(f, at, SAVE_TAG_MSET, &size, &result);
                else if (trailer[1] == SAVE_TAG_MODC || trailer[1] == SAVE_TAG_MODH || trailer[1] == SAVE_TAG_PLNS ||
                         trailer[1] == SAVE_TAG_FCTN || trailer[1] == SAVE_TAG_VERS)
                    at -= (long)trailer[0] + 8;
                else break;
            }
        }
    }
    fclose(f);
    if (payload) result = useHostSettingsPayload(payload, size);
    free(payload);
    writeResultLog(strcmp(result, "bad"), "host_settings_read", "from=open result=%s%s picks=%d", result, strcmp(result, "bad") ? "" : " reason=bad_chunk", hostSettingPickCount);
}

static void noteClientLoadStart(const char *path) {
    if (netRoleNow() != NET_ROLE_CLIENT) return;
    netClientLoading = 1;
    netClientLoads++;
    readHostSettingsAtOpen(path);
}

static void useHostSettingsFromList(const SaveList *list) {
    netClientLoading = 0;
    if (netRoleNow() != NET_ROLE_CLIENT) return;
    const char *result = "none";
    for (int i = 0; i < list->count; i++)
        if (list->chunks[i].tag == SAVE_TAG_MSET) result = useHostSettingsPayload(list->chunks[i].payload, list->chunks[i].size);
    if (!strcmp(result, "none")) hostSettingPickCount = 0;
    writeResultLog(strcmp(result, "bad"), "host_settings_read", "from=tail result=%s%s picks=%d", result, strcmp(result, "bad") ? "" : " reason=bad_chunk", hostSettingPickCount);
}
