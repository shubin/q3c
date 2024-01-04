UI_TIMER_GESTURE		= 2300
UI_TIMER_JUMP			= 1000
UI_TIMER_LAND			= 130
UI_TIMER_WEAPON_SWITCH	= 300
UI_TIMER_ATTACK			= 500
UI_TIMER_MUZZLE_FLASH	= 20
UI_TIMER_WEAPON_DELAY	= 250

JUMP_HEIGHT				= 56

SWINGSPEED				= 0.3

SPIN_SPEED				= 0.9
COAST_TIME				= 1000

dp_realtime = 0
jumpHeight = 0

bg_itemlist = require("ui.bg_itemlist")

uis = {}
uis.frametime = 20
uis.xscale = 1
uis.yscale = 1

--[[
===============
UI_PlayerInfo_SetWeapon
===============
]]--
function UI_PlayerInfo_SetWeapon(pi, weaponNum)
  local item = nil
  local path = ""

  pi.currentWeapon = weaponNum
::tryagain::
  pi.realWeapon = weaponNum
  pi.weaponModel = 0
  pi.barrelModel = 0
  pi.flashModel = 0

  if weaponNum == WP_NONE then
    return
  end

  for _, v in pairs(bg_itemlist) do
    if v.giType ~= IT_WEAPON then
      goto continue
    end
    if v.giType == weaponNum then
      item = v
      break
    end
    ::continue::
  end

  if item.classname then
    pi.weaponModel = trap_R_RegisterModel( item.world_model[1] )
  end

  if pi.weaponModel == 0 then
    if weaponNum == WP_MACHINEGUN then
      weaponNum = WP_NONE
      goto tryagain
    end
    weaponNum = WP_MACHINEGUN
    goto tryagain
  end

  if weaponNum == WP_LOUSY_MACHINEGUN then
    pi.barrelModel = trap_R_RegisterModel( "models/weapons3/machinegun/machinegun_barrel.md3" )
  end

  path = item.world_model[1]:gsub("%.%w+$", "") -- strip extension
  if (weaponNum == WP_LOUSY_MACHINEGUN) or (weaponNum == WP_MACHINEGUN) or (weaponNum == WP_GAUNTLET) or (weaponNum == WP_BFG) then   
   pi.barrelModel = trap_R_RegisterModel(path.."_barrel.md3")
  end

  pi.flashModel = trap_R_RegisterModel( path.."_flash.md3" )
  pi.flashDlightColor = vec3_t(0.6, 0.6, 1) -- todo: weapon-specific flash color
end

--[[
===============
UI_ForceLegsAnim
===============
]]--
function UI_ForceLegsAnim(pi, anim)
  pi.legsAnim = ((pi.legsAnim & ANIM_TOGGLEBIT) ~ ANIM_TOGGLEBIT) | anim
  if anim == LEGS_JUMP then
    pi.legsAnimationTimer = UI_TIMER_JUMP
  end
end

--[[
===============
UI_SetLegsAnim
===============
]]--
function UI_SetLegsAnim(pi, anim)
  if pi.pendingLegsAnim then
    anim = pi.pendingLegsAnim
    pi.pendingLegsAnim = 0
  end
  UI_ForceLegsAnim(pi, anim)
end

--[[
===============
UI_ForceTorsoAnim
===============
]]--
function UI_ForceTorsoAnim(pi, anim)
  pi.torsoAnim = ((pi.torsoAnim & ANIM_TOGGLEBIT) ~ ANIM_TOGGLEBIT) | anim

  if anim == TORSO_GESTURE then
    pi.torsoAnimationTimer = UI_TIMER_GESTURE
  end

  if (anim == TORSO_ATTACK) or (anim == TORSO_ATTACK2) then
    pi.torsoAnimationTimer = UI_TIMER_ATTACK
  end
end

--[[
===============
UI_SetTorsoAnim
===============
]]--
function UI_SetTorsoAnim(pi, anim)
  if pi.pendingTorsoAnim then
    anim = pi.pendingTorsoAnim
    pi.pendingTorsoAnim = 0
  end

  UI_ForceTorsoAnim(pi, anim)
end

--[[
===============
UI_TorsoSequencing
===============
]]--
function UI_TorsoSequencing(pi)
  local currentAnim = pi.torsoAnim & ~ANIM_TOGGLEBIT

  if pi.weapon ~= pi.currentWeapon then
    if currentAnim ~= TORSO_DROP then
      pi.torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH
      UI_ForceTorsoAnim(pi, TORSO_DROP)
    end
  end

  if pi.torsoAnimationTimer > 0 then
    return
  end

  if currentAnim == TORSO_GESTURE then
    UI_SetTorsoAnim(pi, TORSO_STAND)
    return
  end

  if (currentAnim == TORSO_ATTACK) or (currentAnim == TORSO_ATTACK2) then
    UI_SetTorsoAnim(pi, TORSO_STAND)
    return
  end

  if (currentAnim == TORSO_DROP) then
    UI_PlayerInfo_SetWeapon(pi, pi.weapon)
    pi.torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH
    UI_ForceTorsoAnim(pi, TORSO_RAISE)
    return
  end

  if currentAnim == TORSO_RAISE then
    UI_SetTorsoAnim(pi, TORSO_STAND)
    return
  end
