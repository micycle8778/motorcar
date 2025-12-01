local screen = ECS.new_entity()
ECS.insert_component(screen, "sprite", "title_screen.png")
ECS.insert_component(screen, "transform", Transform.new():with_scale(vec3.new(2/3)))

if Stages.props ~= nil then
    local label = ECS.new_entity()
    ECS.insert_component(label, "text", Text.new(("Score: %d"):format(Stages.props.score)))
    ECS.insert_component(label, "transform", Transform.new():with_position(vec3.new(-75, 0, 0)))
end

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("space") then
        Stages.change_to("game")
    end
end, "render")