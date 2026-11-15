-- Explicit native presentation acceptance; no simulated engine or UI clicks.
local api=coe5;local request=assert(api.readIni('request')).environment
local out={environment=request};local started,opened,fighting,active,fatal=false,false,false,false,false
local frame,frameFailure,upkeepEvents,drawUnit,drawMonster,drawItem
upkeepEvents=0
local function save()assert(api.writeIni('results',out))end
local function same(a,b)
    if type(a)~=type(b)then return false end;if type(a)~='table'then return a==b end
    for k,v in pairs(a)do if not same(v,b[k])then return false end end
    for k in pairs(b)do if a[k]==nil then return false end end;return true
end
local function close()
    active=false;if not opened then return end;assert(api.setTestFieldTextFocus(false));local ok,why=api.closeTestField();assert(ok,tostring(why));opened=false
end
local function open(spec)local ok,units=api.openTestField(spec);assert(ok,tostring(units));opened=true;return units end
local function test(name,fn)
    if fatal then out[name]={status='NOT_RUN',detail='Unverified cleanup; restart required'};save();return end
    local ok,why=xpcall(fn,tostring);local cleaned,reason=pcall(close)
    if not cleaned then fatal=true;ok=false;why='Cleanup unverified: '..tostring(reason)end
    out[name]={status=ok and 'PASS' or 'FAIL',detail=ok and 'Actual native API readbacks' or why};save()
