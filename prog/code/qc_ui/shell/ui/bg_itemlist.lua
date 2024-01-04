return {
--QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_armor_shard", 
		["pickup_sound"] = "sound/misc/ar1_pkup.wav",
		["world_model"] = { "models/powerups/armor/shard.md3", "models/powerups/armor/shard_sphere.md3" },
		["icon"] = "qc/icons/iconr_shard",
		["pickup_name"] = "Armor Shard",
		["quantity"] = 5,
		["giType"] = IT_ARMOR,
		["giTag"] = 0,
		["prechaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_armor_combat", 
		["pickup_sound"] = "sound/misc/ar2_pkup.wav",
        ["world_model"] = { "models/powerups/armor/armor_yel.md3" },
		["icon"] = "qc/icons/iconr_yellow",
		["pickup_name"] = "Armor",
		["quantity"] = 50,
		["giType"] = IT_ARMOR,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_armor_body", 
		["pickup_sound"] = "sound/misc/ar2_pkup.wav",
        ["world_model"] = { "models/powerups/armor/armor_red.md3" },
		["icon"] = "qc/icons/iconr_red",
		["pickup_name"] = "Heavy Armor",
		["quantity"] = 100,
		["giType"] = IT_ARMOR,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

	--
	-- health
	--
--QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_health_small",
		["pickup_sound"] = "sound/items/s_health.wav",
        ["world_model"] = { "models/powerups/health/small_cross.md3", "models/powerups/health/small_sphere.md3" },
		["icon"] = "qc/icons/iconh_green",
		["pickup_name"] = "5 Health",
		["quantity"] = 5,
		["giType"] = IT_HEALTH,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_health",
		["pickup_sound"] = "sound/items/n_health.wav",
        ["world_model"] = { "models/powerups/health/medium_cross.md3", "models/powerups/health/medium_sphere.md3" },
		["icon"] = "qc/icons/iconh_blue",
		["pickup_name"] = "25 Health",
		["quantity"] = 25,
		["giType"] = IT_HEALTH,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_health_large",
		["pickup_sound"] = "sound/items/l_health.wav",
        ["world_model"] = { "models/powerups/health/large_cross.md3", "models/powerups/health/large_sphere.md3" },
		["icon"] = "qc/icons/iconh_red",
		["pickup_name"] = "50 Health",
		["quantity"] = 50,
		["giType"] = IT_HEALTH,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_health_mega",
		["pickup_sound"] = "sound/items/m_health.wav",
        ["world_model"] = { "models/powerups/health/mega_cross.md3", "models/powerups/health/mega_sphere.md3" },
		["icon"] = "qc/icons/iconh_mega",
		["pickup_name"] = "Mega Health",
		["quantity"] = 100,
		["giType"] = IT_HEALTH,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_hourglass (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_hourglass",
		["pickup_sound"] = "sound/items/s_health.wav",
        ["world_model"] = { "models/powerups/hourglass/hourglass.md3" },
		["icon"] = "qc/icons/hourglass",
		["pickup_name"] = "Hourglass",
		["quantity"] = 5,
		["giType"] = IT_HOURGLASS,
		["giTag"] = 0,
		["precaches"] = "",
		["sounds"] = ""
	},


	--
	-- WEAPONS 
	--

--QUAKED weapon_gauntlet (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_gauntlet", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/gauntlet/gauntlet.md3" },
		["icon"] = "qc/icons/iconw_gauntlet",
		["pickup_name"] = "Gauntlet",
		["quantity"] = 0,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_GAUNTLET,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_shotgun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/shotgun/shotgun.md3" },
		["icon"] = "qc/icons/iconw_shotgun",
		["pickup_name"] = "Shotgun",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_SHOTGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_machinegun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/machinegun/machinegun.md3" },
		["icon"] = "qc/icons/iconw_machinegun",
		["pickup_name"] = "Machinegun",
		["quantity"] = 100,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_MACHINEGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_grenadelauncher",
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/grenadel/grenadel.md3" },
		["icon"] = "qc/icons/iconw_grenade",
		["pickup_name"] = "Grenade Launcher",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_GRENADE_LAUNCHER,
		["precaches"] = "",
		["sounds"] = "sound/weapons/grenade/hgrenb1a.wav sound/weapons/grenade/hgrenb2a.wav"
	},

--QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_rocketlauncher",
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/rocketl/rocketl.md3" },
		["icon"] = "qc/icons/iconw_rocket",
		["pickup_name"] = "Rocket Launcher",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_ROCKET_LAUNCHER,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_lightning", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/lightning/lightning.md3" },
		["icon"] = "qc/icons/iconw_lightning",
		["pickup_name"] = "Lightning Gun",
		["quantity"] = 100,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_LIGHTNING,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_railgun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/railgun/railgun.md3" },
		["icon"] = "qc/icons/iconw_railgun",
		["pickup_name"] = "Railgun",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_RAILGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_plasmagun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/plasma/plasma.md3" },
		["icon"] = "qc/icons/iconw_plasma",
		["pickup_name"] = "Plasma Gun",
		["quantity"] = 100,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_PLASMAGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_bfg",
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/bfg/bfg.md3" },
		["icon"] = "qc/icons/iconw_bfg",
		["pickup_name"] = "BFG10K",
		["quantity"] = 20,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_BFG,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_grapplinghook (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_grapplinghook",
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons2/grapple/grapple.md3" },
		["icon"] = "qc/icons/iconw_grapple",
		["pickup_name"] = "Grappling Hook",
		["quantity"] = 0,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_GRAPPLING_HOOK,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_tribolt (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_tribolt", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        --["world_model"] = { "models/weapons2/tribolt/tribolt.md3" }, 
        ["world_model"] = { "models/weapons2/grenadel/grenadel.md3" },
		["icon"] = "qc/icons/iconw_tribolt",
		["pickup_name"] = "Tribolt",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_TRIBOLT,
		["precaches"] = "",
		["sounds"] = ""
	},
			
--QUAKED weapon_lousy_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_lousy_shotgun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons3/shotgun/shotgun.md3" },
		["icon"] = "qc/icons/iconw_shotgun",
		["pickup_name"] = "Lousy Shotgun",
		["quantity"] = 10,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_LOUSY_SHOTGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_lousy_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_lousy_machinegun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons3/machinegun/machinegun.md3" },
		["icon"] = "qc/icons/iconw_machinegun",
		["pickup_name"] = "Lousy Machinegun",
		["quantity"] = 100,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_LOUSY_MACHINEGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED weapon_lousy_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "weapon_lousy_plasmagun", 
		["pickup_sound"] = "sound/misc/w_pkup.wav",
        ["world_model"] = { "models/weapons3/plasma/plasma.md3" },
		["icon"] = "icons/iconw_plasma",
		["pickup_name"] = "Lousy Plasma Gun",
		["quantity"] = 100,
		["giType"] = IT_WEAPON,
		["giTag"] = WP_LOUSY_PLASMAGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

	--
	-- AMMO ITEMS
	--

--QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_shells",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/shotgunam.md3" },
		["icon"] = "qc/icons/icona_shotgun",
		["pickup_name"] = "Shells",
		["quantity"] = 5,
		["giType"] = IT_AMMO,
		["giTag"] = WP_SHOTGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_bullets",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/machinegunam.md3" },
		["icon"] = "qc/icons/icona_machinegun",
		["pickup_name"] = "Bullets",
		["quantity"] = 50,
		["giType"] = IT_AMMO,
		["giTag"] = WP_MACHINEGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_grenades",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/grenadeam.md3" },
		["icon"] = "qc/icons/icona_grenade",
		["pickup_name"] = "Grenades",
		["quantity"] = 5,
		["giType"] = IT_AMMO,
		["giTag"] = WP_GRENADE_LAUNCHER,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_cells",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/plasmaam.md3" },
		["icon"] = "qc/icons/icona_plasma",
		["pickup_name"] = "Cells",
		["quantity"] = 50,
		["giType"] = IT_AMMO,
		["giTag"] = WP_PLASMAGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_lightning",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/lightningam.md3" },
		["icon"] = "qc/icons/icona_lightning",
		["pickup_name"] = "Lightning",
		["quantity"] = 50,
		["giType"] = IT_AMMO,
		["giTag"] = WP_LIGHTNING,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_rockets",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/rocketam.md3" },
		["icon"] = "qc/icons/icona_rocket",
		["pickup_name"] = "Rockets",
		["quantity"] = 5,
		["giType"] = IT_AMMO,
		["giTag"] = WP_ROCKET_LAUNCHER,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_bolts (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_bolts",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/triboltam.md3" },
		["icon"] = "qc/icons/icona_tribolt",
		["pickup_name"] = "Bolts",
		["quantity"] = 5,
		["giType"] = IT_AMMO,
		["giTag"] = WP_TRIBOLT,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_slugs",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/railgunam.md3" },
		["icon"] = "qc/icons/icona_railgun",
		["pickup_name"] = "Slugs",
		["quantity"] = 5,
		["giType"] = IT_AMMO,
		["giTag"] = WP_RAILGUN,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED ammo_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo_bfg",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/bfgam.md3" },
		["icon"] = "qc/icons/icona_bfg",
		["pickup_name"] = "Bfg Ammo",
		["quantity"] = 15,
		["giType"] = IT_AMMO,
		["giTag"] = WP_BFG,
		["precaches"] = "",
		["sounds"] = ""
	},

