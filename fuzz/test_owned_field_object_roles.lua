-- Opt-in native regression for Windows CoE5 5.43. Run in a disposable profile.
local ran, fighting = false, false
coe5.wrap('BATTLE_WAIT', function(next, ...)
    local action = next(...)
    if fighting and action == 0 and coe5.get('battleDelay') > 2 then return 1 end
    return action
end)
coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local out, open = {}, false
    local ok, why = xpcall(function()
        local spear = assert(coe5.findMonster('Spearman'))
        local accepted, units = coe5.openTestField{
            terrain = 20, minimumWidth = 50, emptyEditor = true, friends = {{spear, 3}}
        }
        assert(accepted, units)
        open = true
        local start = assert(coe5.testFieldState())
        local gate
        for _, row in ipairs(start.units) do
            if row.kind == 'structure' then gate = row; break end
        end
        assert(gate and gate.owner == 24 and gate.creationSource == 'native_gate_replacement' and gate.provenanceVerified)
        local fought, refusal = coe5.fightTestField()
        assert(fought == false and refusal == 'missing_team_b', refusal)
        local placed = assert(coe5.addTestFieldUnits('enemies', {{gate.monster, 1}, {spear, 1}}))
        local rosterGate = assert(coe5.testFieldUnitState(placed[1]))
        assert(rosterGate.kind == 'actor' and rosterGate.owner == gate.owner and rosterGate.monster == gate.monster)
        assert(coe5.moveTestFieldUnit(placed[1], 40, 2))
        assert(coe5.moveTestFieldUnit(placed[2], 40, 15))
        fighting = true
        fought, refusal = coe5.fightTestField()
        fighting = false
        assert(fought ~= false, refusal)
        local final = assert(coe5.testFieldFinalState())
        assert(final.classificationVersion == 1 and final.captured)
        local native, actor
        for _, row in ipairs(final.units) do
            if row.unit == gate.unit and row.generation == gate.generation then native = row end
            if row.unit == rosterGate.unit and row.generation == rosterGate.generation then actor = row end
        end
        assert(native and native.kind == 'structure' and native.creationSource == 'native_gate_replacement')
        assert(actor and actor.kind == 'actor' and actor.creationSource == 'owned_actor_creation')
        assert(final.winner == 0 or final.winner == 24)
        assert(coe5.closeTestField())
        open = false
        assert(coe5.testFieldLifecycle().complete)
        out.structure = {status = 'PASS', same_template = 1, native_gate_not_a_roster_unit = 1, actual_winner = final.winner}
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    out.complete = {status = ok and 'PASS' or 'FAIL', detail = why or 'Final snapshot preserves native creation roles'}
    assert(coe5.writeIni('results', out))
    return action
end, -100)
