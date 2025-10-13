local aabb = Resources.get_gltf("fly.glb").mesh_bundles[1].aabb

function spawn_enemy()
    Log.debug("spawn_enemy")
    local enemy = ECS.new_entity()
    ECS.insert_component(enemy, "enemy", {})
    ECS.insert_component(enemy, "gltf", "fly.glb")
    ECS.insert_component(enemy, "body", Body.new(aabb))
    ECS.insert_component(enemy, "trigger_body", {})

    local transform = Transform.new()

    local spawn_distance = 75
    if Random.randi() % 2 == 1 then
        transform = transform:with_position(vec3.new(Random.randf() * 50, 2, spawn_distance))
    else
        transform = transform:with_position(vec3.new(Random.randf() * 50, 2, -spawn_distance))
    end

    ECS.insert_component(enemy, "transform", transform:with_scale(vec3.new(5)))

    if (Random.randi() % 10) == 1 then
        Sound.play_sound("buzzbuzzbuzz.mp3")
    end
end

local timer = 0
ECS.register_system({}, function()
    timer = timer - Engine.delta()
    if timer <= 0 then
        timer = 5
        spawn_enemy()
    end
end)

ECS.register_system({
    { "enemy", "transform" },
    { "player", "transform" }
}, function(enemy, player)
    local to_player = enemy.transform.position - player.transform.position
    local dir = to_player:normalized()

    enemy.transform:translate_by(dir * -5 * Engine.delta())
    enemy.transform:look_at(player.transform.position)
end, "render")

ECS.register_system({ "entity", "enemy", "colliding_with" }, function(enemy)
    for idx, e in pairs(enemy.colliding_with.entities) do
        if ECS.get_component(e, "player") ~= nil then
            ECS.for_each({ "health" }, function(h) 
                h.health.health = h.health.health - 1
                if h.health.health <= 0 then
                    Stages.change_to("lose")
                end
            end)
            local sounds = { "ough.mp3", "owie.mp3", "yeouchie.mp3" }
            Sound.play_sound(sounds[Random.randi_range(1, 4)])
            ECS.delete_entity(enemy.entity)
        end
    end
end)