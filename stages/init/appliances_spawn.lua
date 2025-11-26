--Spawn Pan
local pan = ECS.new_entity()
ECS.insert_component(pan, "transform", Transform.new()
:with_position(vec3.new(15.5, 2.5, -5)))
ECS.insert_component(pan, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(pan, "gltf", "cube.glb")
ECS.insert_component(pan, "albedo", vec3.new(1., 0., 0.))
ECS.insert_component(pan, "appliance_type", "pan")
ECS.insert_component(pan, "holding", "")

--Spawn Mixing bowl
local mixing_bowl = ECS.new_entity()
ECS.insert_component(mixing_bowl, "transform", Transform.new()
:with_position(vec3.new(15.5, 2.5, 0)))
ECS.insert_component(mixing_bowl, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(mixing_bowl, "gltf", "cube.glb")
ECS.insert_component(mixing_bowl, "albedo", vec3.new(0., 1., 0.))
ECS.insert_component(mixing_bowl, "appliance_type", "mixing bowl")
ECS.insert_component(mixing_bowl, "holding", "")

--Spawn Pot
local pot = ECS.new_entity()
ECS.insert_component(pot, "transform", Transform.new()
:with_position(vec3.new(15.5, 2.5, 5)))
ECS.insert_component(pot, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(pot, "gltf", "cube.glb")
ECS.insert_component(pot, "albedo", vec3.new(0., 0., 1.))
ECS.insert_component(pot, "appliance_type", "pot")
ECS.insert_component(pot, "holding", "")