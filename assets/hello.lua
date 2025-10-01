if Input.is_key_pressed_this_frame("space") then
    print("Hello, world!")
    Sound.play_sound("doo-doo.mp3")
end

if Input.is_key_pressed_this_frame("f3") then
    -- Sound.play_sound("doo-doo.mp3")
    Engine.sprites[1].position.x = Engine.sprites[1].position.x + 10
end

if Input.is_key_pressed_this_frame("esc") then
    Engine.quit()
end