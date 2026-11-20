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
        local tested={}
        assert(api.openTestField({emptyEditor=true,terrain=0}))
        opened=true
        local previousSession=assert(api.testFieldSceneryState()).session
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
            assert(row.width==1 and row.height==1 and row.standable==kind.standable)
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
            tested[kind.monster]=true
        end
        for _,monster in ipairs({1394,1395,1404,1446,1448,1462,1511,1634}) do assert(tested[monster],monster) end
        local actor=assert(api.addTestFieldUnits('friends',{{ordinary,1}}))[1]
        assert(api.moveTestFieldUnit(actor,10,10))
        for _,kind in ipairs(kinds.types) do
            if kind.standable then
                local object=assert(api.addTestFieldScenery(kind.monster,10,10))
                local state=assert(api.testFieldSceneryState())
                local generation=state.objects[1].generation
                assert(api.battleUnitAt(10,10)==actor and api.battleObjectAt(10,10)==object)
                local removed,why=api.removeTestFieldScenery(object,state.session,generation)
                assert(not removed and why=='scenery_occupied' and api.battleUnitAt(10,10)==actor)
                assert(api.moveTestFieldUnit(actor,11,10))
                assert(api.moveTestFieldUnit(actor,10,10))
                assert(api.moveTestFieldUnit(actor,11,10))
                assert(api.removeTestFieldScenery(object,state.session,generation))
                assert(api.battleObjectAt(10,10)<0 and api.moveTestFieldUnit(actor,10,10))
            end
        end
        assert(api.closeTestField())
        opened=false
        assert(assert(api.testFieldLifecycle()).complete)
        assert(api.openTestField({emptyEditor=true,terrain=0}))
        opened=true
        local wall=assert(api.addTestFieldScenery(1404,10,10))
        local state=assert(api.testFieldSceneryState())
        local generation=state.objects[1].generation
        assert(previousSession~=state.session)
        local stale,reason=api.moveTestFieldScenery(wall,previousSession,generation,11,10)
        assert(not stale and reason=='stale_scenery')
        assert(api.battleObjectAt(10,10)==wall and api.battleObjectAt(11,10)<0)
        assert(api.removeTestFieldScenery(wall,state.session,generation))
        assert(api.closeTestField() and assert(api.testFieldLifecycle()).complete)
        opened=false
    end,tostring)
    if opened then api.closeTestField() end
    assert(api.writeIni('results',{complete={status=ok and 'PASS' or 'FAIL',detail=why or 'Owned scenery native contract verified'}}))
    return value
end,-100)
