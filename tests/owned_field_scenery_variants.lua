-- Opt-in native coverage for each exact stock identity. Run in an isolated,
-- muted Windows 5.43 game with request.ini [run] only=plain or castle.
local api=coe5
local mode=assert(api.readIni('request')).run.only
if mode~='plain' and mode~='castle' then return end
local terrain=mode=='castle' and 22 or 0
local ran=false

local function emptyCells()
    local result={}
    for x=8,20 do for y=6,14 do
        if api.battleObjectAt(x,y)<0 and api.battleUnitAt(x,y)<0 then result[#result+1]={x,y} end
        if #result==4 then return result end
    end end
    error('Four empty test cells were not found')
end

local function objectState(unit,kind,x,y)
    local state=assert(api.testFieldSceneryState())
    for _,row in ipairs(state.objects) do
        if row.unit==unit then
            assert(row.monster==kind.monster and row.owner==31 and row.anchorLayer=='object')
            assert(row.width==1 and row.height==1 and row.x==x and row.y==y and row.standable==kind.standable)
            assert(api.get('unitMonster',unit)==kind.monster and api.battleObjectAt(x,y)==unit)
            return state.session,row
        end
    end
    error('Authored object state was not found')
end

api.wrap('MAIN_MENU_WAIT',function(next,...)
    local result=next(...)
    if ran then return result end
    ran=true
    local out,total,failed={},0,0
    local opened=false
    local ok,why=xpcall(function()
        assert(api.openTestField({emptyEditor=true,terrain=terrain,minimumWidth=50}))
        opened=true
        local spear=assert(api.findMonster('Spearman'))
        local a=assert(api.addTestFieldUnits('friends',{{spear,1}}))[1]
        local b=assert(api.addTestFieldUnits('enemies',{{spear,1}}))[1]
        local native=assert(api.testFieldSceneryTypes())
        assert(native.version==1)
        assert(#native.types==143,'Stock per-ID catalog is incomplete')
        local gate,gateReason=api.addTestFieldScenery(1436,7,7)
        assert(not gate and gateReason=='unsupported_scenery')
        for _,kind in ipairs(native.types) do
            local id=kind.monster
            local cover,range=api.get('monsterAbility',id,0x198),api.get('monsterAbility',id,0x1a6)
            local notes={role=kind.role,id=id,sprite=api.get('monsterSprite',id),cover=cover,range=range}
            local tested,detail=xpcall(function()
                local cells=emptyCells()
                local x,y=table.unpack(cells[1])
                local tx,ty=table.unpack(cells[2])
                local px,py=table.unpack(cells[3])
                local fx,fy=table.unpack(cells[4])
                local unit=assert(api.addTestFieldScenery(id,x,y))
                local session,row=objectState(unit,kind,x,y)
                assert(api.battleUnitAt(x,y)<0)
                local _,blocked=api.addTestFieldScenery(id,x,y)
                assert(blocked=='blocked')
                assert(api.moveTestFieldScenery(unit,session,row.generation,tx,ty))
                local _,moved=objectState(unit,kind,tx,ty)
                assert(moved.generation==row.generation and moved.hp==row.hp and api.battleObjectAt(x,y)<0)
                local copy=assert(api.addTestFieldScenery(id,px,py))
                local _,copied=objectState(copy,kind,px,py)
                assert(copy~=unit and copied.generation~=row.generation and copied.hp==row.hp)
                assert(api.removeTestFieldScenery(copy,session,copied.generation))
                assert(api.battleObjectAt(px,py)<0)
                if kind.role~='other' then
                    for _,actor in ipairs({a,b}) do
                        assert(api.battleCanStand(actor,tx,ty,0)==kind.standable,'native stand mode 0')
                        assert(api.battleCanStand(actor,tx,ty,4)==kind.standable,'native stand mode 4')
                        local movedActor=api.moveTestFieldUnit(actor,tx,ty)
                        assert((movedActor==true)==kind.standable,'native editor deployment')
                        if kind.standable then
                            assert(api.battleUnitAt(tx,ty)==actor and api.battleObjectAt(tx,ty)==unit)
                            assert(api.battleWallCover(actor,0)==cover,'native cover')
                            assert(api.battleSquareAbility(actor,0x1a6)==range,'native missile-range bonus')
                            local _,occupied=api.removeTestFieldScenery(unit,session,row.generation)
                            assert(occupied=='scenery_occupied')
                            assert(api.moveTestFieldUnit(actor,fx,fy))
                            fx,fy=px,py
                        end
                    end
                end
                assert(api.removeTestFieldScenery(unit,session,row.generation))
                assert(api.battleObjectAt(tx,ty)<0 and api.get('unitHp',unit)==-9999)
                assert(not api.removeTestFieldScenery(unit,session,row.generation))
                local rebuilt=assert(api.addTestFieldScenery(id,x,y))
                local _,restored=objectState(rebuilt,kind,x,y)
                assert(restored.generation~=row.generation and restored.hp==row.hp)
                assert(api.removeTestFieldScenery(rebuilt,session,restored.generation))
                notes.hp=row.hp
            end,tostring)
            notes.status=tested and 'PASS' or 'FAIL'
            notes.detail=detail
            out['variant'..id]=notes
            if not tested then failed=failed+1 end
            total=total+1
            -- A refusal must not contaminate later variant results.
            local state=assert(api.testFieldSceneryState())
            for _,row in ipairs(state.objects) do
                assert(api.moveTestFieldUnit(a,6,4))
                assert(api.moveTestFieldUnit(b,7,4))
                assert(api.removeTestFieldScenery(row.unit,state.session,row.generation))
            end
        end
        assert(api.closeTestField())
        opened=false
        assert(assert(api.testFieldLifecycle()).complete)
    end,tostring)
    if opened then api.closeTestField() end
    out.complete={status=ok and failed==0 and 'PASS' or 'FAIL',detail=why or 'Per-ID scenery lifecycle',tested=total,failed=failed}
    assert(api.writeIni('results',out))
    return result
end,-100)
