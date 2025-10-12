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
ECS.register_system({ "global_transform", "camera" }, function(camera)
    local gt = camera.global_transform

    if Input.is_key_pressed_this_frame("left click") then
        local result = Physics.cast_ray(gt:position(), gt:forward())
        if result ~= nil then
            ECS.insert_component(result.entity, "albedo", vec3.new(1, 0, 0))
        end
    end
end, "render")
