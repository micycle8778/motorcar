Log.trace("hello") -- world

local insect = ECS.new_entity()
ECS.insert_component(insect, "sprite", "insect.png")
ECS.insert_component(insect, "transform", 
    Transform.new()
        :with_position(vec3.new(0, 0, 1))
        :with_scale(vec3.new(20))
)

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("k") then
        Stages.change_to("second", "Hello, world!")
    end
end)
