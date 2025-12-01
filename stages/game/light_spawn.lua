function spawn_light(position, color) 
    local light = ECS.new_entity()
    ECS.insert_component(light, "light", Light.new(color, 100.))
    ECS.insert_component(light, "transform", Transform.new():with_position(position))
end

local power = .6
local spread = 12
spawn_light(vec3.new(13, 4, spread), vec3.new(.8, .8, .15) * power)
spawn_light(vec3.new(13, 4, -spread), vec3.new(.8, .8, .15) * power)

