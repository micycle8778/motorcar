namespace motorcar {
    struct Engine;
    class PhysicsManager {
        Engine& engine;

        public:
            PhysicsManager(Engine& engine);
    };
}