-- universal ammo pack
--QUAKED ammo (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "ammo",
		["pickup_sound"] = "sound/misc/am_pkup.wav",
        ["world_model"] = { "models/powerups/ammo/uniam.md3" },
		["icon"] = "qc/icons/icona_uni",
		["pickup_name"] = "Ammo",
		["quantity"] = 15,
		["giType"] = IT_AMMO,
		["giTag"] = WP_NONE,
		["precaches"] = "",
		["sounds"] = ""
	},

	--
	-- HOLDABLE ITEMS
	--
--QUAKED holdable_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "holdable_teleporter", 
		["pickup_sound"] = "sound/items/holdable.wav",
        ["world_model"] = { "models/powerups/holdable/teleporter.md3" },
		["icon"] = "icons/teleporter",
		["pickup_name"] = "Personal Teleporter",
		["quantity"] = 60,
		["giType"] = IT_HOLDABLE,
		["giTag"] = HI_TELEPORTER,
		["precaches"] = "",
		["sounds"] = ""
	},
--QUAKED holdable_medkit (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "holdable_medkit", 
		["pickup_sound"] = "sound/items/holdable.wav",
        ["world_model"] = { "models/powerups/holdable/medkit.md3", "models/powerups/holdable/medkit_sphere.md3" },
		["icon"] = "icons/medkit",
		["pickup_name"] = "Medkit",
		["quantity"] = 60,
		["giType"] = IT_HOLDABLE,
		["giTag"] = HI_MEDKIT,
		["precaches"] = "",
		["sounds"] = "sound/items/use_medkit.wav"
	},

	--
	-- POWERUP ITEMS
	--
--QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_quad", 
		["pickup_sound"] = "sound/items/quaddamage.wav",
        ["world_model"] = { "models/powerups/instant/quad.md3", "models/powerups/instant/quad_ring.md3" },
		["icon"] = "qc/icons/quad",
		["pickup_name"] = "Quad Damage",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_QUAD,
		["precaches"] = "",
		["sounds"] = "sound/items/damage2.wav sound/items/damage3.wav"
	},

--QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_enviro",
		["pickup_sound"] = "sound/items/protect.wav",
        ["world_model"] = { "models/powerups/instant/enviro.md3", "models/powerups/instant/enviro_ring.md3" },
		["icon"] = "qc/icons/envirosuit",
		["pickup_name"] = "Battle Suit",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_BATTLESUIT,
		["precaches"] = "",
		["sounds"] = "sound/items/airout.wav sound/items/protect3.wav"
	},

--QUAKED item_protection (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_protection",
		["pickup_sound"] = "sound/items/protect.wav",
        ["world_model"] = { "models/powerups/instant/protection.md3", "models/powerups/instant/protection_ring.md3" },
		["icon"] = "qc/icons/protection",
		["pickup_name"] = "Protection",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_PROTECTION,
		["precaches"] = "",
		["sounds"] = "sound/items/airout.wav sound/items/protect3.wav"
	},

--QUAKED item_haste (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_haste",
		["pickup_sound"] = "sound/items/haste.wav",
        ["world_model"] = { "models/powerups/instant/haste.md3", "models/powerups/instant/haste_ring.md3" },
		["icon"] = "qc/icons/haste",
		["pickup_name"] = "Speed",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_HASTE,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_invis (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_invis",
		["pickup_sound"] = "sound/items/invisibility.wav",
        ["world_model"] = { "models/powerups/instant/invis.md3", "models/powerups/instant/invis_ring.md3" },
		["icon"] = "qc/icons/invis",
		["pickup_name"] = "Invisibility",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_INVIS,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_regen",
		["pickup_sound"] = "sound/items/regeneration.wav",
        ["world_model"] = { "models/powerups/instant/regen.md3", "models/powerups/instant/regen_ring.md3" },
		["icon"] = "qc/icons/regen",
		["pickup_name"] = "Regeneration",
		["quantity"] = 30,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_REGEN,
		["precaches"] = "",
		["sounds"] = "sound/items/regen.wav"
	},

--QUAKED item_flight (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	{
		["classname"] = "item_flight",
		["pickup_sound"] = "sound/items/flight.wav",
        ["world_model"] = { "models/powerups/instant/flight.md3", "models/powerups/instant/flight_ring.md3" },
		["icon"] = "qc/icons/flight",
		["pickup_name"] = "Flight",
		["quantity"] = 60,
		["giType"] = IT_POWERUP,
		["giTag"] = PW_FLIGHT,
		["precaches"] = "",
		["sounds"] = "sound/items/flight.wav"
	},

--QUAKED team_CTF_redflag (1 0 0) (-16 -16 -16) (16 16 16)
	{
		["classname"] = "team_CTF_redflag",
		["pickup_sound"] = NULL,
        ["world_model"] = { "models/flags/r_flag.md3" },
		["icon"] = "icons/iconf_red1",
		["pickup_name"] = "Red Flag",
		["quantity"] = 0,
		["giType"] = IT_TEAM,
		["giTag"] = PW_REDFLAG,
		["precaches"] = "",
		["sounds"] = ""
	},

--QUAKED team_CTF_blueflag (0 0 1) (-16 -16 -16) (16 16 16)
	{
		["classname"] = "team_CTF_blueflag",
		["pickup_sound"] = NULL,
        ["world_model"] = { "models/flags/b_flag.md3" },
		["icon"] = "icons/iconf_blu1",
		["pickup_name"] = "Blue Flag",
		["quantity"] = 0,
		["giType"] = IT_TEAM,
		["giTag"] = PW_BLUEFLAG,
		["precaches"] = "",
		["sounds"] = ""
	}
}
