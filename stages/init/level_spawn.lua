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
    local house = ECS.new_entity()
    ECS.insert_component(house, "house", {})
    ECS.insert_component(house, obj[1], {})
    ECS.insert_component(house, "gltf", obj[2])
    ECS.insert_component(house, "transform", Transform.new()
        :with_position(obj[3])
        :rotated(vec3.new(0, 1, 0), -obj[4])
    )
end

