-- Opt-in current-build native regression. Main-menu test fields only.
local api=coe5;local request=assert(api.readIni('request')).environment
local out={environment=request};local started,opened,fighting,fatal=false,false,false,false
local function save()assert(api.writeIni('results',out))end
local function same(a,b)
    if type(a)~=type(b)then return false end;if type(a)~='table'then return a==b end
    for k,v in pairs(a)do if not same(v,b[k])then return false end end
    for k in pairs(b)do if a[k]==nil then return false end end;return true
end
local function state()local s,why=api.testFieldState();assert(s,why);assert(s.complete and #s.issues==0,'Native state is incomplete or inconsistent');return s end
local function row(id)for _,u in ipairs(state().units)do if u.unit==id then return u end end;error('Owned actor absent')end
local function open(spec)local ok,units=api.openTestField(spec);assert(ok,tostring(units));opened=true;return units end
local function close()
    if not opened then return end;local ok,why=api.closeTestField();assert(ok,tostring(why));opened=false
end
local function test(name,fn)
    if fatal then out[name]={status='NOT_RUN',detail='Earlier cleanup was unverified; restart required'};save();return end
    local ok,why=xpcall(fn,tostring);local clean,reason=pcall(close)
    if not clean then fatal=true;ok=false;why='Cleanup unverified: '..tostring(reason)end
    out[name]={status=ok and 'PASS' or 'FAIL',detail=ok and 'Actual native calls and readback' or why};save()
end
local function lists(id)
    local s=assert(api.testFieldSpellState(id));return {known=s.known,memorized=s.memorized}
end
local function clearAnchor(w,h,ignore)
    local width=api.get('battleWidth')
    for x=1,width-w-1 do for y=1,19-h do
        local free=true
        for dx=0,w-1 do for dy=0,h-1 do
            local unit=api.battleUnitAt(x+dx,y+dy);local object=api.battleObjectAt(x+dx,y+dy)
            if unit and unit>=0 and unit~=ignore or object and object>=0 then free=false end
        end end
        if free then return x,y end
    end end;error('No genuinely empty target footprint')
end
api.wrap('BATTLE_WAIT',function(next,...)
    local id=next(...);if fighting and id==0 and (api.get('battleDelay')or 0)>2 then return 1 end;return id
end)
api.wrap('MAIN_MENU_WAIT',function(next,...)
    local id=next(...);if started then return id end;started=true
    test('capabilities_and_invalid_inputs_without_unrelated_modules',function()
        local c=assert(api.testFieldEditorCapabilities());assert(c.emptyEditor==1 and c.state==1 and c.lifecycle==1 and c.multipleUnique==1)
        assert(c.configuration==0 and c.cursedRemoval==0 and c.restoreHigherKnown==0)
        assert(api.setTestFieldConfiguration==nil and api.testFieldFinalState==nil and api.battleView==nil and api.loadedContentOrigin==nil)
        local s,why=api.testFieldState();assert(s==nil and why=='not_open')
        local ok;ok,why=api.removeTestFieldUnit(2147483647);assert(not ok)
    end)
    test('empty_sides_delete_last_and_reuse',function()
        local units=open{emptyEditor=true,friends={},enemies={}};assert(#state().units==0 and units.commander==-1)
        local added=assert(api.addTestFieldUnits('enemies',{{953,1}}));local b=added[1];assert(row(b).hp>0)
        local before=state();local accepted,why=api.fightTestField();assert(not accepted and why=='missing_team_a');assert(same(before,state()))
        assert(api.removeTestFieldUnit(b));assert(api.get('unitHp',b)<=0)
        local living=0;for _,u in ipairs(state().units)do if u.hp>0 then living=living+1 end end;assert(living==0)
        added=assert(api.addTestFieldUnits('friends',{{13,1}}));local a=added[1];assert(row(a).hp>0)
        accepted,why=api.fightTestField();assert(not accepted and why=='missing_team_b')
        assert(api.removeTestFieldUnit(a));state()
        out.empty={both_empty_verified=1,added_either_side=1,battle_refusal_unchanged=1}
    end)
    test('large_footprint_rejected_move_cleanup_and_immediate_reuse',function()
        local large=assert(api.findMonster('Arcane Frost Dragon'));local units=open{emptyEditor=true,allowMultipleUnique=true,friends={{large,1}},enemies={{953,1}}}
        local a,b=units.friends[1],units.enemies[1];local before=row(a);assert(before.width>1 and before.height>1)
        local known=lists(b);local bx,by=api.get('unitBattleX',b),api.get('unitBattleY',b)
        local x,y=clearAnchor(before.width,before.height,a);assert(api.moveTestFieldUnit(a,x,y));local placed=row(a)
        assert(#placed.occupied==placed.width*placed.height)
        local accepted=api.moveTestFieldUnit(a,bx,by);assert(not accepted);assert(same(placed,row(a)) and same(known,lists(b)))
        local added=assert(api.addTestFieldUnits('friends',{{large,1}}));local failed=added[1]
        accepted=api.moveTestFieldUnit(failed,bx,by);assert(not accepted);assert(api.removeTestFieldUnit(failed));state()
        assert(api.removeTestFieldUnit(a));assert(api.get('unitHp',a)<=0)
        for _,cell in ipairs(placed.occupied)do assert(api.battleUnitAt(cell.x,cell.y)~=a)end
        added=assert(api.addTestFieldUnits('friends',{{13,1}}));assert(api.moveTestFieldUnit(added[1],x,y));state();assert(same(known,lists(b)))
        out.large={monster=large,width=placed.width,height=placed.height,occupied=#placed.occupied,reused_x=x,reused_y=y,other_actor_spells_unchanged=1}
    end)
    test('unique_preflight_and_independent_native_records',function()
        open{emptyEditor=true,friends={{201,1}},enemies={{953,1}}};local before=state()
        local accepted=api.addTestFieldUnits('friends',{{201,1}});assert(not accepted);assert(same(before,state()));close()
        local u=open{emptyEditor=true,allowMultipleUnique=true,friends={{201,2}},enemies={{201,1}}}
        local a,b,c=u.friends[1],u.friends[2],u.enemies[1];assert(a~=b and a~=c and b~=c)
        local ra,rb,rc=row(a),row(b),row(c);assert(ra.generation~=rb.generation and rb.generation~=rc.generation)
        local old=api.get('unitHp',a);local other=api.get('unitHp',b);api.set('unitHp',a,old-1);assert(api.get('unitHp',b)==other);api.set('unitHp',a,old)
        assert(api.removeTestFieldUnit(a));assert(api.get('unitHp',b)>0 and api.get('unitHp',c)>0);state()
        out.unique={model='three independently tracked native actors',monster=201,a=a,b=b,c=c}
    end)
    test('spell_identity_validation_and_other_actor_preservation',function()
        local u=open{emptyEditor=true,friends={{594,1}},enemies={{594,1}}};local a,b=u.friends[1],u.enemies[1]
        local before,other=lists(a),lists(b);assert(#before.known>0);assert(api.setTestFieldSpells(a,before));assert(same(before,lists(a)))
        local bad={known={before.known[1],before.known[1]},memorized={}};local ok,why,detail=api.setTestFieldSpells(a,bad)
        assert(not ok and why=='duplicate_spell' and detail.stage=='validate' and same(before,lists(a)))
        ok,why=api.setTestFieldSpells(a,{known={},memorized={before.known[1]}});assert(not ok and why=='memorized_not_known')
        assert(same(before,lists(a)) and same(other,lists(b)));state()
    end)
    test('legacy_commander_guard_and_repeated_real_battles',function()
        local u=open{commander=13,enemies={{953,1}}};local ok,why=api.removeTestFieldUnit(u.commander);assert(not ok and why=='primary_commander');close()
        for i=1,2 do
            u=open{commander=13,enemies={{953,1}}};fighting=true;local accepted,reason=api.fightTestField();fighting=false;assert(accepted~=false,tostring(reason))
            local winner,round=api.get('battleWinner'),api.get('battleRound');assert(type(winner)=='number' and type(round)=='number' and round>0)
            out['legacy_fight_'..i]={winner=winner,round=round,completed=1};close()
        end
        local c=api.testFieldLifecycle();assert(c and c.open==false and c.complete==true);assert(api.unitNoLand(0)==nil)
    end)
    test('reported_neutral_land_terrain_initialization_and_battles',function()
        local allowed={Plain=true,Forest=true,Mountains=true,Swamp=true,Desert=true};local seen={};local count=0
        for _,terrain in ipairs(assert(api.testFieldTerrains()))do
            local name=api.get('terrainName',terrain);assert(allowed[name] and not seen[name]);seen[name]=true;count=count+1
            open{emptyEditor=true,terrain=terrain,friends={{13,1}},enemies={{953,1}}};state()
            assert(api.get('squareTerrain',1,1)==terrain,'Terrain initialization differs from requested loaded identity')
            fighting=true;local ok,why=api.fightTestField();fighting=false;assert(ok~=false,tostring(why))
            local round=api.get('battleRound');assert(type(round)=='number' and round>0)
            out['terrain_'..terrain]={name=name,round=round,winner=api.get('battleWinner'),completed=1};close()
            local lifecycle=assert(api.testFieldLifecycle());assert(lifecycle.open==false and lifecycle.complete==true)
        end
        assert(count==5 and seen.Forest and seen.Plain,'Required verified land subset missing')
    end)
    local pass=not fatal;for _,r in pairs(out)do if type(r)=='table' and (r.status=='FAIL' or r.status=='NOT_RUN')then pass=false end end
    out.complete={status=pass and 'PASS' or 'FAIL',scope='Current standalone field core; presentation/configuration are absent'};save();return id
end,-100)
