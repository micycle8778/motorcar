-- data-driven cooking lut
--
-- cooking_lut[applicance][state:food] = new_state
-- where
-- appliance = name of the appliance (pan, mixing bowl, pot)
-- state = what's in the appliance (`holding.food_item`)
-- food = what the player is holding
-- new_state is what's put in the appliance
--
-- if new_state == nil, then the cooking failed.
local cooking_lut = {
    pot = {
        [":raw_mashed_potato"] = "mashed_potatoes",

        -- chili logic
        [":raw_mashed_tomato"] = "raw_mashed_tomato",
        [":cooked_sausage"] = "cooked_sausage",
        ["cooked_sausage:raw_mashed_tomato"] = "chili",
        ["raw_mashed_tomato:cooked_sausage"] = "chili",
    },
    ["mixing bowl"] = {
        [":raw_potato"] = "raw_mashed_potato",
        [":raw_tomato"] = "raw_mashed_tomato",
    },
    pan = {
        [":raw_sausage"] = "cooked_sausage",
        [":cooked_sausage"] = "cooked_sausage",
        ["cooked_sausage:buns"] = "hot_dog",
    }
}

local food_meshes = {
    buns = "hotdog_bun.glb",
    raw_potato = "potato.glb",
    raw_tomato = "tomato.glb",
    raw_sausage = "sausage_raw.glb",

    raw_mashed_potato = "potato_squashed.glb",
    raw_mashed_tomato = "tomato_squashed.glb",

    cooked_sausage = "sausage_cooked.glb",
    mashed_potatoes = "mashed_potatoes.glb",
    chili = "chili.glb",
    hot_dog = "hotdog.glb"
}

local food_transforms = {
    buns = Transform.new():with_position(vec3.new(0, -.15, -2)):with_rotation_euler(vec3.new(0, 3.14 / 2, 0)),
    raw_potato = Transform.new():with_position(vec3.new(0, .075, -2)),
    raw_tomato = Transform.new():with_position(vec3.new(0, -.2, -1.5)),
    raw_sausage = Transform.new():with_position(vec3.new(0, -.15, -2)):with_rotation_euler(vec3.new(0, 3.14 / 2, 0)),

    raw_mashed_potato = Transform.new():with_position(vec3.new(0, -.2, -1.5)),
    raw_mashed_tomato = Transform.new():with_position(vec3.new(0, -.2, -1.5)),

    cooked_sausage = Transform.new():with_position(vec3.new(0, -.15, -2)):with_rotation_euler(vec3.new(0, 3.14 / 2, 0)),
    mashed_potatoes = Transform.new():with_position(vec3.new(0, -.2, -1.5)),
    chili = Transform.new():with_position(vec3.new(0, -.2, -1.5)),
    hot_dog = Transform.new():with_position(vec3.new(0, -.15, -2)):with_rotation_euler(vec3.new(0, 3.14 / 2, 0)),
}

function spawn_food_mesh(camera_entity, food)
    local mesh = ECS.new_entity()
    ECS.insert_component(mesh, "parent", camera_entity)
    ECS.insert_component(mesh, "gltf", food_meshes[food])
    ECS.insert_component(mesh, "transform", food_transforms[food])
    ECS.insert_component(mesh, "held_item", food)
end

function pickup_ingredient(player, camera, r)
    local food_type = ECS.get_component(r.entity, "food_type") or ""
    Log.trace(("hit: %d; (%f, %f); food_type = '%s'"):format(r.entity, r.position.x, r.position.y, food_type))

    if food_type ~= "" then
        player.holding.food_item = food_type
        spawn_food_mesh(camera.entity, food_type)
        return true
    end
    return false
end

function pickup_cooked(player, camera, r)
    local appliance = ECS.get_component(r.entity, "appliance_type") or ""
    local holding = ECS.get_component(r.entity, "holding") or { food_item = "" }
    Log.trace(("hit: %d; (%f, %f); appliance_type = '%s'; holding = %s"):format(r.entity, r.position.x, r.position.y, appliance, holding.food_item))

    if appliance ~= "" and holding.food_item ~= "" then
        player.holding.food_item = holding.food_item
        holding.food_item = ""
        spawn_food_mesh(camera.entity, player.holding.food_item)
        return true
    end
    return false
end

ECS.register_system({
    { "player", "holding" },
    { "camera", "global_transform", "entity" }
},
function(player, camera)
    if not Input.is_key_pressed_this_frame("m1") then return end

    local gun_exists = false
    ECS.for_each({ "gun" }, function() gun_exists = true end)
    if gun_exists then return end

    local gt = camera.global_transform
    local result = Physics.cast_ray(gt:position(), gt:forward())
    if result == nil then 
        Log.debug("miss")
        return
    end

    if player.holding.food_item == "" then
        if pickup_ingredient(player, camera, result) then
        elseif pickup_cooked(player, camera, result) then
        else Log.trace("no pickup")
        end
    else -- player is holding item
        local appliance = ECS.get_component(result.entity, "appliance_type") or ""
        local holding = ECS.get_component(result.entity, "holding") or { food_item = "" }
        Log.trace(("hit: %d; (%f, %f); appliance_type = '%s'; holding = %s"):format(result.entity, result.position.x, result.position.y, appliance, holding.food_item))

        if appliance ~= "" then
            local new_state = cooking_lut[appliance][("%s:%s"):format(holding.food_item, player.holding.food_item)]

            if new_state == nil then
                Log.error("TODO: bad sfx/vfx")
            else
                Log.debug("TODO: happy sfx/vfx")
            end

            holding.food_item = new_state or ""

            ECS.for_each({ "held_item", "entity" }, function(e) ECS.delete_entity(e.entity) end)
            player.holding.food_item = ""
        end
    end
end, "render")
