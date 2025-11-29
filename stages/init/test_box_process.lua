ECS.register_component("test_box")
ECS.register_system({"test_box", "global_transform", "transform", "entity"},
function(test_box)
    local test_speed = 5
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

--Interact with appliance
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
            --Case: player is depositing something
            if(player.holding.food_item ~= "") then
                
                --Case: Pot
                if(appliance.appliance_type == "pot") then
                    --Case: cooked sausage
                    if(player.holding.food_item == "cooked_sausage") then
                        --Case: pot is holding cooked mashed tomato already
                        if(appliance.holding.food_item == "cooked_mashed_tomato") then
                            Log.debug("Pot was holding: " .. appliance.holding.food_item)
                            appliance.holding.food_item = "chili"
                            player.holding.food_item = ""
                            Log.debug("Pot is now holding: " .. appliance.holding.food_item)
                            Log.debug("Player is now holding: " .. player.holding.food_item)
                            return
                        end
                        --Case: pot is not holding anything
                        if(appliance.holding.food_item == "") then
                            appliance.holding.food_item = "cooked_sausage"
                            player.holding.food_item = ""
                            Log.debug("Pot is now holding: " .. appliance.holding.food_item)
                            Log.debug("Player is now holding: " .. player.holding.food_item)
                            return
                        end
                    end
                    --Case: mashed tomato
                    if(player.holding.food_item == "raw_mashed_tomato") then
                        --Case: pot is holding cooked sausage already
                        if(appliance.holding.food_item == "cooked_sausage") then
                            Log.debug("Pot was holding: " .. appliance.holding.food_item)
                            appliance.holding.food_item = "chili"
                            player.holding.food_item = ""
                            Log.debug("Pot is now holding: " .. appliance.holding.food_item)
                            Log.debug("Player is now holding: " .. player.holding.food_item)
                            return
                        end
                        --Case: pot is not holding anything
                        appliance.holding.food_item = "cooked_mashed_tomato"
                        player.holding.food_item = ""
                        Log.debug("Pan is now holding: " .. appliance.holding.food_item)
                        Log.debug("Player is now holding: " .. player.holding.food_item)
                        return
                    end
                    --Case: mashed potato
                    if(player.holding.food_item == "raw_mashed_potato") then
                        appliance.holding.food_item = "mashed_potatoes"
                        player.holding.food_item = ""
                        Log.debug("Pot is now holding: " .. appliance.holding.food_item)
                        Log.debug("Player is now holding: " .. player.holding.food_item)
                        return
                    end
                end

                --Case: Mixing Bowl
                if(appliance.appliance_type == "mixing bowl") then
                    --Case: raw potato
                    if(player.holding.food_item == "raw_potato") then
                        appliance.holding.food_item = "raw_mashed_potato"
                        player.holding.food_item = ""
                        Log.debug("Pan is now holding: " .. appliance.holding.food_item)
                        Log.debug("Player is now holding: " .. player.holding.food_item)
                        return
                    end

                    --Case: raw tomato
                    if(player.holding.food_item == "raw_tomato") then
                        appliance.holding.food_item = "raw_mashed_tomato"
                        player.holding.food_item = ""
                        Log.debug("Pan is now holding: " .. appliance.holding.food_item)
                        Log.debug("Player is now holding: " .. player.holding.food_item)
                        return
                    end
                end

                --Case: Pan
                if(appliance.appliance_type == "pan") then

                    --Case: player is putting raw sausage in pan
                    if(player.holding.food_item == "raw_sausage") then
                        appliance.holding.food_item = "cooked_sausage"
                        player.holding.food_item = ""
                        Log.debug("Pan is now holding: " .. appliance.holding.food_item)
                        Log.debug("Player is now holding: " .. player.holding.food_item)
                        return
                    end
                    --Case: player is holding bun trying to pick up cooked sausage
                    if(player.holding.food_item == "buns") then
                        if(appliance.holding.food_item == "cooked_sausage") then
                            appliance.holding.food_item = ""
                            player.holding.food_item = "hot_dog"
                            Log.debug("Pan is now holding: " .. appliance.holding.food_item)
                            Log.debug("Player is now holding: " .. player.holding.food_item)
                        end
                    end
                end
            end
            --Case: playing is picking something up from appliance
            --Case: Pot
            if(appliance.appliance_type == "pot") then
                --Case: pot is holding chili
                if(appliance.holding.food_item == "chili") then
                    player.holding.food_item = appliance.holding.food_item
                    appliance.holding.food_item = ""
                    Log.debug("Player is holding: " .. player.holding.food_item)
                    Log.debug("Appliance is holding: " .. appliance.holding.food_item)
                    return
                end
                --Case: pot is holding mashed potatos
                if(appliance.holding.food_item == "mashed_potatoes") then
                    player.holding.food_item = appliance.holding.food_item
                    appliance.holding.food_item = ""
                    Log.debug("Player is holding: " .. player.holding.food_item)
                    Log.debug("Appliance is holding: " .. appliance.holding.food_item)
                    return
                end
            end
            --Case: Mixing Bowl
            if(appliance.appliance_type == "mixing bowl") then
                --Case: mixing bowl is holding raw_mashed_tomatos
                if(appliance.holding.food_item == "raw_mashed_tomato") then
                    player.holding.food_item = appliance.holding.food_item
                    appliance.holding.food_item = ""
                    Log.debug("Player is holding: " .. player.holding.food_item)
                    Log.debug("Appliance is holding: " .. appliance.holding.food_item)
                    return
                end
                --Case: mixing bowl is holding raw_mashed_potatos
                if(appliance.holding.food_item == "raw_mashed_potato") then
                    player.holding.food_item = appliance.holding.food_item
                    appliance.holding.food_item = ""
                    Log.debug("Player is holding: " .. player.holding.food_item)
                    Log.debug("Appliance is holding: " .. appliance.holding.food_item)
                    return
                end
            end
            --Case: Pan
            if(appliance.appliance_type == "pan") then
                --Case: pan is holding cooked_sausage
                if(appliance.holding.food_item == "cooked_sausage") then
                    player.holding.food_item = appliance.holding.food_item
                    appliance.holding.food_item = ""
                    Log.debug("Player is holding: " .. player.holding.food_item)
                    Log.debug("Appliance is holding: " .. appliance.holding.food_item)
                    return
                end
            end
        end
    end
end)

--Delete the test box after 1/2 a second
local test_time = 0
ECS.register_system({"test_box", "entity", "timer"},
function(test_box)
    test_time = test_box.timer.time
    test_time = test_time - Engine.delta()
    if(test_time <= 0) then
        ECS.delete_entity(test_box.entity)
    end
    test_box.timer.time = test_time
end, "render")