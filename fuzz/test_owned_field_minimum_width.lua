-- Opt-in native probe. Run as a standalone Lua mod in a disposable profile.
local ran=false
coe5.wrap('MAIN_MENU_WAIT',function(next,...)
    local result=next(...)
    if ran then return result end
    ran=true
    local out={}
    local open=false
    local passed,why=xpcall(function()
        local caps=coe5.testFieldEditorCapabilities()
        assert(caps.minimumWidthVersion==1 and caps.minimumWidthMin==32 and caps.minimumWidthMax==1536)
        local spearman=assert(coe5.findMonster('Spearman'))
        local function check(options)
            local accepted,units=coe5.openTestField(options)
            open=accepted
            assert(accepted,tostring(units))
            local state=assert(coe5.testFieldState())
            assert(state.complete and #state.issues==0 and state.height==20)
            for _,row in ipairs(state.units)do assert(row.alive and row.anchorCount==1)end
            assert(coe5.closeTestField());open=false
            local life=assert(coe5.testFieldLifecycle());assert(life.complete and not life.open)
            return state.width
        end
        local function plan(width,count)
            return {terrain=0,emptyEditor=true,minimumWidth=width,friends={{spearman,count}},enemies={{spearman,count}}}
        end
        local baseline=check(plan(nil,1))
        for _,width in ipairs({32,50,70,160})do
            local actual=check(plan(width,1))
            assert(actual>=width and actual>=baseline)
            out['width'..width]={status='PASS',requested=width,observed=actual}
        end
        local dense=check(plan(nil,100))
        assert(dense>=32 and check(plan(32,100))==dense)
        assert(check(plan(nil,1))==baseline)
        out.defaults={status='PASS',small=baseline,dense=dense,dense_with_floor=dense,default_after=baseline}
        local last=coe5.get('unitLastIndex')
        for _,invalid in ipairs({false,31,1537,31.5,'50'})do
            local accepted,reason=coe5.openTestField(plan(invalid,1))
            open=accepted
            assert(accepted==false and reason=='bad_minimum_width')
            assert(coe5.get('unitLastIndex')==last)
        end
        out.refusals={status='PASS',bad_requests=5}
    end,tostring)
    if open then coe5.closeTestField()end
    out.complete={status=passed and'PASS'or'FAIL',detail=tostring(why or'Native setup and closure checked')}
    assert(coe5.writeIni('results',out))
    return result
end,-100)
