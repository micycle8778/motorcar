Log.trace("")

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("esc") then
        Engine.quit()
    end
end)
