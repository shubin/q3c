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
// tr_shade.c

#include "tr_local.h"


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t tess;


// we must set some things up before beginning any tesselation
// because a surface may be forced to perform a RB_End due to overflow

void RB_BeginSurface( const shader_t* shader, int fogNum )
{
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = shader;
	tess.fogNum = fogNum;
	tess.xstages = (const shaderStage_t**)shader->stages;
	tess.depthFade = DFT_NONE;
	tess.deformsPreApplied = qfalse;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}
}


static void RB_DrawDynamicLight()
{
	backEnd.pc[RB_LIT_VERTICES_LATECULLTEST] += tess.numVertexes;

	static byte clipBits[SHADER_MAX_VERTEXES];
	const dlight_t* dl = tess.light;
	const cullType_t cullType = tess.shader->cullType;

	for (int i = 0; i < tess.numVertexes; ++i) {
		vec3_t dist;
		VectorSubtract(dl->transformed, tess.xyz[i], dist);

		const float dp = DotProduct(dist, tess.normal[i]);
		if ((cullType == CT_FRONT_SIDED && dp <= 0.0f) ||
			(cullType == CT_BACK_SIDED  && dp >= 0.0f)) {
			clipBits[i] = byte(-1);
			continue;
		}

		int clip = 0;
		if (dist[0] > dl->radius)
			clip |= 1;
		else if (dist[0] < -dl->radius)
			clip |= 2;
		if (dist[1] > dl->radius)
			clip |= 4;
		else if (dist[1] < -dl->radius)
			clip |= 8;
		if (dist[2] > dl->radius)
			clip |= 16;
		else if (dist[2] < -dl->radius)
			clip |= 32;

		clipBits[i] = clip;
	}

	// build a list of triangles that need light
	int numIndexes = 0;
	for (int i = 0; i < tess.numIndexes; i += 3) {
		const int a = tess.indexes[i + 0];
		const int b = tess.indexes[i + 1];
		const int c = tess.indexes[i + 2];
		if (!(clipBits[a] & clipBits[b] & clipBits[c])) {
			tess.dlIndexes[numIndexes + 0] = a;
			tess.dlIndexes[numIndexes + 1] = b;
			tess.dlIndexes[numIndexes + 2] = c;
			numIndexes += 3;
		}
	}
	tess.dlNumIndexes = numIndexes;

	backEnd.pc[RB_LIT_INDICES_LATECULL_IN] += numIndexes;
	backEnd.pc[RB_LIT_INDICES_LATECULL_OUT] += tess.numIndexes - numIndexes;

	if (numIndexes <= 0)
		return;

	backEnd.pc[RB_LIT_BATCHES]++;
	backEnd.pc[RB_LIT_VERTICES] += tess.numVertexes;
	backEnd.pc[RB_LIT_INDICES] += tess.numIndexes;
	gal.Draw(DT_DYNAMIC_LIGHT);
}


static void RB_DrawGeneric()
{
	if (tess.depthFade == DFT_NONE && tess.fogNum && tess.shader->fogPass) {
		tess.drawFog = qtrue;

		unsigned int fogStateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		if (tess.shader->fogPass == FP_EQUAL)
			fogStateBits |= GLS_DEPTHFUNC_EQUAL;
		tess.fogStateBits = fogStateBits;

		const fog_t* fog = tr.world->fogs + tess.fogNum;
		for (int i = 0; i < tess.numVertexes; ++i) {
			*(int*)&tess.svarsFog.colors[i] = fog->colorInt;
		}
		RB_CalcFogTexCoords((float*)tess.svarsFog.texcoords, 0, tess.numVertexes);
		tess.svarsFog.texcoordsptr = tess.svarsFog.texcoords;
	} else {
		tess.drawFog = qfalse;
	}

	backEnd.pc[RB_BATCHES]++;
	backEnd.pc[RB_VERTICES] += tess.numVertexes;
	backEnd.pc[RB_INDICES] += tess.numIndexes;
	gal.Draw(tess.depthFade != DFT_NONE ? DT_SOFT_SPRITE : DT_GENERIC);
}


