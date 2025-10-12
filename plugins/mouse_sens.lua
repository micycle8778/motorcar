local storage = ECS.new_entity()
ECS.insert_component(storage, "mouse_sens", { x = 0.0025 })

ECS.register_system({ "mouse_sens" }, function(e)
    local x = e.mouse_sens.x
    if Input.is_key_pressed_this_frame("scroll down") then
        Log.debug("scroll down")
        e.mouse_sens.x = x * 0.9
    elseif Input.is_key_pressed_this_frame("scroll up") then
        Log.debug("scroll up")
        e.mouse_sens.x = x * 1.1
    end
end, "render")
