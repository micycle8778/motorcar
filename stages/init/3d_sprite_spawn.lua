function spawn_light(position, color) 
    local horse = ECS.new_entity()
    ECS.insert_component(horse, "transform", 
        Transform.new()
            :with_scale(vec3.new(3))
            :with_position(position)
    )
    ECS.insert_component(horse, "sprite3d", "michael-day.png")

    local light = ECS.new_entity()
    ECS.insert_component(light, "light", Light.new(color, 30.))
    ECS.insert_component(light, "parent", horse)
    ECS.insert_component(light, "transform", Transform.new():with_position(vec3.new(0., 0., -0.1)))
end

local spread = 12
spawn_light(vec3.new(13, 0, spread), vec3.new(.6, .6, .0)) -- yellow
spawn_light(vec3.new(13, 0, -spread), vec3.new(.6, .0, .6)) -- pink

