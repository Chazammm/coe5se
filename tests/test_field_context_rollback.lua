-- Run in a fresh isolated Windows profile with no campaign active.
local api = coe5
local started = false
local planeFields = {'planeName','planeX','planeY','planeWidth','planeHeight','planeFlags'}
local function world()
    local values = {}
    local function remember(key, ...)
        values[#values+1] = tostring(api.get(key, ...))
    end
    for _,key in ipairs({'worldW','worldH','planeLastIndex','activePlayer'}) do remember(key) end
    for _,plane in ipairs({0,1,8}) do
        for _,key in ipairs(planeFields) do remember(key,plane) end
    end
    for x=0,2 do for y=0,2 do
        for _,key in ipairs({'squareTerrain','squareGround','squareTemperature','squarePlane','squareArmy','squareOwner'}) do
            remember(key,x,y)
        end
    end end
    return table.concat(values,',')
end

api.wrap('MAIN_MENU_WAIT',function(next,...)
    local result = next(...)
    if started then return result end
    started = true
    local last = api.get('planeLastIndex')
    local original = {}
    local before = world()
    local bounds = {'planeX','planeY','planeWidth','planeHeight'}
    api.set('planeLastIndex',math.max(last,1))
    for _,key in ipairs(bounds) do original[key] = api.get(key,1) end
    local ok,why = xpcall(function()
        -- An already registered plane takes precedence over the temporary plane 8.
        api.set('planeWidth',1,0)
        api.set('planeHeight',1,0)
        api.set('planeX',1,0)
        api.set('planeY',1,0)
        api.set('planeWidth',1,3)
        api.set('planeHeight',1,3)
        local obstruction = world()
        local opened,reason = api.openTestField{terrain=223,emptyEditor=true}
        if opened then
            assert(api.closeTestField())
            error('The competing plane did not block staging')
        end
        assert(reason=='terrain_init_failed',reason)
        assert(world()==obstruction,'The refused open changed the competing plane or the borrowed world')
    end,tostring)
    for _,key in ipairs({'planeWidth','planeHeight'}) do api.set(key,1,0) end
    for _,key in ipairs(bounds) do api.set(key,1,original[key]) end
    api.set('planeLastIndex',last)
    if ok then
        ok,why = xpcall(function()
            assert(world()==before,'The probe did not restore its fixture')
            assert(api.openTestField{terrain=223,emptyEditor=true})
            api.set('planeWidth',8,2)
            assert(api.get('planeWidth',8)==2,'The probe could not change the staged plane')
            local canFight,reason = api.fightTestField()
            local closed,closeReason = api.closeTestField()
            assert(closed,closeReason)
            assert(canFight==false and reason=='field_context_mismatch',reason)
            assert(world()==before,'The changed field did not restore the borrowed world on close')
            for _,terrain in ipairs({223,0}) do
                assert(api.openTestField{terrain=terrain,emptyEditor=true})
                assert(api.closeTestField())
                assert(world()==before,'The following field close changed the borrowed world')
            end
        end,tostring)
    end
    assert(api.writeIni('results',{complete={status=ok and 'PASS' or 'FAIL',
        detail=why or 'Staging and changed-context battle refused, borrowed world restored, Nexus and Plain reopened'}}))
    return result
end,-100)
