local label = ECS.new_entity()
ECS.insert_component(label, "score", { score = 0 })
ECS.insert_component(label, "text", Text.new("Score: 0"))
ECS.insert_component(label, "transform", Transform.new():with_position(vec3.new(-640, 320, 0)))

ECS.register_system({ "text", "score" }, function(label)
    label.score.score = label.score.score + 1
    label.text.text = ("Score: %d"):format(label.score.score)
end, Event.new("enemy_served"))