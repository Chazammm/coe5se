-- Opt-in Windows native probe. Run in a disposable profile from the main menu.
local ran, exercise, failure = false, nil, nil

coe5.wrap('BATTLE_WAIT', function(next, ...)
    local action = next(...)
    if exercise then
        local step = exercise
        exercise = nil
        local ok, why = xpcall(step, tostring)
        if not ok then failure = why end
    end
    return action
end)

coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local open, roundtrips = false, 0
    local ok, why = xpcall(function()
        assert(coe5.testFieldEditorCapabilities().battleViewport == 1)
        local spear = assert(coe5.findMonster('Spearman'))
        assert(coe5.openTestField{terrain=0, emptyEditor=true, minimumWidth=70, friends={{spear,1}}, enemies={{spear,1}}})
        open = true
        coe5.drawTestField()
        local original = assert(coe5.testFieldState())
        local screenW, screenH = coe5.get('screenW'), coe5.get('screenH')
        local function wait(step)
            exercise = step
            coe5.drawTestField()
            assert(not exercise and not failure, failure or 'Owned WAIT was not observed')
        end
        local function checkCorner(x, y)
            local viewport = assert(coe5.testFieldViewport())
            local sx, sy = coe5.battleCellToScreen(x, y)
            assert(sx >= viewport.x and sx < viewport.x + viewport.width)
            assert(sy >= viewport.y and sy < viewport.y + viewport.height)
            local cx, cy = coe5.battleScreenToCell(sx, sy)
            assert(cx == x and cy == y)
            roundtrips = roundtrips + 1
        end
        wait(function()
            assert(type(coe5.testFieldPointerWheel()) == 'number')
            assert(coe5.setTestFieldViewport{x=20,y=40,width=screenW-320,height=screenH-80})
            assert(coe5.fitTestFieldViewport())
            assert(coe5.testFieldViewport().fits)
            checkCorner(0, 0)
            checkCorner(69, 19)
        end)
        for _, zoom in ipairs({0.1, 1, 2}) do
            for _, corner in ipairs({{0,0,1000000,1000000}, {69,19,-1000000,-1000000}}) do
                wait(function()
                    assert(coe5.setTestFieldZoom(zoom))
                    assert(coe5.panTestFieldViewport(corner[3], corner[4]))
                    checkCorner(corner[1], corner[2])
                end)
            end
        end
        local after = assert(coe5.testFieldState())
        assert(original.width == after.width and #original.grid == #after.grid and #original.units == #after.units)
        for i, cell in ipairs(after.grid) do
            assert(cell.unit == original.grid[i].unit and cell.object == original.grid[i].object)
        end
        for i, unit in ipairs(after.units) do
            assert(unit.unit == original.units[i].unit and unit.generation == original.units[i].generation and unit.owner == original.units[i].owner)
        end
        local accepted, reason = coe5.setTestFieldZoom(0.5)
        assert(not accepted and reason == 'not_battle_frame')
        assert(coe5.closeTestField())
        open = false
        assert(coe5.testFieldLifecycle().complete)
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    assert(coe5.writeIni('results', {
        complete = {status=ok and 'PASS' or 'FAIL', detail=why or 'Verified Fit, pan, native corners and cleanup', roundtrips=roundtrips}
    }))
    return action
end, -100)
