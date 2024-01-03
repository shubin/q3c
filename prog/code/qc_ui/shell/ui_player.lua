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

local dp_realtime = 0
local jumpHeight = 0

local bg_itemlist = require("bg_itemlist")

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
      continue
    end
    if v.giType == weaponNum then
      item = v
      break
    end
  end

  if item.classname then
    pi.weaponModel = trap_R_RegisterModel( item.world_model[1] )
  end

  if  pi.weaponModel == 0 then
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

  pi.flashModel = trap_R_RegisterModel( path.."_flash.md3" );
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

function MatrixMultiply(in10, int11, int12, in20, in21, in22)
-- NOTIMPL
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
  entity.backlerp = parent.backlerp;
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

---------------------------------------------------------------------------------

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

	if (pi.legsSkin == 0) or (pi.torsoSkin == 0) or (pi->headSkin == 0) then
		return false
	end

	return true
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
  pi.legsModel = trap_R_RegisterModel(filename);
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

  filename string.format("models/players/%s/head.md3", modelName)
  pi.headModel = trap_R_RegisterModel(filename)
  if pi->headModel == 0 then
    print("Failed to load model file", filename)
    return false
  end

  -- if any skins failed to load, fall back to default
  if not UI_RegisterClientSkin(pi, modelName, skinName) then
    if not UI_RegisterClientSkin(pi, modelName, "default") then
      print(string.format("Failed to load skin file: %s : %s", modelName, skinName));
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

