-- Opt-in native probe for Windows CoE5 5.43 in a disposable test profile.
local ran,fighting=false,false
coe5.wrap('BATTLE_WAIT',function(next,...)
    local action=next(...)
    if fighting and action==0 and coe5.get('battleDelay')>2 then return 1 end
    return action
end)
coe5.wrap('MAIN_MENU_WAIT',function(next,...)
    local action=next(...)
    if ran then return action end
    ran=true
    local out,opened={},false
    local ok,why=xpcall(function()
        assert(coe5.testFieldEditorCapabilities().unitRetirement==1)
        local spear=assert(coe5.findMonster('Spearman'))
        local troll=assert(coe5.findMonster('Ageless Troll'))
        local accepted,units=coe5.openTestField{terrain=0,emptyEditor=true,friends={{spear,1}},enemies={{spear,2}}}
        opened=accepted;assert(accepted,units)
        local added=assert(coe5.addTestFieldUnits('enemies',{{troll,1}}))
        local unit=assert(added[1])
        local move=coe5.moveTestFieldUnit(unit,coe5.get('battleWidth')-1,19)
        assert(move==false)
        assert(coe5.removeTestFieldUnit(unit))
        local row=assert(coe5.testFieldUnitState(unit))
        assert(row.alive==false and row.hp==-9999 and row.worldX==-1 and row.worldY==-1)
        assert(#row.anchors==0 and #row.occupied==0)
        fighting=true;assert(coe5.fightTestField()~=false);fighting=false
        assert(coe5.get('battleRound')>0)
        local final=assert(coe5.testFieldFinalState())
        for _,other in ipairs(final.units)do
            assert(other.monster~=troll or not other.alive,'Retired Troll returned to combat')
        end
        assert(coe5.closeTestField());opened=false
        local life=assert(coe5.testFieldLifecycle());assert(not life.open and life.complete)
        for _,id in ipairs(units.friends)do
            assert(coe5.get('unitHp',id)==-9999 and coe5.get('unitX',id)==-1 and coe5.get('unitY',id)==-1)
        end
        out.retirement={status='PASS',native_battle=1,cleanup=1}
    end,tostring)
    fighting=false
    if opened then coe5.closeTestField()end
    out.complete={status=ok and'PASS'or'FAIL',detail=tostring(why or'Detached native record remained retired')}
    assert(coe5.writeIni('results',out));return action
end,-100)
