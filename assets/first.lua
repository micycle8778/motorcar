Log.debug("first.lua started")

local michael = ECS.new_entity()
ECS.insert_component(michael, "name", "Michael")

local joe = ECS.new_entity()
ECS.insert_component(joe, "name", "Joe")

ECS.register_system({ "name" }, function(named)
    Log.trace(named.name)
end, Event.new("rollcall"))

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("r") then
        ECS.fire_event("rollcall")
    end

    if Input.is_key_pressed_this_frame("esc") then
        Engine.quit()
    end
end)

Log.debug("first.lua ended")
