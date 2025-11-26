--Spawn Sausages
local sausage = ECS.new_entity()
ECS.insert_component(sausage, "transform", Transform.new()
:with_position(vec3.new(28, 1.5 , 10)))
ECS.insert_component(sausage, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(sausage, "gltf", "carrot.glb")
ECS.insert_component(sausage, "food_type", "sausage")


--Spawn Tomatos
local tomato = ECS.new_entity()
ECS.insert_component(tomato, "transform", Transform.new()
:with_position(vec3.new(28, 1.5 , 3.5)))
ECS.insert_component(tomato, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(tomato, "gltf", "carrot.glb")
ECS.insert_component(tomato, "albedo", vec3.new(0. , 0. , 1.))
ECS.insert_component(tomato, "food_type", "tomato")

--Spawn Potatos
local potato = ECS.new_entity()
ECS.insert_component(potato, "transform", Transform.new()
:with_position(vec3.new(28, 1.5 , -3.5)))
ECS.insert_component(potato, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(potato, "gltf", "carrot.glb")
ECS.insert_component(potato, "albedo", vec3.new(0. , 1. , 0.))
ECS.insert_component(potato, "food_type", "potato")

--Spawn Hot Dog Buns
local buns = ECS.new_entity()
ECS.insert_component(buns, "transform", Transform.new()
:with_position(vec3.new(28, 1.5 , -10)))
ECS.insert_component(buns, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(buns, "gltf", "carrot.glb")
ECS.insert_component(buns, "albedo", vec3.new(1. , 0. , 0.))
ECS.insert_component(buns, "food_type", "buns")