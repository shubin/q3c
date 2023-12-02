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
// tr_models.c -- model loading and caching

#include "tr_local.h"

#define LL(x) x=LittleLong(x)


const model_t* R_GetModelByHandle( qhandle_t index )
{
	// out of range gets the default model
	if ( index < 1 || index >= tr.numModels )
		return tr.models[0];

	return tr.models[index];
}


model_t* R_AllocModel()
{
	if ( tr.numModels == MAX_RENDERER_MODELS )
		ri.Error( ERR_DROP, "R_AllocModel: MAX_RENDERER_MODELS hit\n" );

	model_t* mod = RI_New<model_t>();
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}


static qbool R_LoadMD3( model_t *mod, int lod, void *buffer, const char *mod_name )
{
	int					i, j;
	md3Header_t			*pinmodel;
	md3Frame_t			*frame;
	md3Surface_t		*surf;
	md3Shader_t			*shader;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3XyzNormal_t		*xyz;
	md3Tag_t			*tag;
	int					version;
	int					size;

	pinmodel = (md3Header_t *)buffer;

	version = LittleLong( pinmodel->version );
	if (version != MD3_VERSION) {
		ri.Printf( PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n",
				mod_name, version, MD3_VERSION );
		return qfalse;
	}

	mod->type = MOD_MD3;
	size = LittleLong(pinmodel->ofsEnd);
	mod->dataSize += size;
	mod->md3[lod] = (md3Header_t*)ri.Hunk_Alloc( size, h_low );

	Com_Memcpy (mod->md3[lod], buffer, LittleLong(pinmodel->ofsEnd) );

	LL( mod->md3[lod]->ident );
	LL( mod->md3[lod]->version );
	LL( mod->md3[lod]->numFrames );
	LL( mod->md3[lod]->numTags );
	LL( mod->md3[lod]->numSurfaces );
	LL( mod->md3[lod]->ofsFrames );
	LL( mod->md3[lod]->ofsTags );
	LL( mod->md3[lod]->ofsSurfaces );
	LL( mod->md3[lod]->ofsEnd );

	if ( mod->md3[lod]->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMD3: %s has no frames\n", mod_name );
		return qfalse;
	}

	// swap all the frames
	frame = (md3Frame_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsFrames );
	for ( i = 0 ; i < mod->md3[lod]->numFrames ; i++, frame++) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0 ; j < 3 ; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	// swap all the tags
	tag = (md3Tag_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsTags );
	for ( i = 0 ; i < mod->md3[lod]->numTags * mod->md3[lod]->numFrames ; i++, tag++) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	// swap all the surfaces
	surf = (md3Surface_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsSurfaces );
	for ( i = 0 ; i < mod->md3[lod]->numSurfaces ; i++) {

		LL( surf->ident );
		LL( surf->flags );
		LL( surf->numFrames );
		LL( surf->numShaders );
		LL( surf->numTriangles );
		LL( surf->ofsTriangles );
		LL( surf->numVerts );
		LL( surf->ofsShaders );
		LL( surf->ofsSt );
		LL( surf->ofsXyzNormals );
		LL( surf->ofsEnd );

		if ( surf->numVerts > SHADER_MAX_VERTEXES ) {
			ri.Error (ERR_DROP, "R_LoadMD3: %s has more than %i verts on a surface (%i)",
				mod_name, SHADER_MAX_VERTEXES, surf->numVerts );
		}
		if ( surf->numTriangles*3 > SHADER_MAX_INDEXES ) {
			ri.Error (ERR_DROP, "R_LoadMD3: %s has more than %i triangles on a surface (%i)",
				mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles );
		}

		// change to surface identifier
		surf->ident = SF_MD3;

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surf->name );

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

		// register the shaders
		shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
		for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
			const shader_t* sh = R_FindShader( shader->name, LIGHTMAP_NONE, FINDSHADER_MIPRAWIMAGE_BIT );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
		}

		// swap all the triangles
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);
		}

		// swap all the ST
		st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
		for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
			st->st[0] = LittleFloat( st->st[0] );
			st->st[1] = LittleFloat( st->st[1] );
		}

		// swap all the XyzNormals
		xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
		for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ )
		{
			xyz->xyz[0] = LittleShort( xyz->xyz[0] );
			xyz->xyz[1] = LittleShort( xyz->xyz[1] );
			xyz->xyz[2] = LittleShort( xyz->xyz[2] );
			xyz->normal = LittleShort( xyz->normal );
		}

		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}


/*
Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an optimization
to prevent disk rescanning if they are asked for again.
*/

