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

-- ECS.register_system({{ "blue_house", "colliding_with" }, { "money" }}, function() Log.trace("hello") end)

function handle_house_overlaps(color)
    ECS.register_system({"entity", color .. "_house" }, function(house, money)
        local colliding_with = 0
        ECS.for_each({color .. "_house", "colliding_with"}, function()
            colliding_with = colliding_with + 1
        end)
        -- Log.warn("#colliding_with = " .. tostring(colliding_with))
    end)
    ECS.register_system({{ color .. "_house", "colliding_with" }, { "money" }}, function(house, money)
        for idx, e in ipairs(house.colliding_with.entities) do
            if ECS.get_component(e, color .. "_mail") then
                ECS.delete_entity(e)
                Sound.play_sound("chaching.mp3")
                money.money.money = money.money.money + 1
            end
        end
    end, "render")
end

handle_house_overlaps("red")
handle_house_overlaps("blue")
handle_house_overlaps("purple")
