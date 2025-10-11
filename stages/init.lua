Log.trace("hello") -- world

local cube = ECS.new_entity()
ECS.insert_component(cube, "gltf", "cube.glb")
ECS.insert_component(cube, "transform",
    Transform.new()
        :with_position(vec3.new(0, 0, 1))
        :with_scale(vec3.new(.5))
)
ECS.insert_component(cube, "body", Body.new(AABB.new()))
ECS.insert_component(cube, "cube1", {})

local cube2 = ECS.new_entity()
ECS.insert_component(cube2, "gltf", "cube.glb")
ECS.insert_component(cube2, "transform",
    Transform.new()
        :with_position(vec3.new(2, 0, 1))
        :with_scale(vec3.new(.5))
)
ECS.insert_component(cube2, "body", Body.new(AABB.new()))
ECS.insert_component(cube2, "cube2", {})

ECS.register_system({}, function()
    if Input.is_key_pressed_this_frame("k") then
        Stages.change_to("second", "Hello, world!")
    end
end, "render")
