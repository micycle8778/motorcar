local camera_holder_holder = ECS.new_entity()
ECS.insert_component(camera_holder_holder, "player", {})
ECS.insert_component(camera_holder_holder, "camera_holder_holder", {})
ECS.insert_component(camera_holder_holder, "transform", Transform.new():with_position(vec3.new(0, 3, 0)))
ECS.insert_component(camera_holder_holder, "body", Body.new(AABB.new(vec3.new(), vec3.new(3))))
ECS.insert_component(camera_holder_holder, "kinematic_body", {})

local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new())
ECS.insert_component(camera_holder, "parent", camera_holder_holder)

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new())
ECS.insert_component(camera, "parent", camera_holder)

local horse = ECS.new_entity()
Resources.get_gltf("horse.glb").mesh_bundles[2].should_draw = false
ECS.insert_component(horse, "gltf", "horse.glb")
ECS.insert_component(horse, "parent", camera_holder_holder)
ECS.insert_component(horse, "transform", Transform.new():with_position(vec3.new(0, -1.25, 0)):rotated(vec3.new(0, 1, 0), 3.1415))

local crosshair = ECS.new_entity()
ECS.insert_component(crosshair, "transform", Transform.new():with_scale(vec3.new(2)))
ECS.insert_component(crosshair, "sprite", "insect.png")

local speed_boost = ECS.new_entity()
ECS.insert_component(speed_boost, "speed_boost", { value = 0 })

local health = ECS.new_entity()
ECS.insert_component(health, "health", { health = 5 })

local water = ECS.new_entity()
ECS.insert_component(water, "water", { water = 100 })

local money = ECS.new_entity()
ECS.insert_component(money, "money", { money = 0 })

Input.lock_mouse()



local gltf = Resources.get_gltf("mail.glb")
function spawn_mail()
    local hs = gltf.mesh_bundles[1].aabb.half_size
    Log.warn(tostring(hs.x) .. " " .. tostring(hs.y) .. " " .. tostring(hs.z))

    local mail = ECS.new_entity()
    ECS.insert_component(mail, "gltf", "mail.glb")
    ECS.insert_component(mail, "mail", {})
    ECS.insert_component(mail, "body", Body.new(gltf.mesh_bundles[1].aabb))
    -- ECS.insert_component(mail, "body", Body.new(AABB.new(vec3.new(0), vec3.new(1))))
    ECS.insert_component(mail, "trigger_body", {})
    ECS.insert_component(mail, "transform", Transform.new()
        :with_position(vec3.new(
            -2 + Random.randf() * 4,
            -2 + Random.randf() * 4,
            -2 + Random.randf() * 4
        ))
    )
    ECS.insert_component(mail, "parent", camera_holder_holder)

    local colors = { "red_mail", "blue_mail", "purple_mail" }
    local vs = {
        red_mail = vec3.new(1, 0, 0),
        blue_mail = vec3.new(0, 0, 1),
        purple_mail = vec3.new(.7, 0, 1)
    }

    local color = colors[1 + Random.randi() % 3]

    ECS.insert_component(mail, color, {})
    ECS.insert_component(mail, "albedo", vs[color])
end

spawn_mail()
spawn_mail()
spawn_mail()
spawn_mail()
spawn_mail()
