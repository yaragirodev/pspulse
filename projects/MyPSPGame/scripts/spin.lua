-- spin.lua - example PSPulse script
-- The owning object slowly spins and bobs above the ground.
local t = 0
local baseY = 0

function on_start()
    local x, y, z = position()
    baseY = y
    t = 0
end

function on_update(dt)
    t = t + dt
    local rx, ry, rz = rotation()
    set_rotation(rx, ry + dt * 45, rz)
    local x, y, z = position()
    set_position(x, baseY + math.sin(t * 2) * 0.35, z)

    -- Drive with the analog nub / d-pad when playing on the PSP.
    local ax = input.axis("X")
    if math.abs(ax) > 0.25 then
        set_position(x + ax * dt * 3, y, z)
    end
    if input.pressed("CROSS") then
        print("CROSS pressed on " .. name())
    end
end
