local sprite = ECS.new_entity()
ECS.insert_component(sprite, "sprite", "lose.png")
ECS.insert_component(sprite, "transform", Transform.new():with_scale(vec3.new(15)))

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("space") then Stages.change_to("init") end
end, "render")