ECS.register_system({ "global_transform", "transform", "camera_holder" }, function(camera_holder)
    local gt = camera_holder.global_transform

    if Input.is_key_held_down("a") then
        camera_holder.transform:translate_by(gt:left() * Engine.delta())
    end

    if Input.is_key_held_down("d") then
        camera_holder.transform:translate_by(gt:right() * Engine.delta())
    end

    if Input.is_key_held_down("w") then
        camera_holder.transform:translate_by(gt:forward() * Engine.delta())
    end

    if Input.is_key_held_down("s") then
        camera_holder.transform:translate_by(gt:backward() * Engine.delta())
    end
end)

ECS.register_system({ "transform", "camera_holder" }, function(camera_holder)
    local mouse_motion = Input.get_mouse_motion_this_frame()
    camera_holder.transform:rotate_by(vec3.new(0, 1, 0), -mouse_motion.x * 0.0025)
end)

ECS.register_system({ "transform", "camera" }, function(camera)
    local mouse_motion = Input.get_mouse_motion_this_frame()
    camera.transform:rotate_by(vec3.new(1, 0, 0), -mouse_motion.y * 0.0025)
end)