end


--[[
===============
UI_LegsSequencing
===============
]]--
function UI_LegsSequencing(pi)
  local currentAnim = pi.legsAnim & ~ANIM_TOGGLEBIT

  if pi.legsAnimationTimer > 0 then
    if currentAnim == LEGS_JUMP then
      jumpHeight = JUMP_HEIGHT * math.sin(math.pi * (UI_TIMER_JUMP - pi.legsAnimationTimer) / UI_TIMER_JUMP)
    end
    return
  end

  if currentAnim == LEGS_JUMP then
    UI_ForceLegsAnim(pi, LEGS_LAND)
    pi.legsAnimationTimer = UI_TIMER_LAND
    jumpHeight = 0
    return
  end

  if currentAnim == LEGS_LANDthen then
    UI_SetLegsAnim(pi, LEGS_IDLE)
    return
  end
end

function VectorMA(v, s, b)
  return vec3_t(v.x + s * b.x, v.y + s * b.y, v.z + s * b.z)
end

function MatrixMultiply(in10, in11, in12, in20, in21, in22)
  return vec3_t(
    in10.x * in20.x + in10.y * in21.x + in10.z * in22.x,
    in10.x * in20.y + in10.y * in21.y + in10.z * in22.y,
    in10.x * in20.z + in10.y * in21.z + in10.z * in22.z
  ),
  vec3_t(
    in11.x * in20.x + in11.y * in21.x + in11.z * in22.x,
    in11.x * in20.y + in11.y * in21.y + in11.z * in22.y,
    in11.x * in20.z + in11.y * in21.z + in11.z * in22.z
  ),
  vec3_t(
	in12.x * in20.x + in12.y * in21.x + in12.z * in22.x,
	in12.x * in20.y + in12.y * in21.y + in12.z * in22.y,
	in12.x * in20.z + in12.y * in21.z + in12.z * in22.z
  )
end

--[[
======================
UI_PositionEntityOnTag
======================
]]--
function UI_PositionEntityOnTag(entity, parent, parentModel, tagName)
  local lerped = orientation_t()
	
  -- lerp the tag
  trap_CM_LerpTag(lerped, parentModel, parent.oldframe, parent.frame, 1.0 - parent.backlerp, tagName)

  -- FIXME: allow origin offsets along tag?
  entity.origin = vec3_t(parent.origin)
  entity.origin = VectorMA(entity.origin, lerped.origin[0], parent.axis0)
  entity.origin = VectorMA(entity.origin, lerped.origin[1], parent.axis1)
  entity.origin = VectorMA(entity.origin, lerped.origin[2], parent.axis2)

  entity.axis0, entity.axis1, entity.axis2 = MatrixMultiply(lerped.axis0, lerped.axis1, lerped.axis2, parent.axis0, parent.axis1, parent.axis2)
  entity.backlerp = parent.backlerp
end

--[[
======================
UI_PositionRotatedEntityOnTag
======================
]]--
function UI_PositionRotatedEntityOnTag(entity, parent, parentModel, tagName)
  local lerped = orientation_t()

  -- lerp the tag
  trap_CM_LerpTag(lerped, parentModel, parent.oldframe, parent.frame, 1.0 - parent.backlerp, tagName)

  -- FIXME: allow origin offsets along tag?
  entity.origin = vec3_t(parent.origin)
  entity.origin = VectorMA(entity.origin, lerped.origin[0], parent.axis0)
  entity.origin = VectorMA(entity.origin, lerped.origin[1], parent.axis1)
  entity.origin = VectorMA(entity.origin, lerped.origin[2], parent.axis2)

  local tampAxis0, tempAxis1, tempAxis2 = MatrixMultiply(entity.axis0, entity.axis1, entity.axis2, lerped.axis0, lerped.axis1, lerped.axis1)
  entity.axis0, entity.axis1, entity.axis2 = MatrixMultiply(tempAxis0, tempAxis1, tempAxis2, parent.axis0, parent.axis1, parent.axis2)   
end

--[[
===============
UI_SetLerpFrameAnimation
===============
]]--
function UI_SetLerpFrameAnimation(ci, lf, newAnimation)
	local anim = {}

	lf.animationNumber = newAnimation
	newAnimation = newAnimation & ~ANIM_TOGGLEBIT

	if (newAnimation < 0) or (newAnimation >= MAX_ANIMATIONS) then
		trap_Error( string.format("Bad animation number: %i", newAnimation) )
	end

	anim = ci.animations[newAnimation]

	lf.animation = anim
	lf.animationTime = lf.frameTime + anim.initialLerp
end

