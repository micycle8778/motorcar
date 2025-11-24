ECS.register_system({}, function()

    if Input.is_key_pressed_this_frame("escape") then
        Engine.quit()
    end
    
end, "render")