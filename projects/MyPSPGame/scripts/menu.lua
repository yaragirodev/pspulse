-- menu.lua - simple probe controlled by the d-pad / nub
function on_start()
    print("menu.lua started")
end

function on_update(dt)
    local x, y, z = position()
    local speed = 4 * dt
    if input.down("LEFT") or input.axis("X") < -0.3 then x = x - speed end
    if input.down("RIGHT") or input.axis("X") > 0.3 then x = x + speed end
    if input.down("UP") or input.axis("Y") > 0.3 then z = z - speed end
    if input.down("DOWN") or input.axis("Y") < -0.3 then z = z + speed end
    set_position(x * 0.98, y, z * 0.98)
end