--[[
===============
UI_RunLerpFrame
===============
]]--
function UI_RunLerpFrame(ci, lf, newAnimation)
  -- see if the animation sequence is switching
  if (newAnimation ~= lf.animationNumber) or (lf.animation == nil) then
    UI_SetLerpFrameAnimation(ci, lf, newAnimation)
  end

  --if we have passed the current frame, move it to
  -- oldFrame and calculate a new frame
  if dp_realtime >= lf.frameTime then
    lf.oldFrame = lf.frame
    lf.oldFrameTime = lf.frameTime

    -- get the next frame based on the animation
    anim = lf.animation
    if anim.frameLerp == 0 then
      return		-- shouldn't happen
    end
    if dp_realtime < lf.animationTime then
      lf.frameTime = lf.animationTime		-- initial lerp
    else
      lf.frameTime = lf.oldFrameTime + anim.frameLerp
    end
    f = (lf.frameTime - lf.animationTime) / anim.frameLerp

    local numFrames = anim.numFrames
    if anim.flipflop then
      numFrames = numFrames * 2
    end
    if f >= numFrames then
      f = f - numFrames
      if anim.loopFrames then
        f = f % anim.loopFrames
        f = f + anim.numFrames - anim.loopFrames
      else
        f = numFrames - 1
        -- the animation is stuck at the end, so it
        -- can immediately transition to another sequence
		lf.frameTime = dp_realtime
      end
    end
    if anim.reversed then
      lf.frame = anim.firstFrame + anim.numFrames - 1 - f
    else if anim.flipflop and (f >= anim.numFrames) then
      lf.frame = anim.firstFrame + anim.numFrames - 1 - (f % anim.numFrames)
	else
      lf.frame = anim.firstFrame + f
    end end
    if dp_realtime > lf.frameTime then
      lf.frameTime = dp_realtime
	end
  end

  if lf.frameTime > dp_realtime + 200 then
    lf.frameTime = dp_realtime
  end

  if lf.oldFrameTime > dp_realtime then
    lf.oldFrameTime = dp_realtime
  end
  -- calculate current lerp value
  if lf.frameTime == lf.oldFrameTime then
    lf.backlerp = 0
  else
    lf.backlerp = 1.0 - (dp_realtime - lf.oldFrameTime) / (lf.frameTime - lf.oldFrameTime)
  end
end

--[[
===============
UI_PlayerAnimation
===============
]]--
function UI_PlayerAnimation(pi)
  local legsOld, legs, legsBackLerp, torsoOld, torso, torsoBackLerp

  -- legs animation
  pi.legsAnimationTimer = pi.legsAnimationTimer - uis.frametime
  if pi.legsAnimationTimer < 0 then
    pi.legsAnimationTimer = 0
  end

  UI_LegsSequencing(pi)

  if pi.legs.yawing and ((pi.legsAnim & ~ANIM_TOGGLEBIT) == LEGS_IDLE) then
    UI_RunLerpFrame(pi, pi.legs, LEGS_TURN)
  else
    UI_RunLerpFrame(pi, pi.legs, pi.legsAnim)
  end
  legsOld = pi.legs.oldFrame
  legs = pi.legs.frame
  legsBackLerp = pi.legs.backlerp

  -- torso animation
  pi.torsoAnimationTimer = pi.torsoAnimationTimer - uis.frametime
  if pi.torsoAnimationTimer < 0 then
    pi.torsoAnimationTimer = 0
  end

  UI_TorsoSequencing(pi)

  UI_RunLerpFrame(pi, pi.torso, pi.torsoAnim)
  torsoOld = pi.torso.oldFrame
  torso = pi.torso.frame
  torsoBackLerp = pi.torso.backlerp

  return legsOld, legs, legsBackLerp, torsoOld, torso, torsoBackLerp
end

--[[
=================
AngleSubtract

Always returns a value from -180 to 180
=================
]]--
function AngleSubtract(a1, a2)
	local a = a1 - a2
	while a > 180 do
		a = a - 360
	end
	while a < -180 do
		a = a + 360
	end
	return a
end

function AngleMod(a)
  local r = (360.0/65536) * (math.floor(a*(65536/360.0)) & 65535)
  return r
end

--[[
==================
UI_SwingAngles
==================
]]--
function UI_SwingAngles(destination, swingTolerance, clampTolerance, speed, angle, swinging)
  -- return angle, swinging
  local swing
  local move
  local scale

  if swinging then
    -- see if a swing should be started
    swing = AngleSubtract(angle, destination)
    if (swing > swingTolerance) or (swing < -swingTolerance) then
      swinging = true
    end
  end

  if ~swinging then
    return angle, swinging
  end
	
  -- modify the speed depending on the delta
  -- so it doesn't seem so linear
  swing = AngleSubtract(destination, angle)
  scale = math.abs(swing)
  if scale < swingTolerance * 0.5 then
    scale = 0.5
  else if scale < swingTolerance then
    scale = 1.0
  else
    scale = 2.0
  end end

  -- swing towards the destination angle
  if swing >= 0 then
    move = uis.frametime * scale * speed
    if move >= swing then
      move = swing
      swinging = false
    end
    angle = AngleMod(angle + move)
  else if swing < 0 then
    move = uis.frametime * scale * -speed
    if move <= swing then
      move = swing
      swinging = false
    end
    angle = AngleMod(angle + move)
  end end

  -- clamp to no more than tolerance
  swing = AngleSubtract(destination, angle)
  if swing > clampTolerance then
    angle = AngleMod(destination - (clampTolerance - 1))
  else if swing < -clampTolerance then
    angle = AngleMod(destination + (clampTolerance - 1))
  end end
  return angle, swinging 
end

