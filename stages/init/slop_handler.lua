ECS.register_component("slop")

ECS.register_system({"slop", "global_transform", "transform", "entity"}, 
function(slop)

    local slop_speed = 10
    local gt = slop.global_transform

    if(gt:position().y <= 0) then
        ECS.delete_entity(slop.entity)
    else
        slop.transform:translate_by(slop.slop.direction * Engine.delta() * slop_speed)
        slop.transform:translate_by(vec3.new(0, Engine.delta() * -2, 0))
    end

end, "render")