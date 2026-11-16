-- Focused real-game parsing/identity checks; normal gameplay never loads this.
local api=coe5;local env=assert(api.readIni('request')).environment
local out={environment=env};local started,opened=false,false
local function save()assert(api.writeIni('configuration-contract-results',out))end
local function same(a,b)
    if type(a)~=type(b)then return false end;if type(a)~='table'then return a==b end
    for k,v in pairs(a)do if not same(v,b[k])then return false end end
    for k in pairs(b)do if a[k]==nil then return false end end;return true
end
local function wanted(state)
    local s={items={},schools={},known=state.known,memorized=state.memorized}
    for _,row in ipairs(state.items)do s.items[row.position+1]=row.id end
    for _,row in ipairs(state.schools)do s.schools[#s.schools+1]={path=row.path,level=row.base}end;return s
end
local function run()
    local c=assert(api.testFieldEditorCapabilities());assert(c.configuration==1 and c.cursedRemoval==1)
    local ok,units=api.openTestField{emptyEditor=true,friends={{594,1}},enemies={{594,1}}};assert(ok);opened=true
    local a,b=units.friends[1],units.enemies[1];local before=assert(api.testFieldLoadoutState(a));local other=assert(api.testFieldLoadoutState(b));assert(before.valid and #before.schools>0)
    assert(api.setTestFieldConfiguration(a,wanted(before)))
    local calls=0
    local proxy=setmetatable({},{__index=function(_,key)calls=calls+1;return wanted(before)[key]end})
    local accepted,why,detail=api.setTestFieldConfiguration(a,proxy)
    assert(not accepted and why=='bad_item_positions' and calls==0 and detail.rollbackVerified,'Configuration parsing invoked a caller metatable')
    out.raw_top={status='PASS',callbacks=calls,refusal=why,rollback_verified=1}
    local request=wanted(before);local first=request.schools[1]
    request.schools[1]=setmetatable({},{__index=function(_,key)calls=calls+1;return first[key]end})
    accepted,why,detail=api.setTestFieldConfiguration(a,request)
    assert(not accepted and why=='bad_school_level' and calls==0 and detail.rollbackVerified,'School-row parsing invoked a caller metatable')
    out.raw_school={status='PASS',callbacks=calls,refusal=why,rollback_verified=1}
    assert(same(before,api.testFieldLoadoutState(a)) and same(other,api.testFieldLoadoutState(b)))
    out.identity={status='PASS',native_id=a,generation=before.generation,other_actor_unchanged=1}
end
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local id=next(...);if started then return id end;started=true
    local ok,why=xpcall(run,tostring)
    if opened then local clean,reason=api.closeTestField();if not clean then ok=false;why='Cleanup unverified: '..tostring(reason)end end
    out.complete={status=ok and 'PASS' or 'FAIL',detail=ok and 'Actual native configuration parser and readback' or why};save();return id
end,-100)