function AngleVectors(angles, forward, right, up)
  local angle = angles.y * (math.pi*2 / 360)
  local sy = math.sin(angle)
  local cy = math.cos(angle)
  angle = angles.x * (math.pi*2 / 360)
  local sp = math.sin(angle)
  local cp = math.cos(angle)
  angle = angles.z * (math.pi*2 / 360)
  local sr = math.sin(angle)
  local cr = math.cos(angle)

  if forward then
    forward.x = cp*cy
    forward.y = cp*sy
    forward.z = -sp
  end
  if right then
    right.x = (-1*sr*sp*cy+-1*cr*-sy)
    right.y = (-1*sr*sp*sy+-1*cr*cy)
    right.z = -1*sr*cp
  end
  if up then
    up.x = (cr*sp*cy+-sr*-sy)
    up.y = (cr*sp*sy+-sr*cy)
    up.z = cr*cp
  end
end

--[[
======================
UI_MovedirAdjustment
======================
]]--
function UI_MovedirAdjustment(pi)
  local relativeAngles = vec3_t(0, 0, 0)
  local moveVector = vec3_t(0, 0, 0)

  relativeAngles.x = pi.viewAngles.x - p.moveAngles.x
  relativeAngles.y = pi.viewAngles.y - p.moveAngles.y
  relativeAngles.z = pi.viewAngles.z - p.moveAngles.z

  AngleVectors(relativeAngles, moveVector, nil, nil)
  if math.abs(moveVector.x) < 0.01 then
    moveVector.x = 0.0
  end
  if math.abs(moveVector.y ) < 0.01 then
    moveVector.y = 0.0
  end

  if (moveVector.y == 0) and (moveVector.x > 0) then
    return 0
  end
  if (moveVector.y < 0) and (moveVector.x > 0) then
    return 22
  end
  if (moveVector.y < 0) and (moveVector.x == 0) then
    return 45
  end
  if (moveVector.y < 0) and (moveVector.x < 0) then
    return -22
  end
  if (moveVector.y == 0) and (moveVector.x < 0) then
    return 0
  end
  if (moveVector.y > 0) and (moveVector.x < 0) then
    return 22
  end
  if (moveVector.y > 0) and (moveVector.x == 0) then
    return  -45
  end
  return -22
end

vec3_origin = vec3_t(0, 0, 0)

--[[
=================
AnglesToAxis
=================
]]--
function AnglesToAxis(angles, axis0, axis1, axis2)
  local right = vec3_t(0, 0, 0)
  -- angle vectors returns "right" instead of "y axis"
  AngleVectors(angles, axis0, right, axis2)
  axis1.x = vec3_origin.x - right.x
  axis1.y = vec3_origin.y - right.y
  axis1.z = vec3_origin.z - right.z
end

--[[
===============
UI_PlayerAngles
===============
]]--
function UI_PlayerAngles(pi, legs0, legs1, legs2, torso0, torso1, torso2, head0, head1, head2)
  local legsAngles = vec3_t(0, 0, 0)
  local torsoAngles = vec3_t(0, 0, 0)
  local headAngles = vec3_t(0, 0, 0)
  local dest, adjust

  headAngles.x = pi.viewAngles.x
  headAngles.y = pi.viewAngles.y
  headAngles.z = pi.viewAngles.z

  headAngles.y = AngleMod(headAngles.y)

  -- --------- yaw -------------

  -- allow yaw to drift a bit
  if  ((pi.legsAnim & ~ANIM_TOGGLEBIT) ~= LEGS_IDLE) or ((pi.torsoAnim & ~ANIM_TOGGLEBIT) ~= TORSO_STAND) then
    --if not standing still, always point all in the same direction
    pi.torso.yawing = true	  -- always center
    pi.torso.pitching = true -- always center
    pi.legs.yawing = true    -- always center
  end

  -- adjust legs for movement dir
  adjust = UI_MovedirAdjustment(pi)
  legsAngles.y = headAngles.y + adjust
  torsoAngles.y = headAngles.y + 0.25 * adjust

  -- torso
  pi.torso.yawAngle, pi.torso.yawing = UI_SwingAngles(torsoAngles.y, 25, 90, SWINGSPEED, pi.torso.yawAngle, pi.torso.yawing)
  pi.legs.yawAngle, pi.legs.yawing   = UI_SwingAngles(legsAngles.y, 40, 90, SWINGSPEED, pi.legs.yawAngle, pi.legs.yawing)

  torsoAngles.y = pi.torso.yawAngle
  legsAngles.y = pi.legs.yawAngle

  -- --------- pitch -------------

  -- only show a fraction of the pitch angle in the torso
  if headAngles.x > 180 then
    dest = (-360 + headAngles.x) * 0.75
  else
    dest = headAngles.x * 0.75
  end
  UI_SwingAngles(dest, 15, 30, 0.1, pi.torso.pitchAngle, pi.torso.pitching)
  torsoAngles.x = pi.xtorso.pitchAngle

  if pi.fixedtorso then
    torsoAngles.x = 0.0
  end

  if pi.fixedlegs then
    legsAngles.y = torsoAngles.y
    legsAngles.x = 0.0
    legsAngles.x = 0.0
  end

  -- pull the angles back out of the hierarchial chain
  AnglesSubtract( headAngles, torsoAngles, headAngles )
  AnglesSubtract( torsoAngles, legsAngles, torsoAngles )
  AnglesToAxis( legsAngles, legs0, legs1, legs2 )
  AnglesToAxis( torsoAngles, torso0, torso1, torso2 )
  AnglesToAxis( headAngles, head0, head1, head2 )
