/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		be_aas_route.c
 *
 * desc:		AAS
 *
 * $Archive: /MissionPack/code/botlib/be_aas_route.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_crc.h"
#include "l_libvar.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

#define ROUTING_DEBUG

//travel time in hundreths of a second = distance * 100 / speed
#define DISTANCEFACTOR_CROUCH		1.3f		//crouch speed = 100
#define DISTANCEFACTOR_SWIM			1		//should be 0.66, swim speed = 150
#define DISTANCEFACTOR_WALK			0.33f	//walk speed = 300

//cache refresh time
#define CACHE_REFRESHTIME		15.0f	//15 seconds refresh time

//maximum number of routing updates each frame
#define MAX_FRAMEROUTINGUPDATES		10


/*

  area routing cache:
  stores the distances within one cluster to a specific goal area
  this goal area is in this same cluster and could be a cluster portal
  for every cluster there's a list with routing cache for every area
  in that cluster (including the portals of that cluster)
  area cache stores aasworld.clusters[?].numreachabilityareas travel times

  portal routing cache:
  stores the distances of all portals to a specific goal area
  this goal area could be in any cluster and could also be a cluster portal
  for every area (aasworld.numareas) the portal cache stores
  aasworld.numportals travel times

*/

#ifdef ROUTING_DEBUG
static int numareacacheupdates(0);
static int numportalcacheupdates(0);
#endif //ROUTING_DEBUG

static size_t routingcachesize(0);
static size_t max_routingcachesize(4096 * 1024);

