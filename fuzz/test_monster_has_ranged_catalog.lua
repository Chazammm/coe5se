-- Run as a private main-menu Lua mod; results use only that mod's INI storage.
local api=coe5
local request=assert(api.readIni('request')).environment
local out={environment=request};local started,opened,fatal=false,false,false
local function save()assert(api.writeIni('results',out))end
local function guards()
    assert(api.unitNoLand(0)==nil and api.countFreeCommanders(0)==nil and api.unitFireResistance(0)==nil)
end
local function catalog()
    local rows={};local total,ranged=0,0
    local last=assert(api.get('monsterLastIndex'))
    for id=0,last do
        local name=api.get('monsterName',id)
        if type(name)=='string' and name~=''then
            local value=api.monsterHasRanged(id);assert(type(value)=='boolean',tostring(id))
            rows[id]=value;total=total+1;if value then ranged=ranged+1 end
        end
    end
    for _,id in ipairs({-1,last+1,2147483647})do assert(api.monsterHasRanged(id)==nil)end
    return rows,total,ranged
end
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local result=next(...);if started then return result end;started=true
    local ok,why=xpcall(function()
        guards();local expected,total,ranged=catalog();assert(total>0 and ranged>0)
        local accepted,detail=api.openTestField{commander=api.findMonster('Spearman'),enemies={{api.findMonster('Goblin'),1}}}
        assert(accepted,tostring(detail));opened=true
        local during,dt,dr=catalog();assert(dt==total and dr==ranged)
        for id,value in pairs(expected)do assert(during[id]==value,'Context mismatch: '..id)end
        assert(api.closeTestField());opened=false;guards()
        local after,at,ar=catalog();assert(at==total and ar==ranged)
        for id,value in pairs(expected)do assert(after[id]==value,'After-close mismatch: '..id)end
        out.counts={loaded_named_monsters=total,ranged=ranged,context_mismatches=0}
    end,tostring)
    if opened then local closed,detail=api.closeTestField();opened=false;if not closed then fatal=true;why=tostring(why)..'; cleanup='..tostring(detail)end end
    out.catalog_main_menu_field_and_after_close={status=ok and not fatal and 'PASS' or 'FAIL',detail=ok and 'All loaded named monsters; native classifier and unrelated context guards' or why}
    out.complete={status=ok and not fatal and 'PASS' or 'FAIL'};save();return result
end,-100)
