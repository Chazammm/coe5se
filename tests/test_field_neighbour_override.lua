-- Enable the adjacent test fixture in a fresh, isolated profile.
local api=coe5
local started=false
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local result=next(...)
    if started then return result end
    started=true
    local ok,why=xpcall(function()
        local info=assert(api.testFieldTerrainInfo(223))
        assert(not info.available and info.reason=='planar_neighbour_changed')
        for _,id in ipairs(api.testFieldTerrains()) do assert(id~=223) end
        local accepted,reason=api.openTestField{terrain=223,emptyEditor=true}
        assert(not accepted and reason=='planar_neighbour_changed')
        assert(api.openTestField{terrain=0,emptyEditor=true})
        assert(api.closeTestField())
    end,tostring)
    assert(api.writeIni('results',{complete={status=ok and 'PASS' or 'FAIL',detail=why or 'Changed neighbour refused before borrowing the world'}}))
    return result
end,-100)
