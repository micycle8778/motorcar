local foods = { "chili", "mashed_potatoes", "hot_dog" }
local food_to_color = {
    chili = vec3.new(1., 0., 0.),
    mashed_potatoes = vec3.new(0., 1., 0),
    hot_dog = vec3.new(0., 0., 1.)
}

local MAX_ENEMIES = 30
function spawn_enemy() 
    -- enforce MAX_ENEMIES
    local enemy_count = 0
    ECS.for_each({ "enemy" }, function() enemy_count = enemy_count + 1 end)
    if enemy_count >= MAX_ENEMIES then return end

    local enemy = ECS.new_entity()
    --Log.debug(type(foods))
    local food = Random.pick_random(foods)
    ECS.insert_component(enemy, "gltf", "enemy.glb")

    ECS.insert_component(enemy, "body", Body.new(AABB.new(vec3.new(0., 2, 0.), vec3.new(1,2,1))))
    ECS.insert_component(enemy, "trigger_body", {})
    ECS.insert_component(enemy, "albedo", food_to_color[food])
    ECS.insert_component(enemy, "enemy", { wants = food })

    -- spawn enemies between x = -13 to x - 20 and z = 20 to z = -20
    local x_pos = Random.randf_range(-20, -13)
    local z_pos = Random.randf_range(-20, 20)
    ECS.insert_component(enemy, "transform", Transform:new()
        :with_position(vec3.new(x_pos, 0, z_pos))
    )
end

local start_timer = 5.
local timer = start_timer
ECS.register_system({}, function()
    timer = timer - Engine.delta()
    if timer <= 0. then
        spawn_enemy()
        timer = start_timer
    end
end, "render")

spawn_enemy()
spawn_enemy()
