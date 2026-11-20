-- Opt-in Windows 5.43 smoke test. Install as a disposable local probe in an
-- isolated profile and provide its request.ini with [run] only=scenery.
local api=coe5
local request=api.readIni('request')
if not request or not request.run or request.run.only~='scenery' then return end
local started=false
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local value=next(...)
    if started then return value end
    started=true
    local opened=false
    local ok,why=xpcall(function()
        local kinds=assert(api.testFieldSceneryTypes())
        assert(kinds.version==1)
        assert(api.openTestField({emptyEditor=true,terrain=0}))
        opened=true
        local ordinary=assert(api.findMonster('Spearman'))
        local unsupported,reason=api.addTestFieldScenery(ordinary,0,0)
        assert(not unsupported and reason=='unsupported_scenery')
        for _,kind in ipairs(kinds.types) do
            local points={}
            for x=0,api.get('battleWidth')-1 do
                for y=0,19 do if api.battleObjectAt(x,y)<0 and api.battleUnitAt(x,y)<0 then
                    points[#points+1]={x=x,y=y}
                end end
                if #points>=2 then break end
            end
            local a,b=points[1],points[2]
            local id=assert(api.addTestFieldScenery(kind.monster,a.x,a.y))
            local state=assert(api.testFieldSceneryState())
            assert(#state.objects==1)
            local row=state.objects[1]
            assert(row.unit==id and row.monster==kind.monster and row.owner==31 and row.anchorLayer=='object')
            assert(api.battleObjectAt(a.x,a.y)==id and api.battleUnitAt(a.x,a.y)<0)
            assert(not api.moveTestFieldScenery(id,state.session,row.generation+1,b.x,b.y))
            assert(api.moveTestFieldScenery(id,state.session,row.generation,b.x,b.y))
            assert(api.battleObjectAt(a.x,a.y)<0 and api.battleObjectAt(b.x,b.y)==id)
            local moved=assert(api.testFieldSceneryState()).objects[1]
            assert(moved.generation==row.generation and moved.hp==row.hp)
            local fight,refusal=api.fightTestField()
            assert(not fight and refusal=='missing_team_a',refusal)
            assert(api.removeTestFieldScenery(id,state.session,row.generation))
            assert(api.battleObjectAt(b.x,b.y)<0 and #assert(api.testFieldSceneryState()).objects==0)
            assert(not api.removeTestFieldScenery(id,state.session,row.generation))
        end
        assert(#kinds.types>=2)
        assert(api.closeTestField())
        opened=false
        assert(assert(api.testFieldLifecycle()).complete)
    end,tostring)
    if opened then api.closeTestField() end
    assert(api.writeIni('results',{complete={status=ok and 'PASS' or 'FAIL',detail=why or 'Owned scenery native contract verified'}}))
    return value
end,-100)
