Log.trace("hello") -- world

local insect = ECS.new_entity()
ECS.insert_component(insect, "sprite", "insect.png")
ECS.insert_component(insect, "transform", 
    Transform.new()
        :with_position(vec3.new(0, 0, 1))
        :with_scale(vec3.new(20))
)

local horse = ECS.new_entity()
ECS.insert_component(horse, "gltf", "horse.glb")
ECS.insert_component(horse, "transform",
    Transform.new()
        :with_position(vec3.new(2, 0, 1))
)

local carrot = ECS.new_entity()
ECS.insert_component(carrot, "gltf", "carrot.glb")
ECS.insert_component(carrot, "transform",
    Transform.new()
        :with_position(vec3.new(0, 0, 1))
)

local horse2 = ECS.new_entity()
ECS.insert_component(horse2, "gltf", "horse.glb")
ECS.insert_component(horse2, "transform",
    Transform.new()
        :with_position(vec3.new(-2, 0, 1))
        :with_scale(vec3.new(1.5))
)
ECS.insert_component(horse2, "albedo", vec3.new(.5, 0, 1))

Log.trace("horse2 is " .. tostring(horse2))

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("k") then
        Stages.change_to("second", "Hello, world!")
    end
end)
