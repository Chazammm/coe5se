-- Opt-in stock context and provenance probe for Windows CoE5 5.43.
local ran = false
coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local out, open = {}, false
    local ok, reason = xpcall(function()
        local caps = coe5.testFieldEditorCapabilities()
        assert(caps.structureProvenance == 1)
        local info = assert(coe5.testFieldEnvironmentInfo())
        local terrains = coe5.testFieldTerrains()
        assert(#terrains == 21 and #info.terrains == #terrains)
        local spear = assert(coe5.findMonster('Spearman'))
        for _, id in ipairs(terrains) do
            local accepted, units = coe5.openTestField{
                terrain = id, minimumWidth = 50, emptyEditor = true,
                friends = {{spear, 1}}, enemies = {{spear, 1}}
            }
            assert(accepted, units)
            open = true
            local env = assert(coe5.testFieldEnvironmentState())
            assert(env.terrain == id and env.setupReturned and env.nativeTemplateObserved and not env.customMapReplaced)
            assert(env.width >= 50 and env.plane == 0 and env.temperature == 25)
            local state = assert(coe5.testFieldState())
            assert(state.complete and state.classificationVersion == 1 and env.session == state.session)
            local gate
            for _, u in ipairs(state.units) do
                if u.kind == 'structure' then
                    assert(u.creationSource == 'native_gate_replacement' and u.provenanceVerified and u.anchorCount == 1)
                    gate = u.unit
                end
            end
            if id == 20 then
                assert(env.tower and env.siege and env.nativeTemplate == 'stonetower' and gate, 'Expected native Guard Tower gate')
                assert(coe5.removeTestFieldUnit(gate) == false)
            elseif id == 27 or id == 68 then
                assert(env.atSea and env.boatCountA == 0 and env.boatCountB == 0)
                local u = units.friends[1]
                local hazard = assert(coe5.testFieldWaterState(u, coe5.get('unitBattleX', u), coe5.get('unitBattleY', u)))
                assert(hazard.cellWater and hazard.willDrown and not hazard.ship)
            end
            assert(coe5.closeTestField())
            open = false
            local lifecycle = assert(coe5.testFieldLifecycle())
            assert(lifecycle.complete and not lifecycle.open and #lifecycle.issues == 0)
            out['context' .. id] = {status = 'PASS', template = env.nativeTemplate}
        end
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    out.complete = {status = ok and 'PASS' or 'FAIL', detail = reason or 'Stock context and cleanup checks completed'}
    assert(coe5.writeIni('results', out))
    return action
end, -100)