//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#ifdef ROUTING_DEBUG
void AAS_RoutingInfo(void)
{
	botimport.Print(PRT_MESSAGE, "%d area cache updates\n", numareacacheupdates);
	botimport.Print(PRT_MESSAGE, "%d portal cache updates\n", numportalcacheupdates);
	botimport.Print(PRT_MESSAGE, "%d bytes routing cache\n", routingcachesize);
} //end of the function AAS_RoutingInfo
#endif //ROUTING_DEBUG
//===========================================================================
// returns the number of the area in the cluster
// assumes the given area is in the given cluster or a portal of the cluster
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
ID_INLINE int AAS_ClusterAreaNum(int cluster, int areanum)
{
	int areacluster;

	areacluster = aasworld.areasettings[areanum].cluster;
	if (areacluster > 0) return aasworld.areasettings[areanum].clusterareanum;
	else
	{
/*#ifdef ROUTING_DEBUG
		if (aasworld.portals[-areacluster].frontcluster != cluster &&
				aasworld.portals[-areacluster].backcluster != cluster)
		{
			botimport.Print(PRT_ERROR, "portal %d: does not belong to cluster %d\n"
											, -areacluster, cluster);
		} //end if
#endif //ROUTING_DEBUG*/
		int side = aasworld.portals[-areacluster].frontcluster != cluster;
		return aasworld.portals[-areacluster].clusterareanum[side];
	} //end else
} //end of the function AAS_ClusterAreaNum
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_InitTravelFlagFromType(void)
{
	for (int i = 0; i < MAX_TRAVELTYPES; i++)
	{
		aasworld.travelflagfortype[i] = TFL_INVALID;
	} //end for
	aasworld.travelflagfortype[TRAVEL_INVALID] = TFL_INVALID;
	aasworld.travelflagfortype[TRAVEL_WALK] = TFL_WALK;
	aasworld.travelflagfortype[TRAVEL_CROUCH] = TFL_CROUCH;
	aasworld.travelflagfortype[TRAVEL_BARRIERJUMP] = TFL_BARRIERJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMP] = TFL_JUMP;
	aasworld.travelflagfortype[TRAVEL_LADDER] = TFL_LADDER;
	aasworld.travelflagfortype[TRAVEL_WALKOFFLEDGE] = TFL_WALKOFFLEDGE;
	aasworld.travelflagfortype[TRAVEL_SWIM] = TFL_SWIM;
	aasworld.travelflagfortype[TRAVEL_WATERJUMP] = TFL_WATERJUMP;
	aasworld.travelflagfortype[TRAVEL_TELEPORT] = TFL_TELEPORT;
	aasworld.travelflagfortype[TRAVEL_ELEVATOR] = TFL_ELEVATOR;
	aasworld.travelflagfortype[TRAVEL_ROCKETJUMP] = TFL_ROCKETJUMP;
	aasworld.travelflagfortype[TRAVEL_BFGJUMP] = TFL_BFGJUMP;
	aasworld.travelflagfortype[TRAVEL_GRAPPLEHOOK] = TFL_GRAPPLEHOOK;
	aasworld.travelflagfortype[TRAVEL_DOUBLEJUMP] = TFL_DOUBLEJUMP;
	aasworld.travelflagfortype[TRAVEL_RAMPJUMP] = TFL_RAMPJUMP;
	aasworld.travelflagfortype[TRAVEL_STRAFEJUMP] = TFL_STRAFEJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMPPAD] = TFL_JUMPPAD;
	aasworld.travelflagfortype[TRAVEL_FUNCBOB] = TFL_FUNCBOB;
} //end of the function AAS_InitTravelFlagFromType
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
ID_INLINE int AAS_TravelFlagForType_inline(int traveltype)
{
	traveltype &= TRAVELTYPE_MASK;
	if (traveltype < 0 || traveltype >= MAX_TRAVELTYPES)
		return TFL_INVALID;

	int tfl = 0;
	if (traveltype & TRAVELFLAG_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (traveltype & TRAVELFLAG_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	tfl |= aasworld.travelflagfortype[traveltype];
	return tfl;
} //end of the function AAS_TravelFlagForType_inline
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_TravelFlagForType(int traveltype)
{
	return AAS_TravelFlagForType_inline(traveltype);
} //end of the function AAS_TravelFlagForType_inline
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_UnlinkCache( aas_routingcache_t* const cache )
{
	if (cache->time_next) cache->time_next->time_prev = cache->time_prev;
	else aasworld.newestcache = cache->time_prev;
	if (cache->time_prev) cache->time_prev->time_next = cache->time_next;
	else aasworld.oldestcache = cache->time_next;
	cache->time_next = NULL;
	cache->time_prev = NULL;
} //end of the function AAS_UnlinkCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_LinkCache(aas_routingcache_t* const cache)
{
	if (aasworld.newestcache)
	{
		aasworld.newestcache->time_next = cache;
		cache->time_prev = aasworld.newestcache;
	} //end if
	else
	{
		aasworld.oldestcache = cache;
		cache->time_prev = NULL;
	} //end else
	cache->time_next = NULL;
	aasworld.newestcache = cache;
} //end of the function AAS_LinkCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeRoutingCache(aas_routingcache_t *cache)
{
	AAS_UnlinkCache(cache);
	routingcachesize -= cache->size;
	FreeMemory(cache);
} //end of the function AAS_FreeRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_RemoveRoutingCacheInCluster( int clusternum )
{
	if (!aasworld.clusterareacache)
		return;

	aas_cluster_t* const cluster = &aasworld.clusters[clusternum];
	for (int i = 0; i < cluster->numareas; i++)
	{
		for (aas_routingcache_t *nextcache, *cache = aasworld.clusterareacache[clusternum][i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.clusterareacache[clusternum][i] = NULL;
	} //end for
} //end of the function AAS_RemoveRoutingCacheInCluster
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_RemoveRoutingCacheUsingArea( int areanum )
{
	int clusternum = aasworld.areasettings[areanum].cluster;
	if (clusternum > 0)
	{
		//remove all the cache in the cluster the area is in
		AAS_RemoveRoutingCacheInCluster( clusternum );
	} //end if
	else
	{
		// if this is a portal remove all cache in both the front and back cluster
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].frontcluster );
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].backcluster );
	} //end else
	// remove all portal cache
	for (int i = 0; i < aasworld.numareas; i++)
	{
		//refresh portal cache
		for (aas_routingcache_t *nextcache, *cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.portalcache[i] = NULL;
	} //end for
} //end of the function AAS_RemoveRoutingCacheUsingArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_EnableRoutingArea(int areanum, int enable)
{
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (bot_developer)
		{
			botimport.Print(PRT_ERROR, "AAS_EnableRoutingArea: areanum %d out of range\n", areanum);
		} //end if
		return 0;
	} //end if
	int flags = aasworld.areasettings[areanum].areaflags & AREA_DISABLED;
	if (enable < 0)
		return !flags;

	if (enable)
		aasworld.areasettings[areanum].areaflags &= ~AREA_DISABLED;
	else
		aasworld.areasettings[areanum].areaflags |= AREA_DISABLED;
	// if the status of the area changed
	if ( (flags & AREA_DISABLED) != (aasworld.areasettings[areanum].areaflags & AREA_DISABLED) )
	{
		//remove all routing cache involving this area
		AAS_RemoveRoutingCacheUsingArea( areanum );
	} //end if
	return !flags;
} //end of the function AAS_EnableRoutingArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
ID_INLINE float AAS_RoutingTime(void)
{
	return AAS_Time();
} //end of the function AAS_RoutingTime
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_GetAreaContentsTravelFlags(int areanum)
{
	int contents = aasworld.areasettings[areanum].contents;
	int tfl = 0;
	if (contents & AREACONTENTS_WATER)
		tfl |= TFL_WATER;
	else if (contents & AREACONTENTS_SLIME)
		tfl |= TFL_SLIME;
	else if (contents & AREACONTENTS_LAVA)
		tfl |= TFL_LAVA;
	else
		tfl |= TFL_AIR;
	if (contents & AREACONTENTS_DONOTENTER)
		tfl |= TFL_DONOTENTER;
	if (contents & AREACONTENTS_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (contents & AREACONTENTS_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	if (aasworld.areasettings[areanum].areaflags & AREA_BRIDGE)
		tfl |= TFL_BRIDGE;
	return tfl;
} //end of the function AAS_GetAreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
ID_INLINE int AAS_AreaContentsTravelFlags_inline(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} //end of the function AAS_AreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaContentsTravelFlags(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} //end of the function AAS_AreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_InitAreaContentsTravelFlags(void)
{
	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = (int *) GetClearedMemory(aasworld.numareas * sizeof(int));
	//
	for (int i = 0; i < aasworld.numareas; i++) {
		aasworld.areacontentstravelflags[i] = AAS_GetAreaContentsTravelFlags(i);
	}
} //end of the function AAS_InitAreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_CreateReversedReachability()
{
#ifdef DEBUG
	int starttime = BL_MilliSeconds();
#endif
	//free reversed links that have already been created
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	//allocate memory for the reversed reachability links
	char* ptr = (char* ) GetClearedMemory(aasworld.numareas * sizeof(aas_reversedreachability_t) +
										 aasworld.reachabilitysize * sizeof(aas_reversedlink_t));
	//
	aasworld.reversedreachability = (aas_reversedreachability_t *) ptr;
	//pointer to the memory for the reversed links
	ptr += aasworld.numareas * sizeof(aas_reversedreachability_t);
	//check all reachabilities of all areas
	for (int i = 1; i < aasworld.numareas; i++)
	{
		//settings of the area
		const aas_areasettings_t& settings = aasworld.areasettings[i];
		//
		if (settings.numreachableareas >= 128)
			botimport.Print(PRT_WARNING, "area %d has more than 128 reachabilities\n", i);
		//create reversed links for the reachabilities
		for (int n = 0; n < settings.numreachableareas && n < 128; n++)
		{
			//reachability link
			const aas_reachability_t& reach = aasworld.reachability[settings.firstreachablearea + n];
			//
			aas_reversedlink_t* const revlink = (aas_reversedlink_t* const) ptr;
			ptr += sizeof(aas_reversedlink_t);
			//
			revlink->areanum = i;
			revlink->linknum = settings.firstreachablearea + n;
			revlink->next = aasworld.reversedreachability[reach.areanum].first;
			aasworld.reversedreachability[reach.areanum].first = revlink;
			aasworld.reversedreachability[reach.areanum].numlinks++;
		} //end for
	} //end for
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "reversed reachability %d msec\n", BL_MilliSeconds() - starttime);
#endif
} //end of the function AAS_CreateReversedReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
unsigned short int AAS_AreaTravelTime(int areanum, const vec3_t start, const vec3_t end)
{
	vec3_t dir;

	VectorSubtract(start, end, dir);
	float dist = VectorLength(dir);
	//if crouch only area
	if (AAS_AreaCrouch(areanum)) dist *= DISTANCEFACTOR_CROUCH;
	//if swim area
	else if (AAS_AreaSwim(areanum)) dist *= DISTANCEFACTOR_SWIM;
	//normal walk area
	else dist *= DISTANCEFACTOR_WALK;
	//
	return dist <= 0.f ? 1 : (int)dist;
} //end of the function AAS_AreaTravelTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_CalculateAreaTravelTimes()
{

	//if there are still area travel times, free the memory
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	//get the total size of all the area travel times
	size_t size = aasworld.numareas * sizeof(unsigned short **);
	for (int i = 0; i < aasworld.numareas; i++)
	{
		const aas_reversedreachability_t& revreach = aasworld.reversedreachability[i];
		//settings of the area
		const aas_areasettings_t& settings = aasworld.areasettings[i];
		//
		size += settings.numreachableareas * sizeof(unsigned short *);
		//
		size += settings.numreachableareas *
			PAD(revreach.numlinks, sizeof(long)) * sizeof(unsigned short);
	} //end for
	//allocate memory for the area travel times
	char *ptr = (char *) GetClearedMemory(size);
	aasworld.areatraveltimes = (unsigned short ***) ptr;
	ptr += aasworld.numareas * sizeof(unsigned short **);
	//calcluate the travel times for all the areas
	for (int i = 0; i < aasworld.numareas; i++)
	{
		//reversed reachabilities of this area
		const aas_reversedreachability_t& revreach = aasworld.reversedreachability[i];
		//settings of the area
		const aas_areasettings_t& settings = aasworld.areasettings[i];
		//
		aasworld.areatraveltimes[i] = (unsigned short **) ptr;
		ptr += settings.numreachableareas * sizeof(unsigned short *);
		//
		for (int l = 0; l < settings.numreachableareas; l++)
		{
			aasworld.areatraveltimes[i][l] = (unsigned short *) ptr;
			ptr += PAD(revreach.numlinks, sizeof(long)) * sizeof(unsigned short);
			//reachability link
			const aas_reachability_t& reach = aasworld.reachability[settings.firstreachablearea + l];
			//
			int n=0;
			for (aas_reversedlink_t * revlink = revreach.first; revlink; revlink = revlink->next, n++)
			{
				vec3_t end;
				VectorCopy(aasworld.reachability[revlink->linknum].end, end);
				//
				aasworld.areatraveltimes[i][l][n] = AAS_AreaTravelTime(i, end, reach.start);
			} //end for
		} //end for
	} //end for
} //end of the function AAS_CalculateAreaTravelTimes
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_PortalMaxTravelTime(int portalnum)
{
	const aas_portal_t& portal = aasworld.portals[portalnum];
	//reversed reachabilities of this portal area
	const aas_reversedreachability_t& revreach = aasworld.reversedreachability[portal.areanum];
	//settings of the portal area
	const aas_areasettings_t& settings = aasworld.areasettings[portal.areanum];
	//
	unsigned short maxt = 0;
	for (int n,l = 0; l < settings.numreachableareas; l++)
	{
		n = 0;
		for (aas_reversedlink_t *revlink = revreach.first; revlink; revlink = revlink->next, n++)
		{
			unsigned short t = aasworld.areatraveltimes[portal.areanum][l][n];
			if (t > maxt)
			{
				maxt = t;
			} //end if
		} //end for
	} //end for
	return maxt;
} //end of the function AAS_PortalMaxTravelTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_InitPortalMaxTravelTimes()
{
	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);

	aasworld.portalmaxtraveltimes = (int *) GetClearedMemory(aasworld.numportals * sizeof(int));

	for (int i = 0; i < aasworld.numportals; i++)
	{
		aasworld.portalmaxtraveltimes[i] = AAS_PortalMaxTravelTime(i);
		//botimport.Print(PRT_MESSAGE, "portal %d max tt = %d\n", i, aasworld.portalmaxtraveltimes[i]);
	} //end for
} //end of the function AAS_InitPortalMaxTravelTimes
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
/*
int AAS_FreeOldestCache(void)
{
	int i, j, bestcluster, bestarea, freed;
	float besttime;
	aas_routingcache_t *cache, *bestcache;

	freed = qfalse;
	besttime = 999999999;
	bestcache = NULL;
	bestcluster = 0;
	bestarea = 0;
	//refresh cluster cache
	for (i = 0; i < aasworld.numclusters; i++)
	{
		for (j = 0; j < aasworld.clusters[i].numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				//never remove cache leading towards a portal
				if (aasworld.areasettings[cache->areanum].cluster < 0) continue;
				//if this cache is older than the cache we found so far
				if (cache->time < besttime)
				{
					bestcache = cache;
					bestcluster = i;
					bestarea = j;
					besttime = cache->time;
				} //end if
			} //end for
		} //end for
	} //end for
	if (bestcache)
	{
		cache = bestcache;
		if (cache->prev) cache->prev->next = cache->next;
		else aasworld.clusterareacache[bestcluster][bestarea] = cache->next;
		if (cache->next) cache->next->prev = cache->prev;
		AAS_FreeRoutingCache(cache);
		freed = qtrue;
	} //end if
	besttime = 999999999;
	bestcache = NULL;
	bestarea = 0;
	for (i = 0; i < aasworld.numareas; i++)
	{
		//refresh portal cache
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			if (cache->time < besttime)
			{
				bestcache = cache;
				bestarea = i;
				besttime = cache->time;
			} //end if
		} //end for
	} //end for
	if (bestcache)
	{
		cache = bestcache;
		if (cache->prev) cache->prev->next = cache->next;
		else aasworld.portalcache[bestarea] = cache->next;
		if (cache->next) cache->next->prev = cache->prev;
		AAS_FreeRoutingCache(cache);
		freed = qtrue;
	} //end if
	return freed;
} //end of the function AAS_FreeOldestCache
*/
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_FreeOldestCache()
{
	aas_routingcache_t *cache;

	for (cache = aasworld.oldestcache; cache; cache = cache->time_next) {
		// never free area cache leading towards a portal
		if (cache->type == CACHETYPE_AREA && aasworld.areasettings[cache->areanum].cluster < 0) {
			continue;
		}
		break;
	}
	if (cache) {
		// unlink the cache
		if (cache->type == CACHETYPE_AREA) {
			//number of the area in the cluster
			int clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
			// unlink from cluster area cache
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.clusterareacache[cache->cluster][clusterareanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		else {
			// unlink from portal cache
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.portalcache[cache->areanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		AAS_FreeRoutingCache(cache);
		return qtrue;
	}
	return qfalse;
} //end of the function AAS_FreeOldestCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
aas_routingcache_t *AAS_AllocRoutingCache(size_t numtraveltimes)
{
	aas_routingcache_t *cache;

	//
	size_t size = sizeof(aas_routingcache_t)
						+ numtraveltimes * sizeof(unsigned short int)
						+ numtraveltimes * sizeof(unsigned char);
	//
	routingcachesize += size;
	//
	cache = (aas_routingcache_t *) GetClearedMemory(size);
	cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t)
								+ numtraveltimes * sizeof(unsigned short int);
	cache->size = size;
	return cache;
} //end of the function AAS_AllocRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeAllClusterAreaCache()
{
	//free all cluster cache if existing
	if (!aasworld.clusterareacache) return;
	//free caches
	for (int i = 0; i < aasworld.numclusters; i++)
	{
		const aas_cluster_t& cluster = aasworld.clusters[i];
		for (int j = 0; j < cluster.numareas; j++)
		{
			for (aas_routingcache_t *nextcache, *cache = aasworld.clusterareacache[i][j]; cache; cache = nextcache)
			{
				nextcache = cache->next;
				AAS_FreeRoutingCache(cache);
			} //end for
			aasworld.clusterareacache[i][j] = NULL;
		} //end for
	} //end for
	//free the cluster cache array
	FreeMemory(aasworld.clusterareacache);
	aasworld.clusterareacache = NULL;
} //end of the function AAS_FreeAllClusterAreaCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_InitClusterAreaCache()
{
	size_t i, size;
	for (size = 0, i = 0; i < aasworld.numclusters; i++)
	{
		size += aasworld.clusters[i].numareas;
	} //end for
	//two dimensional array with pointers for every cluster to routing cache
	//for every area in that cluster
	char *ptr = (char *) GetClearedMemory(
				aasworld.numclusters * sizeof(aas_routingcache_t **) +
				size * sizeof(aas_routingcache_t *));
	aasworld.clusterareacache = (aas_routingcache_t ***) ptr;
	ptr += aasworld.numclusters * sizeof(aas_routingcache_t **);
	for (i = 0; i < aasworld.numclusters; i++)
	{
		aasworld.clusterareacache[i] = (aas_routingcache_t **) ptr;
		ptr += aasworld.clusters[i].numareas * sizeof(aas_routingcache_t *);
	} //end for
} //end of the function AAS_InitClusterAreaCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeAllPortalCache()
{

	//free all portal cache if existing
	if (!aasworld.portalcache) return;
	//free portal caches
	for (size_t i = 0; i < aasworld.numareas; i++)
	{
		for (aas_routingcache_t *nextcache, *cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.portalcache[i] = NULL;
	} //end for
	FreeMemory(aasworld.portalcache);
	aasworld.portalcache = NULL;
} //end of the function AAS_FreeAllPortalCache
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_InitPortalCache()
{
	//
	aasworld.portalcache = (aas_routingcache_t **) GetClearedMemory(
								aasworld.numareas * sizeof(aas_routingcache_t *));
} //end of the function AAS_InitPortalCache
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_InitRoutingUpdate()
{
	//free routing update fields if already existing
	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	//
	size_t maxreachabilityareas = 0;
	for (size_t i = 0; i < aasworld.numclusters; i++)
	{
		if (aasworld.clusters[i].numreachabilityareas > maxreachabilityareas)
		{
			maxreachabilityareas = aasworld.clusters[i].numreachabilityareas;
		} //end if
	} //end for
	//allocate memory for the routing update fields
	aasworld.areaupdate = (aas_routingupdate_t *) GetClearedMemory(
									maxreachabilityareas * sizeof(aas_routingupdate_t));
	//
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	//allocate memory for the portal update fields
	aasworld.portalupdate = (aas_routingupdate_t *) GetClearedMemory(
									(aasworld.numportals+1) * sizeof(aas_routingupdate_t));
} //end of the function AAS_InitRoutingUpdate
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_CreateAllRoutingCache()
{
	aasworld.initialized = qtrue;
	botimport.Print(PRT_MESSAGE, "AAS_CreateAllRoutingCache\n");
	for (size_t i = 1; i < aasworld.numareas; i++)
	{
		if (!AAS_AreaReachability(i)) continue;
		for (size_t j = 1; j < aasworld.numareas; j++)
		{
			if (i == j) continue;
			if (!AAS_AreaReachability(j)) continue;
			AAS_AreaTravelTimeToGoalArea(i, aasworld.areas[i].center, j, TFL_DEFAULT);
		} //end for
	} //end for
	aasworld.initialized = qfalse;
} //end of the function AAS_CreateAllRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================

//the route cache header
//this header is followed by numportalcache + numareacache aas_routingcache_t
//structures that store routing cache
typedef struct routecacheheader_s
{
	int ident;
	int version;
	int numareas;
	int numclusters;
	int areacrc;
	int clustercrc;
	int numportalcache;
	int numareacache;
} routecacheheader_t;

#define RCID						(('C'<<24)+('R'<<16)+('E'<<8)+'M')
#define RCVERSION					2

//void AAS_DecompressVis(byte *in, int numareas, byte *decompressed);
//int AAS_CompressVis(byte *vis, int numareas, byte *dest);

void AAS_WriteRouteCache()
{
	size_t numportalcache = 0;
	for (size_t i = 0; i < aasworld.numareas; i++)
	{
		for (aas_routingcache_t *cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			numportalcache++;
		} //end for
	} //end for
	size_t  numareacache = 0;
	for (size_t i = 0; i < aasworld.numclusters; i++)
	{
		const aas_cluster_t& cluster = aasworld.clusters[i];
		for (size_t j = 0; j < cluster.numareas; j++)
		{
			for (aas_routingcache_t *cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				numareacache++;
			} //end for
		} //end for
	} //end for

	// open the file for writing
	char filename[MAX_QPATH];
	fileHandle_t fp;
	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	botimport.FS_FOpenFile( filename, &fp, FS_WRITE );
	
	if (!fp)
	{
		AAS_Error("Unable to open file: %s\n", filename);
		return;
	} //end if

	//create the header
	routecacheheader_t routecacheheader;
	routecacheheader.ident = RCID;
	routecacheheader.version = RCVERSION;
	routecacheheader.numareas = aasworld.numareas;
	routecacheheader.numclusters = aasworld.numclusters;
	routecacheheader.areacrc = CRC_ProcessString( (unsigned char *)aasworld.areas, sizeof(aas_area_t) * aasworld.numareas );
	routecacheheader.clustercrc = CRC_ProcessString( (unsigned char *)aasworld.clusters, sizeof(aas_cluster_t) * aasworld.numclusters );
	routecacheheader.numportalcache = numportalcache;
	routecacheheader.numareacache = numareacache;
	//write the header
	botimport.FS_Write(&routecacheheader, sizeof(routecacheheader_t), fp);
	//
	size_t totalsize = 0;
	//write all the cache
	for (size_t i = 0; i < aasworld.numareas; i++)
	{
		for (aas_routingcache_t *cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			botimport.FS_Write(cache, cache->size, fp);
			totalsize += cache->size;
		} //end for
	} //end for
	for (size_t i = 0; i < aasworld.numclusters; i++)
	{
		const aas_cluster_t& cluster = aasworld.clusters[i];
		for (size_t j = 0; j < cluster.numareas; j++)
		{
			for (aas_routingcache_t *cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				botimport.FS_Write(cache, cache->size, fp);
				totalsize += cache->size;
			} //end for
		} //end for
	} //end for
	// write the visareas
	/*
	for (i = 0; i < aasworld.numareas; i++)
	{
		if (!aasworld.areavisibility[i]) {
			size = 0;
			botimport.FS_Write(&size, sizeof(int), fp);
			continue;
		}
		AAS_DecompressVis( aasworld.areavisibility[i], aasworld.numareas, aasworld.decompressedvis );
		size = AAS_CompressVis( aasworld.decompressedvis, aasworld.numareas, aasworld.decompressedvis );
		botimport.FS_Write(&size, sizeof(int), fp);
		botimport.FS_Write(aasworld.decompressedvis, size, fp);
	}
	*/
	//
	botimport.FS_FCloseFile(fp);
	botimport.Print(PRT_MESSAGE, "\nroute cache written to %s\n", filename);
	botimport.Print(PRT_MESSAGE, "written %d bytes of routing cache\n", totalsize);
} //end of the function AAS_WriteRouteCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
aas_routingcache_t *AAS_ReadCache(fileHandle_t fp)
{
	if (!fp)
		return NULL;
	

	size_t size;
	botimport.FS_Read(&size, sizeof(size), fp);
	aas_routingcache_t *cache = (aas_routingcache_t *) GetMemory(size);
	cache->size = size;
	botimport.FS_Read((unsigned char *)cache + sizeof(size), size - sizeof(size), fp);
	cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t) - sizeof(unsigned short) +
		(size - sizeof(aas_routingcache_t) + sizeof(unsigned short)) / 3 * 2;
	return cache;
} //end of the function AAS_ReadCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_ReadRouteCache()
{
	fileHandle_t fp;
	char filename[MAX_QPATH];

	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	botimport.FS_FOpenFile( filename, &fp, FS_READ );
	if (!fp)
	{
		return qfalse;
	} //end if

	routecacheheader_t routecacheheader;
	botimport.FS_Read(&routecacheheader, sizeof(routecacheheader_t), fp );
	if (routecacheheader.ident != RCID)
	{
		AAS_Error("%s is not a route cache dump\n", filename);
		return qfalse;
	} //end if
	if (routecacheheader.version != RCVERSION)
	{
		AAS_Error("route cache dump has wrong version %d, should be %d", routecacheheader.version, RCVERSION);
		return qfalse;
	} //end if
	if (routecacheheader.numareas != aasworld.numareas)
	{
		//AAS_Error("route cache dump has wrong number of areas\n");
		return qfalse;
	} //end if
	if (routecacheheader.numclusters != aasworld.numclusters)
	{
		//AAS_Error("route cache dump has wrong number of clusters\n");
		return qfalse;
	} //end if
	if (routecacheheader.areacrc !=
		CRC_ProcessString( (unsigned char *)aasworld.areas, sizeof(aas_area_t) * aasworld.numareas ))
	{
		//AAS_Error("route cache dump area CRC incorrect\n");
		return qfalse;
	} //end if
	if (routecacheheader.clustercrc !=
		CRC_ProcessString( (unsigned char *)aasworld.clusters, sizeof(aas_cluster_t) * aasworld.numclusters ))
	{
		//AAS_Error("route cache dump cluster CRC incorrect\n");
		return qfalse;
	} //end if

	//read all the portal cache
	for (int i = 0; i < routecacheheader.numportalcache; i++)
	{
		if (aas_routingcache_t *cache = AAS_ReadCache(fp))
		{
			cache->next = aasworld.portalcache[cache->areanum];
			cache->prev = NULL;
			if (aasworld.portalcache[cache->areanum])
				aasworld.portalcache[cache->areanum]->prev = cache;
			aasworld.portalcache[cache->areanum] = cache;
		}
		else
			break;
	} //end for
	//read all the cluster area cache
	for (int i = 0; i < routecacheheader.numareacache; i++)
	{
		if (aas_routingcache_t *cache = AAS_ReadCache(fp))
		{
			int clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
			cache->next = aasworld.clusterareacache[cache->cluster][clusterareanum];
			cache->prev = NULL;
			if (aasworld.clusterareacache[cache->cluster][clusterareanum])
				aasworld.clusterareacache[cache->cluster][clusterareanum]->prev = cache;
			aasworld.clusterareacache[cache->cluster][clusterareanum] = cache;
		}
		else
			break;
	} //end for
	// read the visareas
	/*
	aasworld.areavisibility = (byte **) GetClearedMemory(aasworld.numareas * sizeof(byte *));
	aasworld.decompressedvis = (byte *) GetClearedMemory(aasworld.numareas * sizeof(byte));
	for (i = 0; i < aasworld.numareas; i++)
	{
		botimport.FS_Read(&size, sizeof(size), fp );
		if (size) {
			aasworld.areavisibility[i] = (byte *) GetMemory(size);
			botimport.FS_Read(aasworld.areavisibility[i], size, fp );
		}
	}
	*/
	//
	botimport.FS_FCloseFile(fp);
	return qtrue;
} //end of the function AAS_ReadRouteCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#define MAX_REACHABILITYPASSAREAS		64

void AAS_InitReachabilityAreas()
{
	int areas[MAX_REACHABILITYPASSAREAS];
	vec3_t start, end;

	if (aasworld.reachabilityareas)
		FreeMemory(aasworld.reachabilityareas);
	if (aasworld.reachabilityareaindex)
		FreeMemory(aasworld.reachabilityareaindex);

	aasworld.reachabilityareas = (aas_reachabilityareas_t *)
				GetClearedMemory(aasworld.reachabilitysize * sizeof(aas_reachabilityareas_t));
	aasworld.reachabilityareaindex = (int *)
				GetClearedMemory(aasworld.reachabilitysize * MAX_REACHABILITYPASSAREAS * sizeof(int));

	int numreachareas = 0;
	for (int i = 0; i < aasworld.reachabilitysize; i++)
	{
		const aas_reachability_t& reach = aasworld.reachability[i];
		int numareas = 0;
		switch(reach.traveltype & TRAVELTYPE_MASK)
		{
			//trace areas from start to end
			case TRAVEL_BARRIERJUMP:
			case TRAVEL_WATERJUMP:
			case TRAVEL_ROCKETJUMP: 
			case TRAVEL_BFGJUMP: 
				VectorCopy(reach.start, end);
				end[2] = reach.end[2];
				numareas = AAS_TraceAreas(reach.start, end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_WALKOFFLEDGE:
				VectorCopy(reach.end, start);
				start[2] = reach.start[2];
				numareas = AAS_TraceAreas(start, reach.end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_JUMPPAD: 
			case TRAVEL_JUMP: 
			case TRAVEL_GRAPPLEHOOK:
				numareas = AAS_TraceAreas(reach.start, reach.end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;

			//trace arch
			//case TRAVEL_JUMP: break;

			//trace from reach->start to entity center, along entity movement
			//and from entity center to reach->end
			case TRAVEL_ELEVATOR: break;
			case TRAVEL_FUNCBOB: break;

			//no areas in between
			case TRAVEL_WALK: break;
			case TRAVEL_CROUCH: break;
			case TRAVEL_LADDER: break;
			case TRAVEL_SWIM: break;
			case TRAVEL_TELEPORT: break;
			default: break;
		} //end switch
		aasworld.reachabilityareas[i].firstarea = numreachareas;
		aasworld.reachabilityareas[i].numareas = numareas;
		for (int j = 0; j < numareas; j++)
		{
			aasworld.reachabilityareaindex[numreachareas++] = areas[j];
		} //end for
	} //end for
} //end of the function AAS_InitReachabilityAreas
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_InitRouting()
{
	AAS_InitTravelFlagFromType();
	//
	AAS_InitAreaContentsTravelFlags();
	//initialize the routing update fields
	AAS_InitRoutingUpdate();
	//create reversed reachability links used by the routing update algorithm
	AAS_CreateReversedReachability();
	//initialize the cluster cache
	AAS_InitClusterAreaCache();
	//initialize portal cache
	AAS_InitPortalCache();
	//initialize the area travel times
	AAS_CalculateAreaTravelTimes();
	//calculate the maximum travel times through portals
	AAS_InitPortalMaxTravelTimes();
	//get the areas reachabilities go through
	AAS_InitReachabilityAreas();
	//
#ifdef ROUTING_DEBUG
	numareacacheupdates = 0;
	numportalcacheupdates = 0;
#endif //ROUTING_DEBUG
	//
	routingcachesize = 0;
	max_routingcachesize = 1024 * (int) LibVarValue("max_routingcache", "4096");
	// read any routing cache if available
	AAS_ReadRouteCache();
} //end of the function AAS_InitRouting
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeRoutingCaches()
{
	// free all the existing cluster area cache
	AAS_FreeAllClusterAreaCache();
	// free all the existing portal cache
	AAS_FreeAllPortalCache();
	// free cached travel times within areas
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	aasworld.areatraveltimes = NULL;
	// free cached maximum travel time through cluster portals
	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);
	aasworld.portalmaxtraveltimes = NULL;
	// free reversed reachability links
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	aasworld.reversedreachability = NULL;
	// free routing algorithm memory
	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	aasworld.areaupdate = NULL;
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	aasworld.portalupdate = NULL;
	// free lists with areas the reachabilities go through
	if (aasworld.reachabilityareas) FreeMemory(aasworld.reachabilityareas);
	aasworld.reachabilityareas = NULL;
	// free the reachability area index
	if (aasworld.reachabilityareaindex) FreeMemory(aasworld.reachabilityareaindex);
	aasworld.reachabilityareaindex = NULL;
	// free area contents travel flags look up table
	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = NULL;
} //end of the function AAS_FreeRoutingCaches
//===========================================================================
// update the given routing cache
//
// Parameter:			areacache		: routing cache to update
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_UpdateAreaRoutingCache(aas_routingcache_t* const areacache)
{

#ifdef ROUTING_DEBUG
	numareacacheupdates++;
#endif //ROUTING_DEBUG
	//number of reachability areas within this cluster
	size_t numreachabilityareas = aasworld.clusters[areacache->cluster].numreachabilityareas;
	//
	aasworld.frameroutingupdates++;
	//clear the routing update fields
//	Com_Memset(aasworld.areaupdate, 0, aasworld.numareas * sizeof(aas_routingupdate_t));
	//
	int badtravelflags = ~areacache->travelflags;
	//
	int clusterareanum = AAS_ClusterAreaNum(areacache->cluster, areacache->areanum);
	if (clusterareanum >= numreachabilityareas) return;
	//
	unsigned short int startareatraveltimes[128]; //NOTE: not more than 128 reachabilities per area allowed
	Com_Memset(startareatraveltimes, 0, sizeof(startareatraveltimes));
	//
	aas_routingupdate_t* curupdate = &aasworld.areaupdate[clusterareanum];
	curupdate->areanum = areacache->areanum;
	//VectorCopy(areacache->origin, curupdate->start);
	curupdate->areatraveltimes = startareatraveltimes;
	curupdate->tmptraveltime = areacache->starttraveltime;
	//
	areacache->traveltimes[clusterareanum] = areacache->starttraveltime;
	//put the area to start with in the current read list
	curupdate->next = NULL;
	curupdate->prev = NULL;
	aas_routingupdate_t* updateliststart = curupdate;
	aas_routingupdate_t* updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		//
		curupdate->inlist = qfalse;
		//check all reversed reachability links
		const aas_reversedreachability_t& revreach = aasworld.reversedreachability[curupdate->areanum];
		//
		int i=0;
		for (aas_reversedlink_t *revlink = revreach.first; revlink; revlink = revlink->next, i++)
		{
			int linknum = revlink->linknum;
			const aas_reachability_t& reach = aasworld.reachability[linknum];
			//if there is used an undesired travel type
			if (AAS_TravelFlagForType_inline(reach.traveltype) & badtravelflags) continue;
			//if not allowed to enter the next area
			if (aasworld.areasettings[reach.areanum].areaflags & AREA_DISABLED) continue;
			//if the next area has a not allowed travel flag
			if (AAS_AreaContentsTravelFlags_inline(reach.areanum) & badtravelflags) continue;
			//number of the area the reversed reachability leads to
			int nextareanum = revlink->areanum;
			//get the cluster number of the area
			int cluster = aasworld.areasettings[nextareanum].cluster;
			//don't leave the cluster
			if (cluster > 0 && cluster != areacache->cluster) continue;
			//get the number of the area in the cluster
			clusterareanum = AAS_ClusterAreaNum(areacache->cluster, nextareanum);
			if (clusterareanum >= numreachabilityareas) continue;
			//time already travelled plus the traveltime through
			//the current area plus the travel time from the reachability
			unsigned short int t = curupdate->tmptraveltime +
						//AAS_AreaTravelTime(curupdate->areanum, curupdate->start, reach->end) +
						curupdate->areatraveltimes[i] +
							reach.traveltime;
			//
			if (!areacache->traveltimes[clusterareanum] ||
					areacache->traveltimes[clusterareanum] > t)
			{
				areacache->traveltimes[clusterareanum] = t;
				areacache->reachabilities[clusterareanum] = linknum - aasworld.areasettings[nextareanum].firstreachablearea;
				aas_routingupdate_t& nextupdate = aasworld.areaupdate[clusterareanum];
				nextupdate.areanum = nextareanum;
				nextupdate.tmptraveltime = t;
				//VectorCopy(reach->start, nextupdate->start);
				nextupdate.areatraveltimes = aasworld.areatraveltimes[nextareanum][linknum -
													aasworld.areasettings[nextareanum].firstreachablearea];
				if (!nextupdate.inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate.next = NULL;
					nextupdate.prev = updatelistend;
					if (updatelistend) updatelistend->next = &nextupdate;
					else updateliststart = &nextupdate;
					updatelistend = &nextupdate;
					nextupdate.inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
} //end of the function AAS_UpdateAreaRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
aas_routingcache_t *AAS_GetAreaRoutingCache(int clusternum, int areanum, int travelflags)
{
	//number of the area in the cluster
	int clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
	//pointer to the cache for the area in the cluster
	aas_routingcache_t *cache, *clustercache = aasworld.clusterareacache[clusternum][clusterareanum];
	//find the cache without undesired travel flags
	for (cache = clustercache; cache; cache = cache->next)
	{
		//if there aren't used any undesired travel types for the cache
		if (cache->travelflags == travelflags) break;
	} //end for
	//if there was no cache
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(aasworld.clusters[clusternum].numreachabilityareas);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		cache->prev = NULL;
		cache->next = clustercache;
		if (clustercache) clustercache->prev = cache;
		aasworld.clusterareacache[clusternum][clusterareanum] = cache;
		AAS_UpdateAreaRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_AREA;
	AAS_LinkCache(cache);
	return cache;
} //end of the function AAS_GetAreaRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_UpdatePortalRoutingCache(aas_routingcache_t* const portalcache)
{

#ifdef ROUTING_DEBUG
	numportalcacheupdates++;
#endif //ROUTING_DEBUG
	//clear the routing update fields
//	Com_Memset(aasworld.portalupdate, 0, (aasworld.numportals+1) * sizeof(aas_routingupdate_t));
	//
	aas_routingupdate_t* curupdate = &aasworld.portalupdate[aasworld.numportals];
	curupdate->cluster = portalcache->cluster;
	curupdate->areanum = portalcache->areanum;
	curupdate->tmptraveltime = portalcache->starttraveltime;
	//if the start area is a cluster portal, store the travel time for that portal
	int clusternum = aasworld.areasettings[portalcache->areanum].cluster;
	if (clusternum < 0)
	{
		portalcache->traveltimes[-clusternum] = portalcache->starttraveltime;
	} //end if
	//put the area to start with in the current read list
	curupdate->next = NULL;
	curupdate->prev = NULL;
	aas_routingupdate_t* updateliststart = curupdate;
	aas_routingupdate_t* updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//remove the current update from the list
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		//current update is removed from the list
		curupdate->inlist = qfalse;
		//
		const aas_cluster_t& cluster = aasworld.clusters[curupdate->cluster];
		//
		aas_routingcache_t* cache = AAS_GetAreaRoutingCache(curupdate->cluster,
								curupdate->areanum, portalcache->travelflags);
		//take all portals of the cluster
		for (int i = 0; i < cluster.numportals; i++)
		{
			int portalnum = aasworld.portalindex[cluster.firstportal + i];
			const aas_portal_t& portal = aasworld.portals[portalnum];
			//if this is the portal of the current update continue
			if (portal.areanum == curupdate->areanum) continue;
			//
			int clusterareanum = AAS_ClusterAreaNum(curupdate->cluster, portal.areanum);
			if (clusterareanum >= cluster.numreachabilityareas) continue;
			//
			unsigned short int t = cache->traveltimes[clusterareanum];
			if (!t) continue;
			t += curupdate->tmptraveltime;
			//
			if (!portalcache->traveltimes[portalnum] ||
					portalcache->traveltimes[portalnum] > t)
			{
				portalcache->traveltimes[portalnum] = t;
				aas_routingupdate_t* nextupdate = &aasworld.portalupdate[portalnum];
				if (portal.frontcluster == curupdate->cluster)
				{
					nextupdate->cluster = portal.backcluster;
				} //end if
				else
				{
					nextupdate->cluster = portal.frontcluster;
				} //end else
				nextupdate->areanum = portal.areanum;
				//add travel time through the actual portal area for the next update
				nextupdate->tmptraveltime = t + aasworld.portalmaxtraveltimes[portalnum];
				if (!nextupdate->inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
} //end of the function AAS_UpdatePortalRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
aas_routingcache_t *AAS_GetPortalRoutingCache(int clusternum, int areanum, int travelflags)
{
	aas_routingcache_t *cache;

	//find the cached portal routing if existing
	for (cache = aasworld.portalcache[areanum]; cache; cache = cache->next)
	{
		if (cache->travelflags == travelflags) break;
	} //end for
	//if the portal routing isn't cached
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(aasworld.numportals);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		//add the cache to the cache list
		cache->prev = NULL;
		cache->next = aasworld.portalcache[areanum];
		if (aasworld.portalcache[areanum]) aasworld.portalcache[areanum]->prev = cache;
		aasworld.portalcache[areanum] = cache;
		//update the cache
		AAS_UpdatePortalRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_PORTAL;
	AAS_LinkCache(cache);
	return cache;
} //end of the function AAS_GetPortalRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaRouteToGoalArea(int areanum, const vec3_t origin, int goalareanum, int travelflags, int& traveltime, int& reachnum)
{
	if (!aasworld.initialized) return qfalse;

	if (areanum == goalareanum)
	{
		traveltime = 1;
		reachnum = 0;
		return qtrue;
	}

	//
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (bot_developer)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: areanum %d out of range\n", areanum);
		} //end if
		return qfalse;
	} //end if
	if (goalareanum <= 0 || goalareanum >= aasworld.numareas)
	{
		if (bot_developer)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: goalareanum %d out of range\n", goalareanum);
		} //end if
		return qfalse;
	} //end if

	// make sure the routing cache doesn't grow to large
	while(AvailableMemory() < 1 * 1024 * 1024) {
		if (!AAS_FreeOldestCache()) break;
	}
	//
	if (AAS_AreaDoNotEnter(areanum) || AAS_AreaDoNotEnter(goalareanum))
	{
		travelflags |= TFL_DONOTENTER;
	} //end if
	//NOTE: the number of routing updates is limited per frame
	/*
	if (aasworld.frameroutingupdates > MAX_FRAMEROUTINGUPDATES)
	{
#ifdef DEBUG
		//Log_Write("WARNING: AAS_AreaTravelTimeToGoalArea: frame routing updates overflowed");
#endif
		return 0;
	} //end if
	*/

	//
	int clusternum = aasworld.areasettings[areanum].cluster;
	int goalclusternum = aasworld.areasettings[goalareanum].cluster;
	//check if the area is a portal of the goal area cluster
	if (clusternum < 0 && goalclusternum > 0)
	{
		const aas_portal_t& portal = aasworld.portals[-clusternum];
		if (portal.frontcluster == goalclusternum ||
				portal.backcluster == goalclusternum)
		{
			clusternum = goalclusternum;
		} //end if
	} //end if
	//check if the goalarea is a portal of the area cluster
	else if (clusternum > 0 && goalclusternum < 0)
	{
		const aas_portal_t& portal = aasworld.portals[-goalclusternum];
		if (portal.frontcluster == clusternum ||
				portal.backcluster == clusternum)
		{
			goalclusternum = clusternum;
		} //end if
	} //end if
	//if both areas are in the same cluster
	//NOTE: there might be a shorter route via another cluster!!! but we don't care
	if (clusternum > 0 && goalclusternum > 0 && clusternum == goalclusternum)
	{
		//
		const aas_routingcache_t& areacache = *AAS_GetAreaRoutingCache(clusternum, goalareanum, travelflags);
		//the number of the area in the cluster
		int clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//the cluster the area is in
		const aas_cluster_t& cluster = aasworld.clusters[clusternum];
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster.numreachabilityareas) return 0;
		//if it is possible to travel to the goal area through this cluster
		if (areacache.traveltimes[clusterareanum] != 0)
		{
			reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache.reachabilities[clusterareanum];
			if (!origin) {
				traveltime = areacache.traveltimes[clusterareanum];
				return qtrue;
			}
			const aas_reachability_t& reach = aasworld.reachability[reachnum];
			traveltime = areacache.traveltimes[clusterareanum] +
							AAS_AreaTravelTime(areanum, origin, reach.start);
			//
			return qtrue;
		} //end if
	} //end if
	//
	clusternum = aasworld.areasettings[areanum].cluster;
	goalclusternum = aasworld.areasettings[goalareanum].cluster;
	//if the goal area is a portal
	if (goalclusternum < 0)
	{
		//just assume the goal area is part of the front cluster
		const aas_portal_t& portal = aasworld.portals[-goalclusternum];
		goalclusternum = portal.frontcluster;
	} //end if
	//get the portal routing cache
	const aas_routingcache_t& portalcache = *AAS_GetPortalRoutingCache(goalclusternum, goalareanum, travelflags);
	//if the area is a cluster portal, read directly from the portal cache
	if (clusternum < 0)
	{
		traveltime = portalcache.traveltimes[-clusternum];
		reachnum = aasworld.areasettings[areanum].firstreachablearea +
						portalcache.reachabilities[-clusternum];
		return qtrue;
	} //end if
	//
	unsigned short int besttime = 0;
	int bestreachnum = -1;
	//the cluster the area is in
	const aas_cluster_t& cluster = aasworld.clusters[clusternum];
	//find the portal of the area cluster leading towards the goal area
	for (int i = 0; i < cluster.numportals; i++)
	{
		int portalnum = aasworld.portalindex[cluster.firstportal + i];
		//if the goal area isn't reachable from the portal
		if (!portalcache.traveltimes[portalnum]) continue;
		//
		const aas_portal_t& portal = aasworld.portals[portalnum];
		//get the cache of the portal area
		const aas_routingcache_t& areacache = *AAS_GetAreaRoutingCache(clusternum, portal.areanum, travelflags);
		//current area inside the current cluster
		int clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster.numreachabilityareas) continue;
		//if the portal is NOT reachable from this area
		if (!areacache.traveltimes[clusterareanum]) continue;
		//total travel time is the travel time the portal area is from
		//the goal area plus the travel time towards the portal area
		unsigned short int t = portalcache.traveltimes[portalnum] + areacache.traveltimes[clusterareanum];
		//FIXME: add the exact travel time through the actual portal area
		//NOTE: for now we just add the largest travel time through the portal area
		//		because we can't directly calculate the exact travel time
		//		to be more specific we don't know which reachability was used to travel
		//		into the portal area
		t += aasworld.portalmaxtraveltimes[portalnum];
		//
		if (origin)
		{
			reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache.reachabilities[clusterareanum];
			const aas_reachability_t& reach = aasworld.reachability[ reachnum ];
			t += AAS_AreaTravelTime(areanum, origin, reach.start);
		} //end if
		//if the time is better than the one already found
		if (!besttime || t < besttime)
		{
			bestreachnum = reachnum;
			besttime = t;
		} //end if
	} //end for
	if (bestreachnum < 0) {
		return qfalse;
	}
	reachnum = bestreachnum;
	traveltime = besttime;
	return qtrue;
} //end of the function AAS_AreaRouteToGoalArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaTravelTimeToGoalArea(int areanum, const vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, traveltime, reachnum))
	{
		return traveltime;
	}
	return 0;
} //end of the function AAS_AreaTravelTimeToGoalArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaReachabilityToGoalArea(int areanum, const vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, traveltime, reachnum))
	{
		return reachnum;
	}
	return 0;
} //end of the function AAS_AreaReachabilityToGoalArea
//===========================================================================
// predict the route and stop on one of the stop events
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_PredictRoute(struct aas_predictroute_s *route, int areanum, const vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum)
{
	//init output
	route->stopevent = RSE_NONE;
	route->endarea = goalareanum;
	route->endcontents = 0;
	route->endtravelflags = 0;
	VectorCopy(origin, route->endpos);
	route->time = 0;

	int curareanum = areanum;
	vec3_t curorigin;
	VectorCopy(origin, curorigin);

	for (int i = 0; curareanum != goalareanum && (!maxareas || i < maxareas) && i < aasworld.numareas; i++)
	{
		int reachnum = AAS_AreaReachabilityToGoalArea(curareanum, curorigin, goalareanum, travelflags);
		if (!reachnum)
		{
			route->stopevent = RSE_NOROUTE;
			return qfalse;
		} //end if
		const aas_reachability_t& reach = aasworld.reachability[reachnum];
		//
		if (stopevent & RSE_USETRAVELTYPE)
		{
			if (AAS_TravelFlagForType_inline(reach.traveltype) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = curareanum;
				route->endcontents = aasworld.areasettings[curareanum].contents;
				route->endtravelflags = AAS_TravelFlagForType_inline(reach.traveltype);
				VectorCopy(reach.start, route->endpos);
				return qtrue;
			} //end if
			if (AAS_AreaContentsTravelFlags_inline(reach.areanum) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = reach.areanum;
				route->endcontents = aasworld.areasettings[reach.areanum].contents;
				route->endtravelflags = AAS_AreaContentsTravelFlags_inline(reach.areanum);
				VectorCopy(reach.end, route->endpos);
				route->time += AAS_AreaTravelTime(areanum, origin, reach.start);
				route->time += reach.traveltime;
				return qtrue;
			} //end if
		} //end if
		const aas_reachabilityareas_t& reachareas = aasworld.reachabilityareas[reachnum];
		for (int j = 0; j < reachareas.numareas + 1; j++)
		{
			int testareanum = j >= reachareas.numareas ? 
								reach.areanum :
								aasworld.reachabilityareaindex[reachareas.firstarea + j];

			if (stopevent & RSE_ENTERCONTENTS)
			{
				if (aasworld.areasettings[testareanum].contents & stopcontents)
				{
					route->stopevent = RSE_ENTERCONTENTS;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach.end, route->endpos);
					route->time += AAS_AreaTravelTime(areanum, origin, reach.start);
					route->time += reach.traveltime;
					return qtrue;
				} //end if
			} //end if
			if (stopevent & RSE_ENTERAREA)
			{
				if (testareanum == stopareanum)
				{
					route->stopevent = RSE_ENTERAREA;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach.start, route->endpos);
					return qtrue;
				} //end if
			} //end if
		} //end for

		route->time += AAS_AreaTravelTime(areanum, origin, reach.start);
		route->time += reach.traveltime;
		route->endarea = reach.areanum;
		route->endcontents = aasworld.areasettings[reach.areanum].contents;
		route->endtravelflags = AAS_TravelFlagForType_inline(reach.traveltype);
		VectorCopy(reach.end, route->endpos);
		//
		curareanum = reach.areanum;
		VectorCopy(reach.end, curorigin);
		//
		if (maxtime && route->time > maxtime)
			break;
	} //end while
	if (curareanum != goalareanum)
		return qfalse;
	return qtrue;
} //end of the function AAS_PredictRoute
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_BridgeWalkable(int areanum)
{
	return qfalse;
} //end of the function AAS_BridgeWalkable
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_ReachabilityFromNum(int num, struct aas_reachability_s& reach)
{
	if (!aasworld.initialized)
	{
		Com_Memset(&reach, 0, sizeof(aas_reachability_t));
		return;
	} //end if
	if (num < 0 || num >= aasworld.reachabilitysize)
	{
		Com_Memset(&reach, 0, sizeof(aas_reachability_t));
		return;
	} //end if
	Com_Memcpy(&reach, &aasworld.reachability[num], sizeof(aas_reachability_t));;
} //end of the function AAS_ReachabilityFromNum
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextAreaReachability(int areanum, int reachnum)
{
	if (!aasworld.initialized) return 0;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_NextAreaReachability: areanum %d out of range\n", areanum);
		return 0;
	} //end if

	const aas_areasettings_t& settings = aasworld.areasettings[areanum];
	if (!reachnum)
	{
		return settings.firstreachablearea;
	} //end if
	if (reachnum < settings.firstreachablearea)
	{
		botimport.Print(PRT_FATAL, "AAS_NextAreaReachability: reachnum < settings->firstreachableara");
		return 0;
	} //end if
	if (++reachnum >= (settings.firstreachablearea + settings.numreachableareas))
	{
		return 0;
	} //end if
	return reachnum;
} //end of the function AAS_NextAreaReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextModelReachability(int num, int modelnum)
{
	if (num <= 0) num = 1;
	else if (num >= aasworld.reachabilitysize) return 0;
	else num++;
	//
	for (int i = num; i < aasworld.reachabilitysize; i++)
	{
		if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_ELEVATOR)
		{
			if (aasworld.reachability[i].facenum == modelnum) return i;
		} //end if
		else if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_FUNCBOB)
		{
			if ((aasworld.reachability[i].facenum & 0x0000FFFF) == modelnum) return i;
		} //end if
	} //end for
	return 0;
} //end of the function AAS_NextModelReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_RandomGoalArea(int areanum, int travelflags, int& goalareanum, vec3_t goalorigin)
{
	//if the area has no reachabilities
	if (!AAS_AreaReachability(areanum)) return qfalse;
	//
	int n = aasworld.numareas * random();
	for (int i = 0; i < aasworld.numareas; i++)
	{
		if (n <= 0) n = 1;
		if (n >= aasworld.numareas) n = 1;
		if (AAS_AreaReachability(n))
		{
			int t = AAS_AreaTravelTimeToGoalArea(areanum, aasworld.areas[areanum].center, n, travelflags);
			//if the goal is reachable
			if (t > 0)
			{
				if (AAS_AreaSwim(n))
				{
					goalareanum = n;
					VectorCopy(aasworld.areas[n].center, goalorigin);
					//botimport.Print(PRT_MESSAGE, "found random goal area %d\n", *goalareanum);
					return qtrue;
				} //end if
				vec3_t start, end;

				VectorCopy(aasworld.areas[n].center, start);
				if (!AAS_PointAreaNum(start))
					Log_Write("area %d center %f %f %f in solid?", n, start[0], start[1], start[2]);
				VectorCopy(start, end);
				end[2] -= 300;
				aas_trace_t trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
				if (!trace.startsolid && trace.fraction < 1 && AAS_PointAreaNum(trace.endpos) == n)
				{
					if (AAS_AreaGroundFaceArea(n) > 300)
					{
						goalareanum = n;
						VectorCopy(trace.endpos, goalorigin);
						//botimport.Print(PRT_MESSAGE, "found random goal area %d\n", *goalareanum);
						return qtrue;
					} //end if
				} //end if
			} //end if
		} //end if
		n++;
	} //end for
	return qfalse;
} //end of the function AAS_RandomGoalArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaVisible(int srcarea, int destarea)
{
	return qfalse;
} //end of the function AAS_AreaVisible
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
float DistancePointToLine(const vec3_t v1, const vec3_t v2, const vec3_t point)
{
	vec3_t vec, p2;

	AAS_ProjectPointOntoVector(point, v1, v2, p2);
	VectorSubtract(point, p2, vec);
	return VectorLength(vec);
} //end of the function DistancePointToLine
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NearestHideArea(int srcnum, const vec3_t origin, int areanum, int enemynum, const vec3_t enemyorigin, int enemyareanum, int travelflags)
{
	static unsigned short int *hidetraveltimes;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	float dist1, dist2;
	vec3_t v1, v2, p;
	qbool startVisible;

	//
	if (!hidetraveltimes)
	{
		hidetraveltimes = (unsigned short int *) GetClearedMemory(aasworld.numareas * sizeof(unsigned short int));
	} //end if
	else
	{
		Com_Memset(hidetraveltimes, 0, aasworld.numareas * sizeof(unsigned short int));
	} //end else
	unsigned short int besttraveltime = 0;
	int bestarea = 0;
	//assume visible
	startVisible = qtrue;
	//
	int badtravelflags = ~travelflags;
	//
	curupdate = &aasworld.areaupdate[areanum];
	curupdate->areanum = areanum;
	VectorCopy(origin, curupdate->start);
	curupdate->areatraveltimes = aasworld.areatraveltimes[areanum][0];
	curupdate->tmptraveltime = 0;
	//put the area to start with in the current read list
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		//
		curupdate->inlist = qfalse;
		//check all reversed reachability links
		int numreach = aasworld.areasettings[curupdate->areanum].numreachableareas;
		reach = &aasworld.reachability[aasworld.areasettings[curupdate->areanum].firstreachablearea];
		//
		for (int i = 0; i < numreach; i++, reach++)
		{
			//if an undesired travel type is used
			if (AAS_TravelFlagForType_inline(reach->traveltype) & badtravelflags) continue;
			//
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & badtravelflags) continue;
			//number of the area the reachability leads to
			int nextareanum = reach->areanum;
			// if this moves us into the enemies area, skip it
			if (nextareanum == enemyareanum) continue;
			//time already travelled plus the traveltime through
			//the current area plus the travel time from the reachability
			unsigned short int t = curupdate->tmptraveltime +
						AAS_AreaTravelTime(curupdate->areanum, curupdate->start, reach->start) +
							reach->traveltime;

			//avoid going near the enemy
			AAS_ProjectPointOntoVector(enemyorigin, curupdate->start, reach->end, p);
			int j;
			for (j = 0; j < 3; j++)
				if ((p[j] > curupdate->start[j] && p[j] > reach->end[j]) ||
					(p[j] < curupdate->start[j] && p[j] < reach->end[j]))
					break;
			if (j < 3)
			{
				VectorSubtract(enemyorigin, reach->end, v2);
			} //end if
			else
			{
				VectorSubtract(enemyorigin, p, v2);
			} //end else
			dist2 = VectorLength(v2);
			//never go through the enemy
			if (dist2 < 40) continue;
			//
			VectorSubtract(enemyorigin, curupdate->start, v1);
			dist1 = VectorLength(v1);
			//
			if (dist2 < dist1)
			{
				t += (dist1 - dist2) * 10;
			}
			// if we weren't visible when starting, make sure we don't move into their view
			if (!startVisible && AAS_AreaVisible(enemyareanum, nextareanum)) {
				continue;
			}
			//
			if (besttraveltime && t >= besttraveltime) continue;
			//
			if (!hidetraveltimes[nextareanum] ||
					hidetraveltimes[nextareanum] > t)
			{
				//if the nextarea is not visible from the enemy area
				if (!AAS_AreaVisible(enemyareanum, nextareanum))
				{
					besttraveltime = t;
					bestarea = nextareanum;
				} //end if
				hidetraveltimes[nextareanum] = t;
				nextupdate = &aasworld.areaupdate[nextareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				//remember where we entered this area
				VectorCopy(reach->end, nextupdate->start);
				//if this update is not in the list yet
				if (!nextupdate->inlist)
				{
					//add the new update to the end of the list
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
	return bestarea;
} //end of the function AAS_NearestHideArea
