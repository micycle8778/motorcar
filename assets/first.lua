local t = { name = "player" }
ECS.insert_component(0, "name", t)
ECS.insert_component(1, "name", { name = "enemy" })

ECS.for_each({ "entity", "name", "position", "velocity" }, function(t)
    Log.trace(tostring(t.entity) .. " " .. t.name.name .. " " .. tostring(t.position) .. " " .. tostring(t.velocity))
end)
