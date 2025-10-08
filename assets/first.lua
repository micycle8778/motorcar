Log.debug("first.lua started")

local michael = ECS.new_entity()
ECS.insert_component(michael, "name", "Michael")

local joe = ECS.new_entity()
ECS.insert_component(joe, "name", "Joe")

ECS.register_system({ "sprite", "entity" }, function(t)
    Log.trace(tostring(t.entity) .. " " .. t.sprite.resource_path)
end, "render")

ECS.register_system({
    { "sprite", "entity" },
    { "name" }
}, function(sprite, named)
    Log.trace(named.name .. " : " .. tostring(sprite.entity) .. " " .. sprite.sprite.resource_path)
end, "physics")

Log.debug("first.lua ended")
