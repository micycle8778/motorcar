ECS.register_component("gun")

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

--Gun spawn logic
ECS.register_system({{"entity", "camera"}, {"player", "holding"}},
function(player_camera, player)
    if Input.is_key_pressed_this_frame("r") then

        local e -- entity | nil
        ECS.for_each({"entity", "gun"}, function(x) e = x.entity end)

        if e == nil then
            Log.debug("player was holding: " .. player.holding.food_item)
            local gun = ECS.new_entity()
            ECS.insert_component(gun, "gltf", "launcher.glb")
            ECS.insert_component(gun, "transform", Transform.new()
            :with_position(vec3.new(1., -.5, 0.))
            :rotated(vec3.new(0, 1, 0), 3.14))
            ECS.insert_component(gun, "parent", player_camera.entity)
            ECS.insert_component(gun, "gun", {})
            ECS.insert_component(gun, "food_type", player.holding.food_item)

            ECS.for_each({ "held_item", "entity" }, function(e) ECS.delete_entity(e.entity) end)
            player.holding.food_item = ""
        else
            ECS.delete_entity(e)
        end
        Log.debug("player is holding: " .. player.holding.food_item)
    end
end, "render")

function cast_ray(origin, direction)
    function cast_ray(origin, direction)
        -- try physics first
        local hit = Physics.cast_ray(origin, direction)
        if hit then
            local pos = hit.position
            return pos
        end

        -- fallback: intersect ray with horizontal plane y = 0
        local dy = direction.y
        if math.abs(dy) < 1e-6 then
            -- ray parallel to plane
            if math.abs(origin.y) < 1e-6 then
                return origin -- already on the plane
            end
        end

        local t = -origin.y / dy
        if t < 0 then return nil end -- intersection is behind origin
        return origin + direction * t
    end
end

--Gun fire logic
ECS.register_system({
    {"gun", "global_transform", "food_type"},
    { "camera", "global_transform" }
},
function(gun, cam)
    if(Input.is_key_pressed_this_frame("m1")) then
        local gt = gun.global_transform
        local cam_gt = cam.global_transform

        local result = cast_ray(cam_gt:position(), cam_gt:forward())
        if result == nil then return end

        local food = ECS.new_entity()
        --Case: Chili
        if(gun.food_type == "chili") then
            ECS.insert_component(food, "gltf", "chili.glb")
        elseif(gun.food_type == "hot_dog") then --Case: Hot dog
            ECS.insert_component(food, "gltf", "hotdog.glb")
        elseif(gun.food_type == "mashed_potatoes") then --Case: Mashed potatos
            ECS.insert_component(food, "gltf", "mashed_potatoes.glb")
        else --Case: trying to fire an improperly loaded gun
            return
        end
        ECS.insert_component(food, "transform", Transform.new()
        :with_position(gt:position() + (2 * gt:backward())))
        ECS.insert_component(food, "food_type", gun.food_type)
        ECS.insert_component(food, "slop", {
            direction = (result - gt:position()):normalized()
        })
        ECS.insert_component(food, "body", Body.new(AABB.new(vec3.new(0., 0., 0.), vec3.new(0.5,0.5,0.5))))
        ECS.insert_component(food, "trigger_body", {})
        ECS.insert_component(food, "lifetime", {time = 2.0})
    end
end, "render")


--Food grabbing logic
-- ECS.register_system({"camera", "global_transform"}, 
-- function(cam)
--     if(Input.is_key_pressed_this_frame("e")) then
--         local cam_gt = cam.global_transform
--         local dir = cast_ray(cam_gt:position(), cam_gt:forward())
--         if dir == nil then return end
--         
--         local test_box = ECS.new_entity()
--         ECS.insert_component(test_box, "test_box", {direction = (dir - cam_gt:position()):normalized()})
--         ECS.insert_component(test_box, "transform", Transform.new()
--         :with_position(cam_gt:position()))
--         ECS.insert_component(test_box, "timer", {time = 0.5})
--         ECS.insert_component(test_box, "body", Body.new(AABB.new(vec3.new(0., 0., 0.), vec3.new(0.25,0.25,0.25))))
--         ECS.insert_component(test_box, "trigger_body", {})
--     end
-- end, "render")

--Food dropping logic
ECS.register_system({"player", "holding"}, 
function(player)
    if(Input.is_key_pressed_this_frame("space")) then
        ECS.for_each({ "held_item", "entity" }, function(e) ECS.delete_entity(e.entity) end)
        player.holding.food_item = ""
        Log.debug("Player is now holding: " .. player.holding.food_item)
    end 
end, "render")