end

--[[
===============
UI_PlayerFloatSprite
===============
]]--
function UI_PlayerFloatSprite(pi, origin, shader)
  local ent = refEntity_t()
  ent.origin = vec3_t(origin.x, origin.y, origin.z + 48)
  ent.reType = RT_SPRITE
  ent.customShader = shader
  ent.radius = 10
  ent.renderfx = 0
  trap_R_AddRefEntityToScene(ent)
end

--[[
======================
UI_MachinegunSpinAngle
======================
]]--
function UI_MachinegunSpinAngle(pi)
  local delta
  local angle
  local speed
  local torsoAnim

  delta = dp_realtime - pi.barrelTime
  if pi.barrelSpinning then
    angle = pi.barrelAngle + delta * SPIN_SPEED
  else
    if delta > COAST_TIME then
      delta = COAST_TIME
    end

    speed = 0.5 * ( SPIN_SPEED + ( COAST_TIME - delta ) / COAST_TIME )
    angle = pi.barrelAngle + delta * speed
  end

  torsoAnim = pi.torsoAnim  & ~ANIM_TOGGLEBIT
  if torsoAnim == TORSO_ATTACK2 then
    torsoAnim = TORSO_ATTACK
  end
  if pi.barrelSpinning == ~(torsoAnim == TORSO_ATTACK) then
    pi.barrelTime = dp_realtime
    pi.barrelAngle = AngleMod(angle)
    pi.barrelSpinning = (torsoAnim == TORSO_ATTACK)
  end
  return angle
end