end
local function itemNamed(name)for id=0,api.get('itemLastIndex')do if api.get('itemName',id)==name then return id end end;error('Installed item missing: '..name)end
api.observe('BATTLE_AFTER_FIGHT_UPKEEP',function()if fighting then upkeepEvents=upkeepEvents+1 end end)
api.wrap('BATTLE_WAIT',function(next,...)
    local id=next(...);if fighting and id==0 and (api.get('battleDelay')or 0)>2 then return 1 end;return id
end)
api.wrap('BATTLE_DRAW',function(next,...)
    local result=next(...)
    if not active or frame or frameFailure then return result end
    local ok,why=xpcall(function()
        local view=assert(api.battleView());assert(view.cellSize>0 and view.width>0 and view.height>0)
        local checks=0
        for x=1,view.width-2 do for y=1,view.height-2 do
            local sx,sy=api.battleCellToScreen(x,y)
            if sx and sx>1 and sx<view.screenW-1 and sy>1 and sy<view.screenH-1 then
                local xx,yy=api.battleScreenToCell(sx,sy);assert(xx==x and yy==y);checks=checks+1
            end
        end end;assert(checks>0,'No visible cell roundtrip')
        local ax,ay=api.get('unitBattleX',drawUnit),api.get('unitBattleY',drawUnit)
        local footprint=assert(api.testFieldFootprint(drawMonster,drawUnit));assert(footprint.width==3 and footprint.height==3)
        local preview=assert(api.testFieldPlacementPreflight(drawMonster,ax,ay,drawUnit,true));assert(preview.partial==true and #preview.cells==9)
        local copy=assert(api.testFieldPlacementPreflight(drawMonster,ax,ay,drawUnit,false));assert(copy.valid==false)
        local edge=assert(api.testFieldPlacementPreflight(drawMonster,-1,-1));assert(edge.partial and not edge.valid and #edge.cells==9)
        local outside=0;for _,cell in ipairs(edge.cells)do if cell.status=='out_of_bounds'then outside=outside+1 end end;assert(outside>0)
        local buttons=api.testFieldPointerButtons();assert(type(buttons)=='number' and buttons>=0 and buttons<=7)
        local drawn,monster=api.ui.drawMonsterIconFit(16,view.screenH-100,64,64,drawMonster);assert(drawn,tostring(monster))
        assert(monster.width>0 and monster.height>0 and monster.width<=64 and monster.height<=64)
        local itemOk,item=api.ui.drawItemIconFit(88,view.screenH-100,64,64,drawItem);assert(itemOk,tostring(item))
        assert(item.width>0 and item.height>0 and item.width<=64 and item.height<=64)
        api.ui.drawText(16,view.screenH-20,12,'Owned field API projection and native sprite QA',1,1,1,1)
        frame={visible_roundtrips=checks,width=view.width,height=view.height,cell_size=view.cellSize,footprint_cells=#preview.cells,
            partial_geometry=1,edge_outside_cells=outside,monster_source_w=monster.sourceWidth,monster_source_h=monster.sourceHeight,
            item_source_w=item.sourceWidth,item_source_h=item.sourceHeight,pointer_buttons=buttons}
    end,tostring)
    if not ok then frameFailure=why end
    return result
end)
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local id=next(...);if started then return id end;started=true
    test('scoped_surface_and_invalid_contexts',function()
        local c=assert(api.testFieldEditorCapabilities());assert(c.configuration==0 and c.cursedRemoval==0)
        assert(api.setTestFieldConfiguration==nil and api.loadedContentOrigin==nil)
        assert(api.battleView()==nil);assert(api.testFieldFinalState()==nil)
        local ok,why=api.setTestFieldTextFocus(true);assert(not ok and why=='not_open')
        local drawn=api.ui.drawMonsterIconFit(0,0,-1,20,13);assert(not drawn)
        local item=api.ui.drawItemIconFit(0,0,20,20,-1);assert(not item)
    end)
    test('current_native_projection_footprint_pointer_and_mod_sprites',function()
        drawMonster=assert(api.findMonster('Arcane Frost Dragon'));drawItem=itemNamed('Abyss Fragment')
        local units=open{emptyEditor=true,friends={{drawMonster,1}},enemies={{953,1}}};drawUnit=units.friends[1]
        local before=assert(api.testFieldSpellState(drawUnit));local outside=api.testFieldPlacementPreflight(drawMonster,1,1);assert(outside==nil,'Preview accepted outside a frame')
        active=true;for i=1,3 do api.drawTestField();if frame or frameFailure then break end end;active=false
        assert(frame,tostring(frameFailure or 'No native draw observation'));assert(same(before,api.testFieldSpellState(drawUnit)))
        out.projection=frame
    end)
    test('text_focus_roundtrip_without_fabricating_typed_events',function()
        open{emptyEditor=true,friends={},enemies={}}
        assert(api.setTestFieldTextFocus(true));local events=assert(api.takeTestFieldTextEvents());assert(events.focused==true and type(events.events)=='table')
        assert(api.setTestFieldTextFocus(false));local after=assert(api.takeTestFieldTextEvents());assert(after.focused==false)
        out.text={focus_enter_exit_verified=1,typed_input='Not injected in this API test; visible keyboard test is separate'}
    end)
    test('timed_final_capture_readonly_and_upstream_observer_chain',function()
        for cycle=1,2 do
            local units=open{emptyEditor=true,friends={{13,1}},enemies={{953,1}}}
            local observed=upkeepEvents;fighting=true;local ok,why=api.fightTestField();fighting=false;assert(ok~=false,tostring(why))
            assert(upkeepEvents==observed+1,'Existing upkeep observer chain was bypassed or duplicated')
            local final=assert(api.testFieldFinalState());assert(final.phase=='captured' and final.captured and final.complete and final.sceneComplete==false)
            assert(final.source=='post_aftermath_before_upkeep' and #final.grid==final.width*final.height and #final.units>=2)
            local added,reason=api.addTestFieldUnits('friends',{{13,1}});assert(added==nil and reason=='final_read_only')
            ok,reason=api.removeTestFieldUnit(units.friends[1]);assert(not ok and reason=='final_read_only')
            ok,reason=api.moveTestFieldUnit(units.friends[1],8,8);assert(not ok and reason=='final_read_only')
            assert(same(final,api.testFieldFinalState()),'Read-only refusals altered the retained snapshot')
            out['final_'..cycle]={round=api.get('battleRound'),winner=api.get('battleWinner'),units=#final.units,cells=#final.grid,
                complete=1,scene_complete=0,upstream_observer_calls=upkeepEvents-observed}
            close();assert(api.testFieldFinalState()==nil)
        end
    end)
    local passed=not fatal;for _,r in pairs(out)do if type(r)=='table' and (r.status=='FAIL' or r.status=='NOT_RUN')then passed=false end end
    out.complete={status=passed and 'PASS' or 'FAIL',scope='Native presentation branch; modal Inspect and typed input need visible testing'};save();return id
end,-100)
