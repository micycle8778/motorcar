if Input.is_key_pressed_this_frame("d") then
    Sound.play_sound("doo-doo.mp3")
end

if Input.is_key_pressed_this_frame("s") then
    Sound.play_sound("splat.ogg")
end

if Input.is_key_pressed_this_frame("esc") then
    Engine.quit()
end

