-- move player
ECS.register_system({ "global_transform", "transform", "player" },
function(player)
    local speed = 7.5

    local gt = player.global_transform
    if Input.is_key_held_down("a") then
        player.transform:translate_by(gt:left() * Engine.delta() * speed)
    end

    if Input.is_key_held_down("d") then
        player.transform:translate_by(gt:right() * Engine.delta() * speed)
    end

    if Input.is_key_held_down("w") then
        player.transform:translate_by(gt:forward() * Engine.delta() * speed)
    end

    if Input.is_key_held_down("s") then
        player.transform:translate_by(gt:backward() * Engine.delta() * speed)
    end
end)

-- rotate camera along Y
ECS.register_system({
    { "transform", "player" },
    { "mouse_sens" }
}, 
function(player, mouse_sens)
    local x = mouse_sens.mouse_sens.x
    local mouse_motion = Input.get_mouse_motion_this_frame()
    player.transform:rotate_by(vec3.new(0, 1, 0), -mouse_motion.x * x)
end, "render")

-- rotate camera along X
ECS.register_system({
    { "transform", "camera" },
    { "mouse_sens" }
}, 
function(camera, mouse_sens)
    local x = mouse_sens.mouse_sens.x
    local mouse_motion = Input.get_mouse_motion_this_frame()
    camera.transform:rotate_by(vec3.new(1, 0, 0), -mouse_motion.y * x)
end, "render")

-- ray picker
-- ECS.register_system({
--     { "global_transform", "camera" }, { "entity", "player" }, { "money" }, { "water" }
-- }, function(camera, player, money, water)
--     local gt = camera.global_transform
--
--     if Input.is_key_pressed_this_frame("left click") then
--         local result = Physics.cast_ray(gt:position(), gt:forward(), player.entity)
--         local p = gt:position()
--         local d = gt:forward()
--         -- Log.trace(tostring(p.x) .. " " .. tostring(p.y) .. " " .. tostring(p.z))
--         -- Log.trace(tostring(d.x) .. " " .. tostring(d.y) .. " " .. tostring(d.z))
--         if result == nil then return end
--
--         -- if we're holding a weapon, we should handle shooting
--
--         for component, storage in pairs(ECS.get_ecs()) do
--             if storage[result.entity] then
--                 Log.debug(component)
--             end
--         end
--
--         if ECS.get_component(player.entity, "weapon") ~= nil then
--             Log.warn("A")
--             ECS.remove_component_from_entity(player.entity, "weapon")
--             Sound.play_sound("gunshot.mp3")
--             if ECS.get_component(result.entity, "enemy") then
--                 ECS.delete_entity(result.entity)
--                 Sound.play_sound("splat.mp3")
--             end
--         elseif ECS.get_component(result.entity, "mail") ~= nil then
--             Log.warn("B")
--             ECS.delete_entity(result.entity)
--             ECS.insert_component(player.entity, "weapon", {})
--             Sound.play_sound("gun_cock.mp3")
--         elseif ECS.get_component(result.entity, "water_buy_box") ~= nil then
--             Log.warn("C")
--             if money.money.money >= 1 then
--                 money.money.money = money.money.money - 1
--                 water.water.water = water.water.water + 40
--             else
--                 Sound.play_sound("moolah.mp3")
--             end
--         elseif ECS.get_component(result.entity, "mail_buy_box") ~= nil then
--             Log.warn("D")
--             if money.money.money >= 3 then
--                 money.money.money = money.money.money - 3
--                 spawn_mail() spawn_mail() spawn_mail() spawn_mail() spawn_mail()
--             else
--                 Sound.play_sound("moolah.mp3")
--             end
--         else
--             Log.warn("E")
--         end
--
--
--     end
-- end, "render")
