local label = ECS.new_entity()
ECS.insert_component(label, "clock", { clock = 180 }) -- three minutes
ECS.insert_component(label, "text", Text.new("3:00"))
ECS.insert_component(label, "transform", Transform.new():with_position(vec3.new(-640, -320, 0)))

ECS.register_system({ "text", "clock" }, function(label)
    label.clock.clock = label.clock.clock - Engine.delta()
    local c = label.clock.clock
    label.text.text = ("%d:%02d"):format(math.floor(c / 60), math.floor(c % 60))

    if c <= 0 then
        local score = 0
        ECS.for_each({ "score" }, function(s) score = s.score.score end)
        Stages.change_to("init", { score = score })
    end
end, "render")