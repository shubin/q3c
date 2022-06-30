/* borrowed from q3now (https://github.com/eserozvataf/q3now) under the terms of the GPL v2 */

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_promode.h"

float   cpm_pm_jump_z;
int     pm_walljumps;

// Physics
#if !defined( QC )
float	cpm_pm_airstopaccelerate = 1;
float	cpm_pm_aircontrol = 0;
float	cpm_pm_strafeaccelerate = 1;
float	cpm_pm_wishspeed = 400;
#endif

// Weapon switching
float	cpm_weapondrop = 0; // 200;
float	cpm_weaponraise = 250;
float	cpm_outofammodelay = 500;

// Battle Suit
float	cpm_BSprotection = 0.5f;

// Backpacks
int		cpm_backpacks = 0;

// Radius Damage Fix
int		cpm_radiusdamagefix = 0;

// Z Knockback
float	cpm_knockback_z = 24;

// Respawn Times
int		cpm_itemrespawnhealth = 35;
int		cpm_itemrespawnpowerup = 120;
int		cpm_itemrespawnweapon = 5;
int		cpm_itemrespawnammo = 40;
int		cpm_startpowerups = 0;
int		cpm_itemrespawnBS = 120;
int     cpm_playerforcerespawn = 20;

// Megahealth
int		cpm_megastyle = 0;

// Respawn delay
float	cpm_clientrespawndelay = 1700;

// Hit tones
int		cpm_hittones = 0;

// Item size
int		cpm_itemsize = 36;

// Lava damage
float	cpm_lavadamage = 30;
float	cpm_slimedamage = 10;
float	cpm_lavafrequency = 700;

void CPM_UpdateSettings(int gametype, int pro_mode, int pro_physics)
{
	// num = 0: normal quake 3
	// num = 1: pro mode

	cpm_pm_jump_z = 0; // turn off double-jump in vq3
    pm_walljumps = 0;

	// Physics
#if !defined( QC )
	cpm_pm_airstopaccelerate = 1;
	cpm_pm_aircontrol = 0;
	cpm_pm_strafeaccelerate = 1;
	cpm_pm_wishspeed = 400;
	pm_accelerate = 10;
	pm_friction = 6;
#endif

	// vq3 Weapon switching
    cpm_weapondrop = 0; // 200;
	cpm_weaponraise = 250;
	cpm_outofammodelay = 500;

	// vq3 Battle Suit
	cpm_BSprotection = 0.5;

	// Backpacks
	cpm_backpacks = 0;

	// Radius Damage Fix
	cpm_radiusdamagefix = 0;

	// Z Knockback
	cpm_knockback_z = 24;

	// Respawn Times
	cpm_itemrespawnhealth = 35;
	cpm_itemrespawnpowerup = 120;
    cpm_itemrespawnweapon = 5;
	cpm_itemrespawnammo = 40;
	cpm_startpowerups = 0;
	cpm_itemrespawnBS = 120;
	cpm_playerforcerespawn = 20;

	// Megahealth
	cpm_megastyle = 0;

	// Respawn delay
	cpm_clientrespawndelay = 1700;

	// Hit tones
	cpm_hittones = 0;

	// Item size
	cpm_itemsize = 36;

	// Lava damage
	cpm_lavadamage = 30;
	cpm_slimedamage = 10;
	cpm_lavafrequency = 700;

    if (pro_physics)
    {
        cpm_pm_jump_z = 100; // enable double-jump
        pm_walljumps = 1;

        // Physics
#if !defined( QC )
        cpm_pm_airstopaccelerate = 2.5f;
        cpm_pm_aircontrol = 150;
        cpm_pm_strafeaccelerate = 70;
        cpm_pm_wishspeed = 30;
        pm_accelerate = 15;
        pm_friction = 8;
#endif
    }

	if (pro_mode)
	{
		// Weapon switching
		cpm_weapondrop = 0;
		cpm_weaponraise = 0;
		cpm_outofammodelay = 100;

		// Battle Suit
		cpm_BSprotection = 0.25; // ie 75% protection

		// Backpacks
		cpm_backpacks = 0;

		// Radius Damage Fix
		cpm_radiusdamagefix = 1;

		// Z Knockback
		cpm_knockback_z = 40;

		// Respawn Times
		cpm_itemrespawnhealth = 30;
		cpm_itemrespawnpowerup = 60;
        cpm_itemrespawnweapon = 15;
		cpm_itemrespawnammo = 30;
		cpm_startpowerups = 1;
		cpm_itemrespawnBS = 120;
		cpm_playerforcerespawn = 3;

		// Megahealth
		cpm_megastyle = 1;

		// Respawn delay
		cpm_clientrespawndelay = 500;

		// Hit tones
		cpm_hittones = 1;

		// Item size
		cpm_itemsize = 66; // easier to get items in pro mode

		// Lava damage
		cpm_lavadamage = 4;
		cpm_slimedamage = 1.3f;
		cpm_lavafrequency = 100;
	}
}

void CPM_PM_Aircontrol( movement_parameters_t *mp, pmove_t *pm, vec3_t wishdir, float wishspeed)
{
    float	zspeed, speed, dot, k;
    int		i;

    if ((pm->ps->movementDir && pm->ps->movementDir != 4) || wishspeed == 0.0f)
        return; // can't control movement if not moveing forward or backward

    zspeed = pm->ps->velocity[2];
    pm->ps->velocity[2] = 0;
    speed = VectorNormalize(pm->ps->velocity);

    dot = DotProduct(pm->ps->velocity, wishdir);
    k = 32;
    k *= mp->cpm_pm_aircontrol*dot*dot*pml.frametime;


    if (dot > 0) {	// we can't change direction while slowing down
        for (i = 0; i < 2; i++)
            pm->ps->velocity[i] = pm->ps->velocity[i] * speed + wishdir[i] * k;
        VectorNormalize(pm->ps->velocity);
    }

    for (i = 0; i < 2; i++)
        pm->ps->velocity[i] *= speed;

    pm->ps->velocity[2] = zspeed;
}
