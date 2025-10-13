local floor = ECS.new_entity()
ECS.insert_component(floor, "transform", Transform.new())
ECS.insert_component(floor, "gltf", "floor.glb")

-- blue_house.glb (-23.764505386353, 0.0, 23.158887863159) 2.3999998569489
-- blue_house.glb (-1.6854492425919, 0.0, 30.826688766479) 1.5999997854233
-- purple_house.glb (-33.573631286621, 0.0, -42.363201141357) 4.2000002861023
-- red_house.glb (20.240159988403, 0.0, 18.475639343262) 0.80000042915344

local level_data = {
    { "blue_house", "blue_house.glb", vec3.new(-23.764505386353, 0.0, 23.158887863159), 2.3999998569489 },
    { "blue_house", "blue_house.glb", vec3.new(-1.6854492425919, 0.0, 30.826688766479), 1.5999997854233 },
    { "purple_house", "purple_house.glb", vec3.new(-33.573631286621, 0.0, -42.363201141357), 4.2000002861023 },
    { "red_house", "red_house.glb", vec3.new(20.240159988403, 0.0, 18.475639343262), 0.80000042915344 },
}

for idx, obj in ipairs(level_data) do
    -- local gltf = Resources.get_gltf(obj[2])
    -- for idx, v in ipairs(gltf.mesh_bundles) do
    --     local hs = v.aabb.half_size
    --     Log.trace(v.name .. " " .. tostring(hs.x) .. " " .. tostring(hs.y) .. " " .. tostring(hs.z))
    -- end

    local house = ECS.new_entity()
    ECS.insert_component(house, "house", {})
    ECS.insert_component(house, obj[1], {})
    ECS.insert_component(house, "gltf", obj[2])
    ECS.insert_component(house, "body", AABB.new(vec3.new(0), vec3.new(5, 20, 7.5)))
    ECS.insert_component(house, "trigger_body", {})
    ECS.insert_component(house, "transform", Transform.new()
        :with_position(obj[3])
        :rotated(vec3.new(0, 1, 0), -obj[4])
    )
end

local water_buy_box = ECS.new_entity()
ECS.insert_component(water_buy_box, "buy_box", {})
ECS.insert_component(water_buy_box, "water_buy_box", {})
ECS.insert_component(water_buy_box, "gltf", "water_buy_box.glb")
ECS.insert_component(water_buy_box, "body", AABB.new())
ECS.insert_component(water_buy_box, "trigger_body", {})
ECS.insert_component(water_buy_box, "transform", Transform.new():with_position(vec3.new(5, .5, 0)))

local mail_buy_box = ECS.new_entity()
ECS.insert_component(mail_buy_box, "buy_box", {})
ECS.insert_component(mail_buy_box, "mail_buy_box", {})
ECS.insert_component(mail_buy_box, "gltf", "mail_buy_box.glb")
ECS.insert_component(mail_buy_box, "body", AABB.new())
ECS.insert_component(mail_buy_box, "trigger_body", {})
ECS.insert_component(mail_buy_box, "transform", Transform.new():with_position(vec3.new(-5, .5, 0)))
