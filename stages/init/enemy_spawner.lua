local foods = { "chili", "mashed_potatoes", "hot_dog" }
local food_to_color = {
    chili = vec3.new(1., 0., 0.),
    mashed_potatoes = vec3.new(0., 1., 0),
    hot_dog = vec3.new(0., 0., 1.)
}

local enemy = ECS.new_entity()
Log.debug(type(foods))
local food = Random.pick_random(foods)
ECS.insert_component(enemy, "gltf", "enemy.glb")
ECS.insert_component(enemy, "transform", Transform:new())
ECS.insert_component(enemy, "body", Body.new(AABB.new(vec3.new(0., 2, 0.), vec3.new(1,2,1))))
ECS.insert_component(enemy, "trigger_body", {})
ECS.insert_component(enemy, "albedo", food_to_color[food])
ECS.insert_component(enemy, "enemy", { wants = food })