static void RB_DrawDebug( const shaderCommands_t* input, qbool drawNormals, int options )
{
	if (drawNormals) {
		// we only draw the normals for the first (SHADER_MAX_VERTEXES / 2 - 1) vertices
		int nv = tess.numVertexes;
		if (nv >= SHADER_MAX_VERTEXES / 2)
			nv = SHADER_MAX_VERTEXES / 2 - 1;
		for (int i = 0, j = nv; i < nv; ++i, ++j) {
			VectorMA(input->xyz[i], 2, input->normal[i], tess.xyz[j]);
		}
		for (int i = 0, j = 0; i < nv; ++i, j += 3) {
			tess.indexes[j + 0] = i;
			tess.indexes[j + 1] = i;
			tess.indexes[j + 2] = i + nv;
		}
		tess.numVertexes = nv * 2;
		tess.numIndexes = nv * 3;
	}

	const cullType_t cull = (options & SHOWTRIS_BACKFACE_BIT) ? CT_BACK_SIDED : CT_FRONT_SIDED;
	RB_PushSingleStageShader(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE, cull);

	shaderStage_t* const stage = tess.shader->stages[0];
	if (options & SHOWTRIS_VERTEX_COLOR_BIT) {
		stage->rgbGen = CGEN_EXACT_VERTEX;
	} else if (options & SHOWTRIS_VERTEX_ALPHA_BIT) {
		stage->rgbGen = CGEN_DEBUG_ALPHA;
	} else {
		stage->rgbGen = CGEN_CONST;
		stage->constantColor[0] = drawNormals ? 0 : 255;
		stage->constantColor[1] = drawNormals ? 0 : 255;
		stage->constantColor[2] = 255;
		stage->constantColor[3] = 255;
	}
	stage->alphaGen = AGEN_SKIP;
	R_ComputeColors(tess.shader->stages[0], tess.svars[0], 0, tess.numVertexes);

	if ((options & SHOWTRIS_OCCLUDE_BIT) == 0) {
		gal.SetDepthRange(0, 0);
	}
	gal.Draw(DT_GENERIC);
	gal.SetDepthRange(0, 1);

	RB_PopShader();
}


void RB_EndSurface()
{
	shaderCommands_t* input = &tess;

	if (!input->numIndexes || !input->numVertexes)
		return;

	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit" );
	}
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit" );
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->value > 0.0f && r_debugSort->value < tess.shader->sort ) {
		return;
	}

	const shader_t* shader = input->shader;
	if (shader->sort == SS_ENVIRONMENT) {
		RB_DrawSky();
	} else {
		if (!tess.deformsPreApplied) {
			RB_DeformTessGeometry(0, tess.numVertexes, 0, tess.numIndexes);
			for (int i = 0; i < shader->numStages; ++i) {
				R_ComputeColors(shader->stages[i], tess.svars[i], 0, tess.numVertexes);
				R_ComputeTexCoords(shader->stages[i], tess.svars[i], 0, tess.numVertexes, qtrue);
			}
		}

		if (input->pass == shaderCommands_t::TP_LIGHT)
			RB_DrawDynamicLight();
		else
			RB_DrawGeneric();
	}

	// draw debugging stuff
	const qbool showTris = r_showtris->integer & SHOWTRIS_ENABLE_BIT;
	const qbool showNormals = r_shownormals->integer & SHOWTRIS_ENABLE_BIT;
	if (!backEnd.projection2D &&
		(tess.pass == shaderCommands_t::TP_BASE) &&
		tess.numIndexes > 0 &&
		tess.numVertexes > 0 &&
		(showTris || showNormals)) {
		if (showTris)
			RB_DrawDebug(input, qfalse, r_showtris->integer);
		if (showNormals)
			RB_DrawDebug(input, qtrue, r_shownormals->integer);
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
}
