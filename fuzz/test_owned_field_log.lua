-- Opt-in native overlay probe for a disposable Windows 5.43 profile.
local ran, fighting, hiddenAt, restored = false, false, nil, false
coe5.wrap('BATTLE_WAIT', function(next, ...)
    local action = next(...)
    if not fighting then return action end
    local state = assert(coe5.testFieldLogState())
    if not hiddenAt and state.lineCountValid and state.lineCount > 0 then
        assert(state.phase == 'fight' and state.visible)
        assert(coe5.setTestFieldLogVisible(false))
        assert(not coe5.testFieldLogState().visible)
        hiddenAt = state.lineCount
    elseif hiddenAt and not restored and state.lineCount > hiddenAt then
        assert(not state.visible, 'Native log display changed while hidden')
        assert(coe5.setTestFieldLogVisible(true))
        assert(coe5.testFieldLogState().visible)
        restored = true
    end
    if restored and action == 0 and coe5.get('battleDelay') > 2 then return 1 end
    return action
end)
coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local value = next(...)
    if ran then return value end
    ran = true
    local out, open = {}, false
    local ok, why = xpcall(function()
        local spear = assert(coe5.findMonster('Spearman'))
        local accepted, detail = coe5.openTestField{
            terrain = 0, emptyEditor = true, friends = {{spear, 2}}, enemies = {{spear, 2}}
        }
        assert(accepted, detail)
        open = true
        local before = assert(coe5.testFieldLogState())
        local width, delay = coe5.get('battleWidth'), coe5.get('battleDelay')
        assert(before.phase == 'prepare' and before.overlay and not before.reservesMapSpace)
        local invalid, reason = coe5.setTestFieldLogVisible(0)
        assert(not invalid and reason == 'bad_type')
        assert(coe5.setTestFieldLogVisible(false))
        assert(not coe5.testFieldLogState().visible)
        assert(coe5.get('battleWidth') == width and coe5.get('battleDelay') == delay)
        assert(coe5.setTestFieldLogVisible(true))
        fighting = true
        local fought, refusal = coe5.fightTestField()
        fighting = false
        assert(fought ~= false, refusal)
        assert(restored and coe5.get('battleRound') > 0, 'Hidden battle did not generate further messages')
        local after = assert(coe5.testFieldLogState())
        assert(after.phase == 'after' and after.visible and after.lineCount > hiddenAt)
        assert(coe5.setTestFieldLogVisible(not before.visible))
        assert(coe5.closeTestField())
        open = false
        assert(coe5.testFieldLifecycle().complete)
        assert(coe5.openTestField{terrain = 0, emptyEditor = true})
        open = true
        assert(coe5.testFieldLogState().visible == before.visible)
        assert(coe5.closeTestField())
        open = false
        out.overlay = {status = 'PASS', lines_when_hidden = hiddenAt, final_lines = after.lineCount, restored_before_close = 1}
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    out.complete = {status = ok and 'PASS' or 'FAIL', detail = why or 'Native overlay, message production and close verified'}
    assert(coe5.writeIni('results', out))
    return value
end, -100)
