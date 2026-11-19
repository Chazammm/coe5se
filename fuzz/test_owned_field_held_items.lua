-- Opt-in owned-field regression for a disposable Windows 5.43 profile.
local ran = false

coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local open = false
    local ok, why = xpcall(function()
        assert(coe5.testFieldEditorCapabilities().clearHeldItems == 1)
        local spear = assert(coe5.findMonster('Spearman'))
        local dragon = assert(coe5.findMonster('Arcane Frost Dragon'))
        assert(coe5.openTestField{
            terrain = 0, emptyEditor = true, friends = {{dragon, 1}}, enemies = {{spear, 1}}
        })
        open = true
        local units = assert(coe5.testFieldState()).units
        assert(#units == 2)
        local first, second = units[1], units[2]
        if first.monster ~= dragon then first, second = second, first end
        assert(first.monster == dragon and second.monster == spear)
        local unit, other = first.unit, second.unit
        assert(coe5.giveItem(unit, 'Crystal Heart'))
        assert(coe5.giveItem(other, 'Amulet of Anti Magic'))
        local before = assert(coe5.testFieldLoadoutState(unit))
        local untouched = assert(coe5.testFieldLoadoutState(other))
        local empty, schools, held, cursed = {}, {}, 0, 0
        for i, slot in ipairs(before.items) do
            empty[i] = -1
            if slot.id >= 0 then held = held + 1 end
            if slot.cursed then cursed = cursed + 1 end
        end
        assert(held >= 1 and cursed >= 1, ('Expected a cursed item, held=%d cursed=%d'):format(held, cursed))
        for i, school in ipairs(before.schools) do schools[i] = {path = school.path, level = school.base} end
        local state = {
            items = empty, schools = schools, known = before.known,
            memorized = before.memorized, restoreKnown = true, clearHeldItems = true
        }
        local cleared, reason, detail = coe5.setTestFieldConfiguration(unit, state)
        assert(not cleared and reason == 'clear_items_only' and detail.rollbackVerified)
        for i, slot in ipairs(assert(coe5.testFieldLoadoutState(unit)).items) do
            assert(slot.id == before.items[i].id)
        end
        state.restoreKnown = false
        cleared, reason, detail = coe5.setTestFieldConfiguration(unit, state)
        assert(cleared and detail.clearHeldItems, reason)
        local after = assert(coe5.testFieldLoadoutState(unit))
        assert(after.generation == before.generation and after.organInjuryFlags == before.organInjuryFlags)
        for _, slot in ipairs(after.items) do assert(slot.id == -1) end
        for i, slot in ipairs(assert(coe5.testFieldLoadoutState(other)).items) do
            assert(slot.id == untouched.items[i].id)
        end
        assert(coe5.closeTestField())
        open = false
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    assert(coe5.writeIni('results', {
        complete = {status = ok and 'PASS' or 'FAIL', detail = why or 'Verified cursed clear and isolated refusal'}
    }))
    return action
end, -100)
