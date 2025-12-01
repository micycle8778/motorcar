ECS.register_component("lifetime")
ECS.register_system({"lifetime", "entity"}, 
function(entity)
    entity.lifetime.time = entity.lifetime.time - Engine.delta()
    if(entity.lifetime.time <= 0) then
        ECS.delete_entity(entity.entity)
    end
end)