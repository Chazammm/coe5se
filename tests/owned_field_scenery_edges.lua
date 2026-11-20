-- Opt-in audit for malformed arguments, shared edge cells, mod overrides and dense reads.
local api=coe5
local mode=assert(api.readIni('request')).run.only
local started=false
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local value=next(...)
    if started then return value end
    started=true
    local results={}
    local ok,why=xpcall(function()
        assert(api.openTestField({emptyEditor=true,terrain=0,minimumWidth=mode=='dense' and 600 or 50}))
        local width=api.get('battleWidth')
        if mode=='override' then
            local modified
            for id=1396,1409 do if api.get('monsterHp',id)==177 then modified=id;break end end
            assert(modified,'C5M health override fixture did not load')
            local catalog=assert(api.testFieldSceneryTypes())
            local listed=false
            for _,kind in ipairs(catalog.types) do
                if kind.monster==modified then listed=true end
            end
            local id,reason=api.addTestFieldScenery(modified,8,8)
            results.override={monster=modified,listed=listed and 1 or 0,placed=id and 1 or 0,
                hp=id and api.get('unitHp',id) or 0}
            assert(not listed and not id and reason=='unsupported_scenery',
                ('Modified Stone Wall #%d was listed=%s, placed=%s, refusal=%s'):format(modified,tostring(listed),tostring(id),tostring(reason)))
            assert(api.addTestFieldScenery(1512,8,8),'An untouched stock type must still work')
        elseif mode=='dense' then
            local n=0
            for x=5,width-6 do
                if api.battleObjectAt(x,3)<0 then
                    assert(api.addTestFieldScenery(1404,x,3))
                    n=n+1
                    if n==400 then break end
                end
            end
            assert(n==400)
            local start=api.get('ticksMs')
            for _=1,512 do assert(#assert(api.testFieldSceneryState()).objects==n) end
            results.measurement={objects=n,width=width,reads=512,elapsed_ms=api.get('ticksMs')-start}
        else
            local function refuses(reason,fn,...)
                local value,why=fn(...)
                assert(value==nil and why==reason,tostring(why))
            end
            local elephant=assert(api.findMonster('Elephant'))
            local army=assert(api.addTestFieldUnits('friends',{{elephant,1}}))[1]
            local x
            for anchor=width-2,0,-1 do
                if api.moveTestFieldUnit(army,anchor,18) then x=anchor+1;break end
            end
            assert(x and api.battleUnitAt(x,19)==army,'Elephant does not cover a bottom-edge secondary cell')
            refuses('off_field',api.addTestFieldScenery,1512,width,19)
            refuses('off_field',api.addTestFieldScenery,1512,'8',8)
            refuses('off_field',api.addTestFieldScenery,1512,math.maxinteger,1)
            refuses('off_field',api.addTestFieldScenery,1512,8,8.5)
            refuses('blocked',api.addTestFieldScenery,1404,x,19)
            api.drawTestField()
            refuses('bad_final',api.inspectTestFieldFeature,x,19,'false')
            local stair=assert(api.addTestFieldScenery(1512,x,19))
            local state=assert(api.testFieldSceneryState())
            local row=state.objects[1]
            assert(row.unit==stair and row.x==x and row.y==19)
            refuses('scenery_occupied',api.removeTestFieldScenery,stair,state.session,row.generation)
            refuses('scenery_occupied',api.moveTestFieldScenery,stair,state.session,row.generation,10,10)
            refuses('not_a_verified_scenery_or_structure',api.inspectTestFieldFeature,x,19,false)
            refuses('stale_scenery',api.removeTestFieldScenery,stair,state.session,tostring(row.generation))
            local continued=coroutine.resume(coroutine.create(function() api.removeTestFieldScenery(stair,state.session,row.generation) end))
            assert(not continued,'Scenery mutated through a coroutine')
            assert(api.moveTestFieldUnit(army,8,8))
            refuses('blocked',api.moveTestFieldScenery,stair,state.session,row.generation,x,19)
            assert(api.moveTestFieldScenery(stair,state.session,row.generation,10,10))
            assert(api.addTestFieldScenery(1404,11,10))
            refuses('blocked',api.moveTestFieldScenery,stair,state.session,row.generation,11,10)
            local after=assert(api.testFieldSceneryState()).objects[1]
            assert(after.unit==stair and after.generation==row.generation and after.hp==row.hp)
            assert(api.removeTestFieldScenery(stair,state.session,row.generation))
            refuses('stale_scenery',api.removeTestFieldScenery,stair,state.session,row.generation)
        end
        assert(api.closeTestField())
        assert(assert(api.testFieldLifecycle()).complete)
    end,tostring)
    results.complete={status=ok and 'PASS' or 'FAIL',detail=why or mode}
    assert(api.writeIni('results',results))
    return value
end,-100)