--[[
===============
UI_DrawPlayer
===============
]]--
function UI_DrawPlayer(x, y, w, h, pi, time)
  local refdef = refdef_t()
  local legs = refEntity_t()
  local torso = refEntity_t()
  local head = refEntity_t()
  local gun = refEntity_t()
  local barrel = refEntity_t()
  local flash = refEntity_t()
  local origin = vec3_t(0, 0, 0)
  local renderfx
  local mins = vec3_t(-16, -16, -24)
  local maxs = vec3_t(16, 16, 32)
  local len
  local xx

  if (pi.legsModel == 0) or (pi.torsoModel == 0) or (pi.headModel == 0) or (pi.animations[1].numFrames == 0) then
    return
  end

  dp_realtime = time

  if (pi.pendingWeapon ~= WP_NUM_WEAPONS) and (dp_realtime > pi.weaponTimer) then
    pi.weapon = pi.pendingWeapon
    pi.lastWeapon = pi.pendingWeapon
    pi.pendingWeapon = WP_NUM_WEAPONS
    pi.weaponTimer = 0
    if pi.currentWeapon ~= pi.weapon then
      trap_S_StartLocalSound(weaponChangeSound, CHAN_LOCAL)
    end
  end

  -- UI_AdjustFrom640( &x, &y, &w, &h )

  y = y - jumpHeight

  refdef.rdflags = RDF_NOWORLDMODEL
  refdef.viewaxis0 = vec3_t(1, 0, 0)
  refdef.viewaxis1 = vec3_t(0, 1, 0)
  refdef.viewaxis2 = vec3_t(0, 0, 1)

  refdef.x = x
  refdef.y = y
  refdef.width = w
  refdef.height = h

  refdef.fov_x = math.floor(refdef.width / uis.xscale / 640.0 * 90.0)
  xx = refdef.width / uis.xscale / math.tan( refdef.fov_x / 360 * math.pi)
  refdef.fov_y = math.atan2(refdef.height / uis.yscale, xx) * (360 / math.pi)

  -- calculate distance so the player nearly fills the box
  len = 0.7 * (maxs.z - mins.z)
  origin.x = len / math.tan( refdef.fov_x / 180 * math.pi * 0.5 )
  origin.y = 0.5 * (mins.y + maxs.y)
  origin.z = -0.5 * (mins.z + maxs.z)

  refdef.time = dp_realtime

  trap_R_ClearScene()

  -- get the rotation information
  local legs_axis0 = vec3_t(0, 0, 0)
  local legs_axis1 = vec3_t(0, 0, 0)
  local legs_axis2 = vec3_t(0, 0, 0)
  local torso_axis0 = vec3_t(0, 0, 0)
  local torso_axis1 = vec3_t(0, 0, 0)
  local torso_axis2 = vec3_t(0, 0, 0)
  local head_axis0 = vec3_t(0, 0, 0)
  local head_axis1 = vec3_t(0, 0, 0)
  local head_axis2 = vec3_t(0, 0, 0)
  UI_PlayerAngles( pi,
    legs_axis0, legs_axis1, legs_axis2,
    torso_axis0, torso_axis1, torso_axis2,
    head_axis0, head_axis1, head_axis2 )
  legs.axis0 = legs_axis0
  legs.axis1 = legs_axis1
  legs.axis2 = legs_axis2
  torso.axis0 = torso_axis0
  torso.axis1 = torso_axis1
  torso.axis2 = torso_axis2
  
	
  -- get the animation state (after rotation, to allow feet shuffle)
  legs.oldframe, legs.frame, legs.backlerp, torso.oldframe, torso.frame, torso.backlerp = UI_PlayerAnimation(pi)
  renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW

  -- 
  -- add the legs
  --
  legs.hModel = pi.legsModel
  legs.customSkin = pi.legsSkin

  legs.origin = origin

  legs.lightingOrigin = origin
  legs.renderfx = renderfx
  legs.oldorigin = origin

  trap_R_AddRefEntityToScene(legs)

  -- 
  -- add the torso
  --
  torso.hModel = pi.torsoModel
  torso.customSkin = pi.torsoSkin

  torso.lightingOrigin = origin

  UI_PositionRotatedEntityOnTag(torso, legs, pi.legsModel, "tag_torso")

  torso.renderfx = renderfx

  trap_R_AddRefEntityToScene( torso )

  --
  -- add the head
  --
  head.hModel = pi.headModel
  head.customSkin = pi.headSkin

  head.lightingOrigin = origin

  UI_PositionRotatedEntityOnTag(head, torso, pi.torsoModel, "tag_head")

  head.renderfx = renderfx

  trap_R_AddRefEntityToScene(head)

  --
  -- add the gun
  --

  if pi.currentWeapon ~= WP_NONE then
    gun.hModel = pi.weaponModel
    if pi.currentWeapon == WP_RAILGUN then
      gun.shaderRGBA = pi.c1RGBA
    else
      gun.shaderRGBA = rgba_t(255, 255, 255, 255)
    end
    gun.lightingOrigin = origin
    UI_PositionEntityOnTag( gun, torso, pi.torsoModel, "tag_weapon")
    gun.renderfx = renderfx
    trap_R_AddRefEntityToScene(gun)
  end
  --
  -- add the spinning barrel
  --
  if (pi.realWeapon == WP_LOUSY_MACHINEGUN) or (pi.realWeapon == WP_MACHINEGUN) or (pi.realWeapon == WP_GAUNTLET) or (pi.realWeapon == WP_BFG) then
    local angles = vec3_t(0, 0, 0)		
    barrel.lightingOrigin = origin
  	barrel.renderfx = renderfx
    barrel.hModel = pi.barrelModel
    angles.y = 0
    angles.x = 0
    angles.z = UI_MachinegunSpinAngle(pi)
    local barrel_axis0 = vec3_t(0, 0, 0)
    local barrel_axis1 = vec3_t(0, 0, 0)
    local barrel_axis2 = vec3_t(0, 0, 0)
    AnglesToAxis(angles, barrel_axis0, barrel_axis1, barrel_axis2)
    barrel.axis0 = barrel_axis0
    barrel.axis1 = barrel_axis1
    barrel.axis2 = barrel_axis2

    UI_PositionRotatedEntityOnTag(barrel, gun, pi.weaponModel, "tag_barrel")

    trap_R_AddRefEntityToScene(barrel)
  end

  --
  -- add muzzle flash
  --
  if dp_realtime <= pi.muzzleFlashTime then
    if pi.flashModel then
      flash.hModel = pi.flashModel
      if pi.currentWeapon == WP_RAILGUN then
        flash.shaderRGBA = pi.c1RGBA
      else
        flash.shaderRGBA = rgba_t(255, 255, 255, 255)
      end
      flash.lightingOrigin = origin
        
      UI_PositionEntityOnTag(flash, gun, pi.weaponModel, "tag_flash")
      flash.renderfx = renderfx
      trap_R_AddRefEntityToScene(flash)
    end

    -- make a dlight for the flash
    if (pi.flashDlightColor.x > 0) or (pi.flashDlightColor.y > 0) or (pi.flashDlightColor.z > 0) then
      trap_R_AddLightToScene( flash.origin, 199 + math.random(32), pi.flashDlightColor.x, pi.flashDlightColor.y, pi.flashDlightColor.z )
    end
  end

  --
  -- add the chat icon
  --
  if pi.chat then
    UI_PlayerFloatSprite( pi, origin, trap_R_RegisterShaderNoMip( "sprites/balloon3" ) )
  end

  --
  -- add an accent light
  --
  origin = vec3_t(origin.x - 100, origin.y + 100, origin.z + 100)
  trap_R_AddLightToScene( origin, 500, 1.0, 1.0, 1.0 )

  origin = vec3_t(origin.x - 100, origin.y - 100, origin.z - 100)
  trap_R_AddLightToScene( origin, 500, 1.0, 0.0, 0.0 )

  trap_R_RenderScene( refdef )
end

--[[
==========================
UI_RegisterClientSkin
==========================
]]--
function UI_RegisterClientSkin(pi, modelName, skinName)
	local filename = ""

	filename = string.format("models/players/%s/lower_%s.skin", modelName, skinName)
	pi.legsSkin = trap_R_RegisterSkin(filename)

	filename = string.format("models/players/%s/upper_%s.skin", modelName, skinName)
	pi.torsoSkin = trap_R_RegisterSkin(filename)

	filename = string.format("models/players/%s/head_%s.skin", modelName, skinName)
	pi.headSkin = trap_R_RegisterSkin(filename)

	if (pi.legsSkin == 0) or (pi.torsoSkin == 0) or (pi.headSkin == 0) then
		return false
	end

	return true
