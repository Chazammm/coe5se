-- Isolated Windows native context acceptance. Set the request's only value to a terrain ID.
local A=coe5
local out={environment=A.readIni('launcher-context').environment}
local started=false
A.wrap('MAIN_MENU_WAIT',function(next,...)
    local result=next(...)
    if started then return result end
    started=true
    local id=assert(tonumber(A.readIni('request').run.only))
    local function world()
        local values={}
        for _,key in ipairs({'worldW','worldH','planeLastIndex','activePlayer'}) do values[#values+1]=tostring(A.get(key)) end
        for _,plane in ipairs({0,8}) do
            for _,key in ipairs({'planeName','planeX','planeY','planeWidth','planeHeight'}) do values[#values+1]=tostring(A.get(key,plane)) end
        end
        for x=0,2 do for y=0,2 do
            for _,key in ipairs({'squareTerrain','squareGround','squareTemperature','squarePlane','squareArmy','squareOwner'}) do
                values[#values+1]=tostring(A.get(key,x,y))
            end
        end end
        return table.concat(values,',')
    end
    local before=world()
    local ok,why=xpcall(function()
        local info=assert(A.testFieldTerrainInfo(id))
        assert(info.available,info.reason)
        local opened,units=A.openTestField{terrain=id,emptyEditor=true}
        assert(opened,units)
        local c=assert(A.testFieldEnvironmentState())
        assert(c.terrain==id and c.nativeTemplate==info.nativeTemplate and c.fortParts==info.fortParts and c.plane==info.plane)
        local life=assert(A.testFieldLifecycle())
        assert(life.complete)
        local gates,landscape,largeScenery=0,0,0
        local gateTypes={}
        for _,row in ipairs(life.created) do
            if row.creationSource=='native_gate_replacement' then
                assert(row.owner==24 and row.kind=='structure' and row.provenanceVerified)
                gates=gates+1
                gateTypes[row.monster]=true
            elseif row.creationSource=='native_map_loader' or row.creationSource=='native_terrain_placement' then
                landscape=landscape+1
                local footprint=assert(A.testFieldFootprint(row.monster))
                if footprint.width*footprint.height>1 then largeScenery=largeScenery+1 end
            end
        end
        assert(gates>0 and landscape>0,'Native fort did not create a verified gate and landscape')
        local types={}
        for monster in pairs(gateTypes) do types[#types+1]=monster end
        table.sort(types)
        out.context={terrain=id,template=c.nativeTemplate,gates=gates,gate_types=table.concat(types,','),landscape=landscape,
            large_scenery=largeScenery,plane=c.plane,ground=c.ground,
            fort_parts=c.fortParts,temperature=c.temperature,siege=c.siege and 1 or 0,tower=c.tower and 1 or 0}
    end,tostring)
    local cleaned,detail=A.closeTestField()
    if world()~=before or ok and not cleaned then
        ok=false;why='World or owned-field cleanup mismatch: '..tostring(detail)
    end
    out.complete={status=ok and 'PASS' or 'FAIL',detail=why or 'Generated native gate provenance and exact context'}
    assert(A.writeIni('results',out))
    return result
end,-100)
