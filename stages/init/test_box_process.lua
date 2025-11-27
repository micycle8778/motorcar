ECS.register_component("test_box")
ECS.register_system({"test_box", "global_transform", "transform", "entity"},
function(test_box)
    local test_speed = 1
    local gt = test_box.global_transform
    test_box.transform:translate_by(test_box.test_box.direction * Engine.delta() * test_speed)
end)

--Pick up food from food shelf
ECS.register_system({
    {"food_type" , "colliding_with" , "entity"},
    {"test_box", "entity"},
    {"player", "holding"}
},
function(food, test_box, player)
    for idx, e in pairs(food.colliding_with.entities) do
        if ECS.get_component(e, "test_box") ~= nil then
            Log.debug(food.food_type)
            ECS.delete_entity(test_box.entity)
            player.holding.food_item = food.food_type
            Log.debug("The player is now holding:" .. player.holding.food_item)
        end
    end
end)

--Put food in appliance
ECS.register_system({
    {"appliance_type" , "colliding_with" , "entity", "holding"},
    {"test_box", "entity"},
    {"player", "holding"}
},
function(appliance, test_box, player)
    for idx, e in pairs(appliance.colliding_with.entities) do
        if ECS.get_component(e, "test_box") ~= nil then
            --Log.debug(appliance.appliance_type)
            --Log.debug(player.holding.food_item)
            ECS.delete_entity(test_box.entity)
            if(player.holding.food_item ~= "") then
                Log.debug("Player is now holding: " .. player.holding.food_item)
                appliance.holding.food_item = player.holding.food_item
                player.holding.food_item = ""
                Log.debug("Appliance is now holding: " .. appliance.holding.food_item)
                Log.debug("Player is now holding: " .. player.holding.food_item)
            end
        end
    end
end)