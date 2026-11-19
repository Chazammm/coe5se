-- Opt-in, interactive. Close the genuine native card to continue the checks.
local ran = false

coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local open = false
    local ok, why = xpcall(function()
        assert(coe5.testFieldEditorCapabilities().nativeMonsterInspectVersion == 1)
        local devil = assert(coe5.findMonster('Arch Devil'))
        assert(coe5.openTestField{terrain=0, emptyEditor=true, friends={{devil,1}}})
        open = true
        coe5.drawTestField()
        local before = assert(coe5.testFieldState())
        local lifecycle = assert(coe5.testFieldLifecycle())
        local last = coe5.get('unitLastIndex')
        local accepted, reason, refusal = coe5.inspectTestFieldMonster(-1)
        assert(not accepted and reason == 'bad_monster' and refusal.cleanupVerified and not refusal.instanceCreated)
        local shown, detail = coe5.inspectTestFieldMonster(devil)
        assert(shown and detail.monster == devil and detail.source == 'native_unit_card')
        assert(detail.temporaryInstance and detail.uniqueLookupBypasses == 1 and detail.cleanupVerified and not detail.instanceRetained)
        assert(not detail.startingItemsIncluded and not detail.futureInstanceSpellsVerified)
        local after = assert(coe5.testFieldState())
        assert(after.complete and before.complete and #before.units == 1 and #after.units == 1)
        local original, observed = before.units[1], after.units[1]
        assert(observed.unit == original.unit and observed.generation == original.generation and observed.owner == original.owner)
        assert(observed.hp == original.hp and observed.anchorCount == 1 and original.anchorCount == 1)
        assert(observed.anchors[1].x == original.anchors[1].x and observed.anchors[1].y == original.anchors[1].y)
        assert(coe5.get('unitLastIndex') == last and coe5.testFieldLifecycle().createdCount == lifecycle.createdCount)
        assert(coe5.closeTestField())
        open = false
        assert(coe5.testFieldLifecycle().complete)
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    assert(coe5.writeIni('results', {
        complete = {status=ok and 'PASS' or 'FAIL', detail=why or 'Verified real unique preview and owned field cleanup'}
    }))
    return action
end, -100)
