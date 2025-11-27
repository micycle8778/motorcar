--Spawn Sausages
local sausage = ECS.new_entity()
ECS.insert_component(sausage, "transform", Transform.new()
:with_position(vec3.new(26.5, 1.2 , 10)))
ECS.insert_component(sausage, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(sausage, "gltf", "sausage_raw.glb")
ECS.insert_component(sausage, "food_type", "sausage")


--Spawn Tomatos
local tomato = ECS.new_entity()
ECS.insert_component(tomato, "transform", Transform.new()
:with_position(vec3.new(26.5, 1.2 , 3.5)))
ECS.insert_component(tomato, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(tomato, "gltf", "tomato.glb")
ECS.insert_component(tomato, "food_type", "tomato")

--Spawn Potatos
local potato = ECS.new_entity()
ECS.insert_component(potato, "transform", Transform.new()
:with_position(vec3.new(26.5, 1.4 , -3.5)))
ECS.insert_component(potato, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(potato, "gltf", "potato.glb")
ECS.insert_component(potato, "food_type", "potato")

--Spawn Hot Dog Buns
local buns = ECS.new_entity()
ECS.insert_component(buns, "transform", Transform.new()
:with_position(vec3.new(26.5, 1.2 , -10)))
ECS.insert_component(buns, "body", Body.new(AABB.new(vec3.new(0, 0, 0), vec3.new(1))))
ECS.insert_component(buns, "gltf", "hotdog_bun.glb")
ECS.insert_component(buns, "food_type", "buns")