end

function magiclines(s)
  if s:sub(-1) ~= "\n" then
    s = s.."\n"
  end
  return s:gmatch("(.-)\n")
end

--[[
======================
UI_ParseAnimationFile
======================
]]--
-- static qboolean UI_ParseAnimationFile( const char *filename, playerInfo_t *pi ) {
function UI_ParseAnimationFile(filename, pi)
  local animations = {}
  
  pi.fixedLegs = false
  pi.fixedTorso = false
  
  local filecontents = readfile(filename)
  if not filecontents then
    return false
  end

  local parseframes = false
  local i = 0
  for line in magiclines(filecontents) do
    line = line:gsub('//.*$', ''):match("^%s*(.-)%s*$")
    local tokens = {}
    for token in line:gmatch("%w+") do
      table.insert(tokens, token)
    end
  ::do_frames::
    if parseframes then
      -- frames
      i = i + 1
      if tokens[1] == nil then
        if (i >= TORSO_GETFLAG) and (i <= TORSO_NEGATIVE) then
          animations[i].firstFrame = animations[TORSO_GESTURE].firstFrame
          animations[i].frameLerp = animations[TORSO_GESTURE].frameLerp
          animations[i].initialLerp = animations[TORSO_GESTURE].initialLerp
          animations[i].loopFrames = animations[TORSO_GESTURE].loopFrames
          animations[i].numFrames = animations[TORSO_GESTURE].numFrames
          animations[i].reversed = false
          animations[i].flipflop = false
          goto continue
        end
        break
      end
      if #tokens ~= 4 then
        break
      end
      animations[i] = {}

      animations[i].firstFrame = tonumber(tokens[1])
      if i == LEGS_WALKCR then
        skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame
      end
      if (i >= LEGS_WALKCR) and  (i < TORSO_GETFLAG ) then
        animations[i].firstFrame = animations[i].firstFrame - skip
      end
      animations[i].numFrames = tonumber(tokens[2])
      animations[i].reversed = false
      animations[i].flipFlop = false
      -- if numFrames is negative the animation is reversed
      if animations[i].numFrames < 0 then
        animations[i].numFrames = -animations[i].numFrames
        animations[i].reversed = true
      end
      animations[i].loopFrames = tonumber(tokens[3])      
      animations[i].fps = math.min(1, tonumber(tokens[4]))
      animations[i].frameLerp = 1000 / animations[i].fps
      animations[i].frameLerp = animations[i].initialLerp
    else
      if ~tokens[1] then
        goto do_frames
      end
      if tokens[1] == "footsteps" then
        if ~tokens[2] then
          goto do_frames
        end
        goto continue
      else if tokens[1] == "headoffset" then
        if ~tokens[2] or ~tokens[3] or ~tokens[4] then
          goto do_frames
        end
        goto continue
      else if tokens[1] == "sex" then
        if ~tokens[2] then
          goto do_frames
        end
        goto continue
      else if tokens[1] == "fixedlegs" then
        pi.fixedlegs = true
        goto continue
      else if tokens[1] == "fixedtorso" then
        pi.fixedtorso = true
        goto continue
      else if tonumber(tokens[1]) then
        parseframes = true
        goto do_frames
      end end end end end end
      print(string.format("unknown token '%s' in %s", tokens[1], filename))
    end
    ::continue::
  end
  if i ~= MAX_ANIMATIONS then
    print(string.format("Error parsing animation file: %s", filename))
  end
end

--[[
==========================
UI_RegisterClientModelname
==========================
]]--
function UI_RegisterClientModelname(pi, modelSkinName)
  local modelName = ""
  local skinName = ""
  local filename = ""
  local slash = nil

  pi.torsoModel = 0
  pi.headModel = 0

  if (not modelSkinName) or (modelSkinName.len() == 0) then
    return false
  end

  modelName = modelSkinName

  slash = modelName:find("/")
  if not slash then
    -- modelName did not include a skin name
    skinName = "default"
  else
    skinName = modelName:sub(slash + 1)
    -- truncate modelName
    modelName = modelName:sub(1, slash - 1)
  end

  --  load cmodels before models so filecache works
  filename = string.format("models/players/%s/lower.md3", modelName )
  pi.legsModel = trap_R_RegisterModel(filename)
  if pi.legsModel == 0 then
    print("Failed to load model file", filename)
    return false
  end
  
  filename = string.format("models/players/%s/upper.md3", modelName)
  pi.torsoModel = trap_R_RegisterModel(filename)
  if pi.torsoModel == 0 then
    print("Failed to load model file", filename)
    return false
  end

  filename = string.format("models/players/%s/head.md3", modelName)
  pi.headModel = trap_R_RegisterModel(filename)
  if pi.headModel == 0 then
    print("Failed to load model file", filename)
    return false
  end

  -- if any skins failed to load, fall back to default
  if not UI_RegisterClientSkin(pi, modelName, skinName) then
    if not UI_RegisterClientSkin(pi, modelName, "default") then
      print(string.format("Failed to load skin file: %s : %s", modelName, skinName))
      return false
    end
  end

  -- load the animations
  filename = string.format("models/players/%s/animation.cfg", modelName)
  if not UI_ParseAnimationFile(filename, pi) then
    print("Failed to load animation file", filename)
    return false
   end

  return true
end


--[[
===============
UI_PlayerInfo_SetModel
===============
]]--
function UI_PlayerInfo_SetModel( pi, model )	
  UI_RegisterClientModelname( pi, model )
  pi.weapon = WP_MACHINEGUN
  pi.currentWeapon = pi.weapon
  pi.lastWeapon = pi.weapon
  pi.pendingWeapon = WP_NUM_WEAPONS
  pi.weaponTimer = 0
  pi.chat = false
  pi.newModel = true
  UI_PlayerInfo_SetWeapon( pi, pi.weapon )
end


--[[
===============
UI_PlayerInfo_SetInfo
===============
]]--
function UI_PlayerInfo_SetInfo(pi, legsAnim, torsoAnim, viewAngles, moveAngles, weaponNumber, chat)
  local currentAnim
  local weaponNum
  local c

  pi.chat = chat

  c = math.floor(trap_Cvar_VariableValue("color1"))

  pi.color1 = vec3_t(1, 1, 1)  -- todo color parsing

  pi.c1RGBA = rgba_t(255 * pi.color1.x, 255 * pi.color1.y, 255 * pi.color1.z, 255)
  pi.viewAngles = viewAngles

  -- move angles
  pi.moveAngles = moveAngles

  if pi.newModel then
    pi.newModel = false

    jumpHeight = 0
    pi.pendingLegsAnim = 0
    UI_ForceLegsAnim(pi, legsAnim)
    pi.legs.yawAngle = viewAngles.y
    pi.legs.yawing = false

    pi.pendingTorsoAnim = 0
    UI_ForceTorsoAnim(pi, torsoAnim)
    pi.torso.yawAngle = viewAngles.y
    pi.torso.yawing = false

    if weaponNumber ~= WP_NUM_WEAPONS then
      pi.weapon = weaponNumber
      pi.currentWeapon = weaponNumber
      pi.lastWeapon = weaponNumber
      pi.pendingWeapon = WP_NUM_WEAPONS
      pi.weaponTimer = 0
      UI_PlayerInfo_SetWeapon(pi, pi.weapon)
    end
    return
  end

  -- weapon
  if weaponNumber == WP_NUM_WEAPONS then
    pi.pendingWeapon = WP_NUM_WEAPONS
    pi.weaponTimer = 0
  else if weaponNumber ~= WP_NONE then
    pi.pendingWeapon = weaponNumber
    pi.weaponTimer = dp_realtime + UI_TIMER_WEAPON_DELAY
  end end
  weaponNum = pi.lastWeapon
  pi.weapon = weaponNum

  if (torsoAnim == BOTH_DEATH1) or (legsAnim == BOTH_DEATH1) then
    torsoAnim = BOTH_DEATH1
    legsAnim = BOTH_DEATH1
    pi.weapon = WP_NONE
    pi.currentWeapon = WP_NONE
    UI_PlayerInfo_SetWeapon(pi, pi.weapon)

    jumpHeight = 0
    pi.pendingLegsAnim = 0
    UI_ForceLegsAnim(pi, legsAnim)

    pi.pendingTorsoAnim = 0
    UI_ForceTorsoAnim(pi, torsoAnim)
    return
  end

  -- leg animation
  currentAnim = pi.legsAnim & ~ANIM_TOGGLEBIT
  if (legsAnim ~= LEGS_JUMP) and ((currentAnim == LEGS_JUMP) or (currentAnim == LEGS_LAND)) then
    pi.pendingLegsAnim = legsAnim
  else if legsAnim ~= currentAnim then
    jumpHeight = 0
    pi.pendingLegsAnim = 0
    UI_ForceLegsAnim(pi, legsAnim)
  end end

  -- torso animation
  if (torsoAnim == TORSO_STAND) or (torsoAnim == TORSO_STAND2) then
    if (weaponNum == WP_NONE) or (weaponNum == WP_GAUNTLET) then
      torsoAnim = TORSO_STAND2
    else
      torsoAnim = TORSO_STAND
    end
  end

  if (torsoAnim == TORSO_ATTACK) or (torsoAnim == TORSO_ATTACK2) then
    if (weaponNum == WP_NONE) or (weaponNum == WP_GAUNTLET) then
      torsoAnim = TORSO_ATTACK2
    else
      torsoAnim = TORSO_ATTACK
    end
    pi.muzzleFlashTime = dp_realtime + UI_TIMER_MUZZLE_FLASH
    --FIXME play firing sound here
  end

  currentAnim = pi.torsoAnim & ~ANIM_TOGGLEBIT

  if (weaponNum ~= pi.currentWeapon) or (currentAnim == TORSO_RAISE) or (currentAnim == TORSO_DROP) then
    pi.pendingTorsoAnim = torsoAnim
  else if ((currentAnim == TORSO_GESTURE) or (currentAnim == TORSO_ATTACK)) and (torsoAnim ~= currentAnim) then
    pi.pendingTorsoAnim = torsoAnim
  else if torsoAnim ~= currentAnim then
    pi.pendingTorsoAnim = 0
    UI_ForceTorsoAnim(pi, torsoAnim)
  end end end
end
