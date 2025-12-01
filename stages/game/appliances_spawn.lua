--Spawn Pan
local pan = ECS.new_entity()
ECS.insert_component(pan, "transform", Transform.new()
:with_position(vec3.new(15.5, 1.9, -5)))
ECS.insert_component(pan, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(pan, "gltf", "frying_pan.glb")
ECS.insert_component(pan, "appliance_type", "pan")
ECS.insert_component(pan, "holding", {food_item = ""})

--Spawn Mixing bowl
local mixing_bowl = ECS.new_entity()
ECS.insert_component(mixing_bowl, "transform", Transform.new()
:with_position(vec3.new(15.5, 1.8, 0)))
ECS.insert_component(mixing_bowl, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(mixing_bowl, "gltf", "mixing_bowl.glb")
ECS.insert_component(mixing_bowl, "appliance_type", "mixing bowl")
ECS.insert_component(mixing_bowl, "holding", {food_item = ""})

--Spawn Pot
local pot = ECS.new_entity()
ECS.insert_component(pot, "transform", Transform.new()
:with_position(vec3.new(15.5, 1.5, 5)))
ECS.insert_component(pot, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(pot, "gltf", "pot.glb")
ECS.insert_component(pot, "appliance_type", "pot")
ECS.insert_component(pot, "holding", {food_item = ""})