qhandle_t RE_RegisterModel( const char* name )
{
	model_t* mod;
	char		*fext, defex[] = "md3", filename[MAX_QPATH], namebuf[MAX_QPATH+20];

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	// search the currently loaded models
	for ( qhandle_t h = 1; h < tr.numModels; ++h ) {
		mod = tr.models[h];
		if ( !strcmp( mod->name, name ) ) {
			return (mod->type == MOD_BAD) ? 0 :h;
		}
	}

	mod = R_AllocModel();
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );
	mod->numLods = 0;

	void* buf;
	int lod, numLoaded = 0;

	strcpy(filename, name);

	fext = strchr(filename, '.');
	if(!fext)
		fext = defex;
	else
	{
		*fext = '\0';
		fext++;
	}

	fext = defex;

	for ( lod = MD3_MAX_LODS - 1 ; lod >= 0 ; lod-- ) {
		if ( lod )
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		ri.FS_ReadFile( namebuf, (void **)&buf );
		if ( !buf ) {
			continue;
		}

		int ident = LittleLong(*(unsigned *)buf);
		if ( ident != MD3_IDENT ) {
			ri.Printf( PRINT_WARNING, "RE_RegisterModel: unknown fileid for %s\n", name );
			goto fail;
		}
		qbool loaded = R_LoadMD3( mod, lod, buf, name );

		ri.FS_FreeFile (buf);

		if ( !loaded ) {
			if ( lod == 0 ) {
				goto fail;
			} else {
				break;
			}
		} else {
			mod->numLods++;
			numLoaded++;
			// if we have a valid model and are biased
			// so that we won't see any higher detail ones,
			// stop loading them
//			if ( lod <= r_lodbias->integer ) {
//				break;
//			}
		}
	}

	if ( numLoaded ) {
		// duplicate into higher lod spots that weren't loaded
		// in case the user changes r_lodbias on the fly
		for ( lod-- ; lod >= 0 ; lod-- ) {
			mod->numLods++;
			mod->md3[lod] = mod->md3[lod+1];
		}
		return mod->index;
	}
#ifdef _DEBUG
	else {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: couldn't load %s\n", name );
	}
#endif

fail:
	// we still keep the model_t around, so if the model name is asked for
	// again, we won't bother scanning the filesystem
	mod->type = MOD_BAD;
	return 0;
}


///////////////////////////////////////////////////////////////


void R_ModelInit()
{
	tr.numModels = 0;
	// leave a space for NULL model
	model_t* mod = R_AllocModel();
	mod->type = MOD_BAD;
}


void R_Modellist_f( void )
{
	const char* const match = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : NULL;

	int total = 0;
	int models = 0;
	for ( int i = 1 ; i < tr.numModels; i++ ) {
		const model_t* mod = tr.models[i];

		if ( match && !Com_Filter( match, mod->name ) )
			continue;

		int lods = 1;
		for ( int j = 1 ; j < MD3_MAX_LODS ; j++ ) {
			if ( mod->md3[j] && mod->md3[j] != mod->md3[j-1] ) {
				lods++;
			}
		}
		ri.Printf( PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, lods, mod->name );
		total += mod->dataSize;
		models++;
	}
	ri.Printf( PRINT_ALL, "%8i : %i models found\n", total, models );

#if 0	// not working right with new hunk
	if ( tr.world ) {
		ri.Printf( PRINT_ALL, "%8i : %s\n", tr.world->dataSize, tr.world->name );
	}
#endif
}


///////////////////////////////////////////////////////////////


static const md3Tag_t* R_GetTag( const md3Header_t* mod, int frame, const char* tagName )
{
	if ( frame >= mod->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	const md3Tag_t* tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( int i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;
		}
	}

	return NULL;
}


int R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName )
{
	const model_t* model = R_GetModelByHandle( handle );
	if ( !model->md3[0] ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return qfalse;
	}

	const md3Tag_t* start = R_GetTag( model->md3[0], startFrame, tagName );
	const md3Tag_t* end = R_GetTag( model->md3[0], endFrame, tagName );
	if ( !start || !end ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return qfalse;
	}

	float backLerp = 1.0f - frac;

	for ( int i = 0; i < 3; ++i ) {
		tag->origin[i] = start->origin[i] * backLerp +  end->origin[i] * frac;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frac;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frac;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frac;
	}

	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );
	return qtrue;
}


void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs )
{
	const model_t* model = R_GetModelByHandle( handle );

	if ( model->bmodel ) {
		VectorCopy( model->bmodel->bounds[0], mins );
		VectorCopy( model->bmodel->bounds[1], maxs );
		return;
	}

	if ( !model->md3[0] ) {
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}

	const md3Header_t* header = model->md3[0];
	const md3Frame_t* frame = (const md3Frame_t*)( (byte *)header + header->ofsFrames );

	VectorCopy( frame->bounds[0], mins );
	VectorCopy( frame->bounds[1], maxs );
}

