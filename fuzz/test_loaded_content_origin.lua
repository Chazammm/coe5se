-- Explicit real-game acceptance. No campaign, unit creation or random calls.
local api=coe5;local request=assert(api.readIni('request')).environment
local out={environment=request};local started=false
local function save()assert(api.writeIni('results',out))end
local function test(name,fn)
    local ok,why=xpcall(fn,tostring);out[name]={status=ok and 'PASS' or 'FAIL',detail=ok and 'Actual current-build API readback' or why};save()
end
local function itemNamed(name)
    for id=0,api.get('itemLastIndex')do if api.get('itemName',id)==name then return id end end
    error('Installed fixture missing: '..name)
end
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local id=next(...);if started then return id end;started=true
    test('current_upstream_independent_surface',function()
        assert(type(api.loadedContentOrigin)=='function')
        assert(api.testFieldEditorCapabilities==nil and api.setTestFieldConfiguration==nil,'Unrelated editor implementation leaked into independent branch')
        assert(api.monsterHasRanged(api.findMonster('Archer'))==nil,'Independent branch unexpectedly includes Ranged patch')
        assert(api.get('monsterMod',api.findMonster('Shell Priest'))=='CrabCult','Current upstream monster-origin getter was lost')
    end)
    test('observed_active_mod_item_and_explicit_qa_spells',function()
        local item=itemNamed('Abyss Fragment');local origin=assert(api.loadedContentOrigin('item',item))
        assert(origin.id==item and origin.modded and origin.observed and origin.definedBy=='CrabCult' and origin.lastModifiedBy=='CrabCult' and not origin.historyComplete)
        out.crab_item={id=item,kind=origin.kind,creator=origin.definedBy,writer=origin.lastModifiedBy}
        local count=0;local schools={}
        for spell=0,4095 do
            local info=api.spellInfo(spell)
            if info then
                local s=assert(api.loadedContentOrigin('spell',spell))
                if s.modded and s.lastModifiedBy=='ZZZContentOriginQA'then
                    count=count+1;schools[info.path]=true
                    out['mod_spell_'..count]={id=spell,path=info.path,name=info.name,kind=s.kind,writer=s.lastModifiedBy}
                end
            end
        end
        assert(count==2,'Explicit QA spell fixture missing or misattributed');out.spells={observed_qa=count,scope='Installed CrabCult has no custom spells; QA spells are separately authored'}
        for path in pairs(schools)do local s=assert(api.loadedContentOrigin('spellschool',path));assert(s.id==path and type(s.observed)=='boolean');out['school_'..path]={kind=s.kind,observed=s.observed and 1 or 0,modded=s.modded and 1 or 0,scope='School metadata and mod-spell membership are distinct'}end
        local low=assert(api.loadedContentOrigin('item',itemNamed('Amulet of Anti Magic')));assert(low.modded and low.lastModifiedBy=='ZZZContentOriginQA' and low.definedBy==nil,'Low-ID override was not tracked independently of creator')
    end)
    test('unobserved_current_upstream_lua_content_write_becomes_unknown',function()
        local item=itemNamed('Amulet of Anti Magic');local name=api.get('itemName',item)
        local before=assert(api.loadedContentOrigin('item',item));assert(before.observed)
        api.set('itemName',item,'Origin QA temporary')
        local changed=api.get('itemName',item)=='Origin QA temporary'
        local during=api.loadedContentOrigin('item',item)
        api.set('itemName',item,name)
        assert(api.get('itemName',item)==name,'Private fixture restore failed')
        assert(changed,'Current upstream writable item name did not change')
        assert(during and during.kind=='unknown' and not during.observed and not during.modded,'Unobserved Lua content write falsely attributed')
        local after=assert(api.loadedContentOrigin('item',item));assert(after.kind==before.kind and after.observed==before.observed)
        out.lua_write={item=item,invalidated=1,restored=1,scope='Exact record binding, not full writer history'}
    end)
    test('invalid_inputs_and_repeated_queries_are_read_only',function()
        for _,kind in ipairs({'item','spell','spellschool','unsupported'})do
            for _,bad in ipairs({-1,2147483647})do local value,why=api.loadedContentOrigin(kind,bad);assert(value==nil and why=='bad_content')end
        end
        local item=itemNamed('Abyss Fragment');local name=api.get('itemName',item)
        for i=1,100 do local s=assert(api.loadedContentOrigin('item',item));assert(s.id==item and s.modded and api.get('itemName',item)==name)end
        assert(api.unitNoLand(0)==nil,'Unrelated no_game guard changed')
    end)
    local pass=true;for _,row in pairs(out)do if type(row)=='table' and row.status=='FAIL'then pass=false end end
    out.complete={status=pass and 'PASS' or 'FAIL',scope='Native standalone content-origin branch; no full Sandbox editor claim'};save()
    return id
end,-100)
