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
    buns = Transform.new():with_position(vec3.new(0, -.15, 0)):rotated_y(3.14 / 2):rotated_z(.5),
    raw_potato = Transform.new():with_position(vec3.new(0, .075, 0)):rotated_z(.5),
    raw_tomato = Transform.new():with_position(vec3.new(0, -.2, 0)):rotated_z(.5),
    raw_sausage = Transform.new():with_position(vec3.new(0, -.15, 0)):rotated_y(3.14 / 2):rotated_z(.5),

    raw_mashed_potato = Transform.new():with_position(vec3.new(0, -.2, 0)):rotated_z(.5),
    raw_mashed_tomato = Transform.new():with_position(vec3.new(0, -.2, 0)):rotated_z(.5),

    cooked_sausage = Transform.new():with_position(vec3.new(0, -.15, 0)):rotated_y(3.14 / 2):rotated_z(.5),
    mashed_potatoes = Transform.new():with_position(vec3.new(0, -.2, 0)):rotated_z(.5),
    chili = Transform.new():with_position(vec3.new(0, -.2, 0)):rotated_z(.5),
    hot_dog = Transform.new():with_position(vec3.new(0, -.15, 0)):rotated_y(3.14 / 2):rotated_z(.5),
}

function spawn_food_mesh(camera_entity, food)
    local parent = ECS.new_entity()
    ECS.insert_component(parent, "parent", camera_entity)
    ECS.insert_component(parent, "held_item", food)
    ECS.insert_component(parent, "transform", Transform.new():with_position(vec3.new(0, 0, -2)))
    
    local mesh = ECS.new_entity()
    ECS.insert_component(mesh, "parent", parent)
    ECS.insert_component(mesh, "gltf", food_meshes[food])
    ECS.insert_component(mesh, "transform", food_transforms[food])
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
    local distance = result.position:distance_to(gt:position())
    if distance > 3.5 then
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

            local app_pos = ECS.get_component(result.entity, "global_transform"):position()
            if new_state == nil then
                -- dark smoke
                spawn_particles(
                    20, .65, app_pos, -- count + pos
                    vec3.new(.0), vec3.new(.25), -- color
                    .5, .5, -- lifetime
                    2., 6, -- speed
                    .125, .25 -- scale
                )

                -- fire
                spawn_particles(
                    10, .65, app_pos, -- count + pos
                    vec3.new(.65, 0, 0), vec3.new(.8, .55, .1), -- color
                    .6, .4, -- lifetime
                    2., 6, -- speed
                    .25, .35 -- scale
                )
            else
                -- light smoke
                spawn_particles(
                    30, .65, app_pos, -- count + pos
                    vec3.new(.35), vec3.new(1.), -- color
                    .5, .5, -- lifetime
                    2., 6, -- speed
                    .125, .25 -- scale
                )
            end

            holding.food_item = new_state or ""

            ECS.for_each({ "held_item", "entity" }, function(e) ECS.delete_entity(e.entity) end)
            player.holding.food_item = ""
        end
    end
end, "render")


-- particle system: spawn short-lived colored spheres that rise then shrink and are deleted
ECS.register_component("particle")

-- spawn_particles(..., scale_min, scale_max)
-- scale_min/scale_max: optional range for initial particle scale; if omitted, preserves previous random defaults
function spawn_particles(count, radius, center, color1, color2, hold_time, shrink_time, rise_speed_min, rise_speed_max, scale_min, scale_max)
    for i=1,count do
        local e = ECS.new_entity()

        -- random point in disc (uniform)
        local theta = math.random() * 2 * math.pi
        local r = radius * math.sqrt(math.random())
        local x = center.x + r * math.cos(theta)
        local z = center.z + r * math.sin(theta)
        local y = center.y

        -- initial scale and rise speed per-particle
    -- determine initial scale (allow caller to provide a range)
    local smin = scale_min or 0.05
    local smax = scale_max or (smin + 0.08)
    local init_scale = smin + math.random() * (smax - smin)
        local rise_speed = rise_speed_min + math.random() * (rise_speed_max - rise_speed_min)

        -- pick a color along gradient between color1 and color2
        local t = math.random()
        local col = vec3.new(
            color1.x * (1 - t) + color2.x * t,
            color1.y * (1 - t) + color2.y * t,
            color1.z * (1 - t) + color2.z * t
        )

        ECS.insert_component(e, "transform", Transform.new():with_position(vec3.new(x, y, z)):with_scale(vec3.new(init_scale)))
        ECS.insert_component(e, "gltf", "sphere.glb")
        ECS.insert_component(e, "albedo", col)
        ECS.insert_component(e, "particle", { timer = 0., hold_time = hold_time or 0.3, shrink_time = shrink_time or 0.3, rise_speed = rise_speed, init_scale = init_scale })
    end
end

ECS.register_system({ "particle", "transform", "entity" },
function(p)
    local dt = Engine.delta()
    local part = p.particle

    part.timer = part.timer + dt

    if part.timer <= part.hold_time then
        -- rising phase
        p.transform:translate_by(vec3.new(0, dt * part.rise_speed, 0))
    else
        -- shrinking phase
        local elapsed = part.timer - part.hold_time
        local denom = math.max(1e-6, part.shrink_time)
        local t = math.min(1, elapsed / denom)
        local s = part.init_scale * (1 - t)

        -- continue moving up a little while shrinking
        p.transform:translate_by(vec3.new(0, dt * part.rise_speed, 0))

        -- update scale (replace transform with scaled copy)
        ECS.insert_component(p.entity, "transform", p.transform:with_scale(vec3.new(s)))

        if t >= 1 then
            ECS.delete_entity(p.entity)
        end
    end
end, "render")

ECS.register_component("held_item")
ECS.register_system({ "held_item", "transform", "global_transform" }, function(item)
    item.transform:rotate_by(vec3.new(0, 1, 0), Engine.delta())
end)
