if Input.is_key_pressed_this_frame("space") then
    Log.trace("Hello, world!")
    Log.debug("Hello, world!")
    Log.warn("Hello, world!")
    Log.error("Hello, world!")
    Sound.play_sound("doo-doo.mp3")
end

if Input.is_key_pressed_this_frame("f3") then
    Engine.test()
    Engine.sprites[1].position.x = Engine.sprites[1].position.x + 10
end

if Input.is_key_pressed_this_frame("esc") then
    Engine.quit()
end