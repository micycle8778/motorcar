local gltf = Resources.get_gltf("mail.glb")
function spawn_mail()
    local player = -1
    ECS.for_each({ "player", "entity" }, function(p) player = p.entity end)

    local mail = ECS.new_entity()
    ECS.insert_component(mail, "mail", {})
    ECS.insert_component(mail, "gltf", "mail.glb")
    ECS.insert_component(mail, "body", Body.new(AABB.new(vec3.new(0), vec3.new(1))))
    ECS.insert_component(mail, "trigger_body", {})
    ECS.insert_component(mail, "transform", Transform.new()
        :with_position(vec3.new(
            -2 + Random.randf() * 4,
            -2 + Random.randf() * 4,
            -2 + Random.randf() * 4
        ))
    )
    ECS.insert_component(mail, "parent", player)

    local colors = { "red_mail", "blue_mail", "purple_mail" }
    local vs = {
        red_mail = vec3.new(1, 0, 0),
        blue_mail = vec3.new(0, 0, 1),
        purple_mail = vec3.new(.7, 0, 1)
    }

    local color = colors[1 + Random.randi() % 3]

    ECS.insert_component(mail, color, {})
    ECS.insert_component(mail, "albedo", vs[color])
end

-- move player
ECS.register_system({
    { "global_transform", "transform", "camera_holder" },
    { "transform", "camera_holder_holder" }
}, function(camera_holder, camera_holder_holder)
    local speed = 7.5

    ECS.for_each({ "speed_boost" }, function(e) speed = speed + e.speed_boost.value end)

    local moved = false

    local gt = camera_holder.global_transform
    if Input.is_key_held_down("a") then
        camera_holder_holder.transform:translate_by(gt:left() * Engine.delta() * speed)
        moved = true
    end

    if Input.is_key_held_down("d") then
        camera_holder_holder.transform:translate_by(gt:right() * Engine.delta() * speed)
        moved = true
    end

    if Input.is_key_held_down("w") then
        camera_holder_holder.transform:translate_by(gt:forward() * Engine.delta() * speed)
        moved = true
    end

    if Input.is_key_held_down("s") then
        camera_holder_holder.transform:translate_by(gt:backward() * Engine.delta() * speed)
        moved = true
    end

    if moved then
        local target_angle = camera_holder.transform.rotation:angle()
        local our_angle = camera_holder_holder.transform.rotation:angle()
        local diff_angle = target_angle - our_angle
        
        camera_holder.transform:rotate_by(vec3.new(0, 1, 0), diff_angle * 0.1)
        camera_holder_holder.transform:rotate_by(vec3.new(0, 1, 0), -diff_angle * 0.1)
    end
end)

-- rotate camera along Y
ECS.register_system({
    { "transform", "camera_holder" },
    { "mouse_sens" }
}, 
function(camera_holder, mouse_sens)
    local x = mouse_sens.mouse_sens.x
    local mouse_motion = Input.get_mouse_motion_this_frame()
    camera_holder.transform:rotate_by(vec3.new(0, 1, 0), -mouse_motion.x * x)
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
ECS.register_system({
    { "global_transform", "camera" }, { "entity", "player" }, { "money" }, { "water" }
}, function(camera, player, money, water)
    local gt = camera.global_transform

    if Input.is_key_pressed_this_frame("left click") then
        local result = Physics.cast_ray(gt:position(), gt:forward(), player.entity)
        local p = gt:position()
        local d = gt:forward()
        -- Log.trace(tostring(p.x) .. " " .. tostring(p.y) .. " " .. tostring(p.z))
        -- Log.trace(tostring(d.x) .. " " .. tostring(d.y) .. " " .. tostring(d.z))
        if result == nil then return end

        -- if we're holding a weapon, we should handle shooting

        for component, storage in pairs(ECS.get_ecs()) do
            if storage[result.entity] then
                Log.debug(component)
            end
        end

        if ECS.get_component(player.entity, "weapon") ~= nil then
            Log.warn("A")
            ECS.remove_component_from_entity(player.entity, "weapon")
            Sound.play_sound("gunshot.mp3")
            if ECS.get_component(result.entity, "enemy") then
                ECS.delete_entity(result.entity)
                Sound.play_sound("splat.mp3")
            end
        elseif ECS.get_component(result.entity, "mail") ~= nil then
            Log.warn("B")
            ECS.delete_entity(result.entity)
            ECS.insert_component(player.entity, "weapon", {})
            Sound.play_sound("gun_cock.mp3")
        elseif ECS.get_component(result.entity, "water_buy_box") ~= nil then
            Log.warn("C")
            if money.money.money >= 1 then
                money.money.money = money.money.money - 1
                water.water.water = water.water.water + 40
            else
                Sound.play_sound("moolah.mp3")
            end
        elseif ECS.get_component(result.entity, "mail_buy_box") ~= nil then
            Log.warn("D")
            if money.money.money >= 3 then
                money.money.money = money.money.money - 3
                spawn_mail() spawn_mail() spawn_mail() spawn_mail() spawn_mail()
            else
                Sound.play_sound("moolah.mp3")
            end
        else
            Log.warn("E")
        end


    end
end, "render")

local thirsty = false
ECS.register_system({ "water" }, function(water)
    water.water.water = water.water.water - Engine.delta()
    if water.water.water <= 0 then
        Sound.play_sound("thirsty.mp3")
        Stages.change_to("lose")
    end

    if not thirsty and water.water.water <= 20 then
        thirsty = true
        Sound.play_sound("thirsty.mp3")
    elseif water.water.water > 20 then
        thirsty = false
    end
end)
