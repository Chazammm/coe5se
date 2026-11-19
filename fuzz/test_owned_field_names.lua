-- Opt-in Windows 5.43 native probe in a disposable profile.
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
    local out, opened = {}, false
    local ok, why = xpcall(function()
        local caps = coe5.testFieldEditorCapabilities()
        assert(caps.individualNameVersion == 1 and caps.individualNameMaxBytes == 47)
        local spear = assert(coe5.findMonster('Spearman'))
        local goblin = assert(coe5.findMonster('Goblin'))
        local accepted, units = coe5.openTestField{
            terrain = 0, minimumWidth = 50, commander = spear, emptyEditor = false,
            friends = {{spear, 2}}, enemies = {{goblin, 3}}
        }
        assert(accepted, units)
        opened = true
        local a, b = units.friends[1], units.friends[2]
        local commander = assert(units.commander)
        local native = assert(coe5.testFieldUnitName(commander))
        assert(native.storage == 'native_commander_record')
        assert(coe5.setTestFieldUnitName(commander, 'Captain Ash'))
        assert(coe5.testFieldUnitName(commander).shown:find('Captain Ash', 1, true))
        assert(coe5.setTestFieldUnitName(a, 'North Spear'))
        assert(coe5.setTestFieldUnitName(b, 'South Spear'))
        assert(coe5.testFieldUnitName(a).shown == 'North Spear')
        assert(coe5.testFieldUnitName(b).shown == 'South Spear')
        assert(coe5.get('monsterName', spear) == 'Spearman')
        for _, bad in ipairs({string.rep('n', 48), ' ', 'a,b', 'a%b', 'a' .. string.char(0), string.char(0xff)}) do
            local renamed, reason, detail = coe5.setTestFieldUnitName(a, bad)
            assert(not renamed and reason and detail.rollbackVerified)
            assert(coe5.testFieldUnitName(a).shown == 'North Spear')
        end
        assert(coe5.setTestFieldUnitName(a, 'Aurora Ω'))
        assert(coe5.testFieldUnitName(a).shown == 'Aurora Ω')
        assert(coe5.setTestFieldUnitName(a, ''))
        assert(coe5.testFieldUnitName(a).shown == 'Spearman')
        assert(coe5.setTestFieldUnitName(a, 'North Spear'))
        assert(coe5.moveTestFieldUnit(a, 20, 8))
        assert(coe5.testFieldUnitName(a).shown == 'North Spear')
        assert(coe5.removeTestFieldUnit(b))
        local retired, reason, detail = coe5.setTestFieldUnitName(b, 'Retired Spear')
        assert(not retired and reason and detail.rollbackVerified)
        local replacement = assert(coe5.addTestFieldUnits('friends', {{spear, 1}}))[1]
        assert(not coe5.testFieldUnitName(replacement).custom)
        assert(coe5.moveTestFieldUnit(replacement, 21, 8))
        out.names = {status = 'PASS', actors = 2, commander = 1, rejects = 6}
        fighting = true
        local fought, refusal = coe5.fightTestField()
        assert(fought ~= false, refusal)
        fighting = false
        local observed = assert(coe5.testFieldUnitName(a))
        assert(coe5.get('battleRound') > 0 and observed.combatReads > 0)
        assert(coe5.closeTestField())
        opened = false
        assert(coe5.testFieldLifecycle().complete)
        out.battle = {status = 'PASS', round = coe5.get('battleRound'), native_name_reads = observed.combatReads}
    end, tostring)
    if opened then pcall(coe5.closeTestField) end
    out.complete = {status = ok and 'PASS' or 'FAIL', detail = why or 'Owned native naming and cleanup verified'}
    assert(coe5.writeIni('results', out))
    return action
end, -100)
