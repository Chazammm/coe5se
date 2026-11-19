-- Opt-in exact deployment and initial-overlap cases for Windows CoE5 5.43.
local ran = false

local function equal(a, b)
    if type(a) ~= type(b) then return false end
    if type(a) ~= 'table' then return a == b end
    for k, v in pairs(a) do if not equal(v, b[k]) then return false end end
    for k in pairs(b) do if a[k] == nil then return false end end
    return true
end

coe5.wrap('MAIN_MENU_WAIT', function(next, ...)
    local action = next(...)
    if ran then return action end
    ran = true
    local out, open = {}, false
    local ok, why = xpcall(function()
        local caps = coe5.testFieldEditorCapabilities()
        assert(caps.exactDeploymentVersion == 1 and caps.initialDeploymentVersion == 1)
        local spear = assert(coe5.findMonster('Spearman'))
        local hunter = assert(coe5.findMonster('Hunter of Gods'))
        local wall = assert(coe5.findMonster('Bronze Wall'))
        local function start(friends, enemies, terrain)
            local accepted, units = coe5.openTestField{
                emptyEditor = true, terrain = terrain, minimumWidth = 70,
                allowMultipleUnique = true, friends = friends, enemies = enemies
            }
            assert(accepted, units)
            open = true
            return assert(coe5.testFieldState()), assert(coe5.testFieldInitialDeployment())
        end
        local function close()
            assert(coe5.closeTestField())
            open = false
            assert(coe5.testFieldLifecycle().complete)
        end
        local function target(receipt, xA, xB)
            local rows = {}
            local offset = {[0] = 0, [24] = 0}
            for _, actor in ipairs(receipt.actors) do
                local x = (actor.owner == 0 and xA or xB) + offset[actor.owner]
                rows[#rows + 1] = {unit = actor.unit, generation = actor.generation, x = x, y = 10}
                offset[actor.owner] = offset[actor.owner] + 3
            end
            return rows
        end
        local function check(rows)
            local state = assert(coe5.testFieldState())
            assert(state.complete and #state.issues == 0)
            for _, row in ipairs(rows) do
                assert(coe5.battleUnitAt(row.x, row.y) == row.unit)
                assert(coe5.testFieldActorState(row.unit).generation == row.generation)
            end
        end
        local before, receipt = start({{spear, 1}}, {{spear, 1}}, 0)
        assert(before.complete and receipt.fresh and receipt.sourceHealthy and not receipt.eligible)
        local rows = target(receipt, 8, 20)
        local accepted, reason = coe5.setTestFieldDeployment(rows, {initialReceipt = receipt.token})
        assert(not accepted and reason == 'initial_receipt_unavailable')
        assert(equal(before, coe5.testFieldState()))
        local moved, refusal, detail = coe5.setTestFieldDeployment(rows)
        assert(moved and detail.phase == 'committed' and detail.rollbackVerified, refusal)
        check(rows)
        close()
        out.ordinary = {status = 'PASS', actors = #rows}

        before, receipt = start({{hunter, 1}, {wall, 1}}, {{hunter, 1}, {wall, 1}}, 20)
        assert(receipt.fresh and receipt.eligible and receipt.actorOverlapOnly and #receipt.overlaps > 0)
        rows = target(receipt, 4, 60)
        local invalid = target(receipt, 4, 60)
        invalid[2].x, invalid[2].y = invalid[1].x, invalid[1].y
        local applied, refused, failure = coe5.setTestFieldDeployment(invalid, {initialReceipt = receipt.token})
        assert(not applied and refused and failure.initialReceiptUsed and failure.byteRollbackVerified and not failure.rollbackVerified)
        local after = assert(coe5.testFieldState())
        assert(not after.complete and equal(before.grid, after.grid))
        close()
        before, receipt = start({{hunter, 1}, {wall, 1}}, {{hunter, 1}, {wall, 1}}, 20)
        rows = target(receipt, 4, 60)
        moved, refusal, detail = coe5.setTestFieldDeployment(rows, {initialReceipt = receipt.token})
        assert(moved and detail.initialReceiptUsed and detail.finalHealthy and detail.initialSession == receipt.session, refusal)
        assert(not coe5.testFieldInitialDeployment().fresh)
        check(rows)
        close()
        out.overlap = {status = 'PASS', actors = #rows, overlaps = #receipt.overlaps, invalid_source_quarantined = 1}
    end, tostring)
    if open then pcall(coe5.closeTestField) end
    out.complete = {status = ok and 'PASS' or 'FAIL', detail = why or 'Exact ordinary and initial deployment checks completed'}
    assert(coe5.writeIni('results', out))
    return action
end, -100)
