if Input.is_key_pressed_this_frame("d") then
    Sound.play_sound("doo-doo.mp3")
end

if Input.is_key_pressed_this_frame("s") then
    Sound.play_sound("splat.ogg")
    ECS.for_each({ "transform" })
end

if Input.is_key_pressed_this_frame("f") then
    local e = ECS.new_entity()
    ECS.insert_component(e, "sprite", "insect.png")
    ECS.insert_component(e, "transform", 
        Transform.new()
            :with_position(vec3.new(10, 10, 0))
            :with_scale(vec3.new(10))
    )
end

if Input.is_key_pressed_this_frame("esc") then
    Engine.quit()
end
