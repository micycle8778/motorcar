Log.trace("")

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("k") then
        Stages.change_to("second", "Hello, world!")
    end
end)
