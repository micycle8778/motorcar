ECS.register_system({
    { "buy_box", "entity" },
    { "camera", "global_transform" },
    { "player", "entity" }
}, function(buy_box, camera, player)
    local gt = camera.global_transform
    local result = Physics.cast_ray(gt:position(), gt:forward(), player.entity)
    
    if result == nil or result.entity ~= buy_box.entity then
        ECS.insert_component(buy_box.entity, "albedo", vec3.new(1.))
    else
        ECS.insert_component(buy_box.entity, "albedo", vec3.new(1., .8, .8))
    end
end, "render")

ECS.register_system({{ "blue_house", "colliding_with" }, { "money" }}, function() Log.trace("hello") end)

function handle_house_overlaps(color)
ECS.register_system({{ color .. "_house", "colliding_with" }, { "money" }}, function(house, money)
    Log.warn("hello")
    for idx, e in ipairs(house.colliding_with) do
        if ECS.get_component(color .. "_mail") then
            ECS.delete_entity(e)
            Sound.play_sound("chaching.mp3")
            money.money.money = money.money.money + 1
        end
    end
end)
end

handle_house_overlaps("red")
handle_house_overlaps("blue")
handle_house_overlaps("purple")