-- enemy walks to the very left (z = -20) and then walks to the very right (z = 20) forever
local speed = 3.0
ECS.register_system({ "global_transform", "transform", "enemy" },
function(enemy)
    local gt = enemy.global_transform
    local pos = gt:position()

    if not enemy.enemy.direction then
        enemy.enemy.direction = vec3.new(0, 0, 1) -- start by going right
        enemy.transform:set_rotation_euler(vec3.new(0, -3.14 / 2, 0)) -- face right
    end

    -- change direction if at bounds
    if pos.z >= 20 then
        enemy.enemy.direction = vec3.new(0, 0, -1) -- go left
        enemy.transform:set_rotation_euler(vec3.new(0, 3.14 / 2, 0)) -- face left
    elseif pos.z <= -20 then
        enemy.enemy.direction = vec3.new(0, 0, 1) -- go right
        enemy.transform:set_rotation_euler(vec3.new(0, -3.14 / 2, 0)) -- face right
    end

    enemy.transform:translate_by(
        enemy.enemy.direction * 
        Engine.delta() * 
        speed
    )
end)

-- when enemy collides with slop trigger, delete enemy
ECS.register_system({ "enemy", "colliding_with", "entity" }, function(enemy)
    for idx, e in pairs(enemy.colliding_with.entities) do
        if ECS.get_component(e, "slop") ~= nil then
            ECS.delete_entity(enemy.entity)
        end
    end
end)