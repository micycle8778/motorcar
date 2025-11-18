local horse = ECS.new_entity()
ECS.insert_component(horse, "transform", Transform.new():with_scale(vec3.new(3)))
ECS.insert_component(horse, "sprite3d", "michael-day.png")
