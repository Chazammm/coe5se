-- Run in a fresh, muted CoE5 5.43 profile with the experimental Extender.
local A=coe5
local done=false
A.wrap('MAIN_MENU_WAIT',function(next,...)
    local value=next(...)
    if done then return value end
    done=true
    local out={environment=A.readIni('launcher-context').environment}
    local ok,why=xpcall(function()
        local none,reason=A.testFieldTerrainInfo(-1)
        assert(none==nil and reason=='bad_terrain');assert(not A.testFieldTerrainInfo(999999999999))
        local advertised={}
        for _,id in ipairs(A.testFieldTerrains()) do advertised[id]=true end
        for id=0,A.get('terrainLastIndex') do
            local item=A.testFieldTerrainInfo(id)
            if item then
                assert(item.id==id and item.name==A.get('terrainName',id))
                assert(item.flags==A.get('terrainFlags',id))
                assert(item.fortParts&255==A.get('terrain',id,'batmap'))
                assert(item.available==(advertised[id]==true))
                if item.fortParts~=0 or (id>=209 and id<=223) then
                    out['terrain'..id]={name=item.name,flags=('%016x'):format(item.flags),parts=item.fortParts,available=item.available and 1 or 0,selector=item.fortParts&255}
                end
            end
        end
        assert(A.openTestField{terrain=0,emptyEditor=true})
        for _,id in ipairs({1236,1436,1437,1438,1439,1440,1441,1533,1551,1552}) do
            local f=assert(A.testFieldFootprint(id))
            out['gate'..id]={name=A.get('monsterName',id),width=f.width,height=f.height}
        end
        assert(A.closeTestField())
    end,tostring)
    if not ok then A.closeTestField() end
    out.complete={status=ok and 'PASS' or 'FAIL',detail=why or 'Exact loaded fortification data and gate footprints'}
    assert(A.writeIni('results',out))
    return value
end,-100)
