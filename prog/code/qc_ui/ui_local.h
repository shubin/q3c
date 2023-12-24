#ifndef UI_LOCAL_H
#define UI_LOCAL_H

#include "../renderercommon/tr_types.h"

#define UI_API_VERSION	4

void UI_Init( void );
void UI_Shutdown( void );
void UI_KeyEvent( int key, int down );
void UI_MouseEvent( int dx, int dy );
void UI_Refresh( int realtime );
qboolean UI_IsFullscreen( void );
void UI_SetActiveMenu( uiMenuCommand_t menu );
qboolean UI_ConsoleCommand( int realTime );
void UI_DrawConnectScreen( qboolean overlay );

extern glconfig_t g_glConfig;

namespace Rml {

#if defined( RMLUI_CORE_CORE_H )

#endif // RMLUI_CORE_CORE_H

#if defined( RMLUI_CORE_RENDERINTERFACE_H )

class QRenderInterface : public RenderInterface {
public:
#pragma pack(push,1)
	struct QScissor {
		int x, y, w, h;
	};
#pragma pack(pop)

	struct QRenderCmd {
		int				firstIndex, numIndices;
		qhandle_t		shader;
		QScissor		scissor;
	};


	QRenderInterface();
	virtual ~QRenderInterface();
	virtual void RenderGeometry( Vertex *vertices, int num_vertices, int *indices, int num_indices, TextureHandle texture, const Vector2f &translation );
	virtual void EnableScissorRegion( bool enable );
	virtual void SetScissorRegion( int x, int y, int width, int height );
	virtual bool LoadTexture( TextureHandle &texture_handle, Vector2i &texture_dimensions, const String &source );
	virtual bool GenerateTexture( TextureHandle &texture_handle, const byte *source, const Vector2i &source_dimensions );
	virtual void ReleaseTexture( TextureHandle texture );
	virtual void SetTransform( const Matrix4f *transform );

	bool		mScissorEnabled;
	QScissor	mWholeScreen, mScissor;
	Vertex		*mVertices, *pCurrentVertex;
	uint32_t	*mIndices, *pCurrentIndex;
	QRenderCmd	*mCommands, *pCurrentCommand;

	void Initialize( int width, int height );
	void Shutdown();
	virtual void Flush();
};

#endif // RMLUI_CORE_RENDERINTERFACE_H

#if defined( RMLUI_CORE_FILEINTERFACE_H )

class QFileInterface : public FileInterface {
public:
	QFileInterface();
	virtual ~QFileInterface();
	virtual FileHandle Open( const String &path );
	virtual void Close( FileHandle file );
	virtual size_t Read( void *buffer, size_t size, FileHandle file );
	virtual bool Seek( FileHandle file, long offset, int origin );
	virtual size_t Tell( FileHandle file );
	virtual size_t Length( FileHandle file );
	virtual bool LoadFile( const String &path, String &out_data );
};

#endif // RMLUI_CORE_FILEINTERFACE_H

#if defined( RMLUI_CORE_SYSTEMINTERFACE_H )

class QSystemInterface : public SystemInterface {
public:
	QSystemInterface();
	virtual ~QSystemInterface();
	virtual double GetElapsedTime();
	virtual bool LogMessage( Log::Type type, const String &message );
	/*
	virtual int TranslateString( String &translated, const String &input );
	virtual void JoinPath( String &translated_path, const String &document_path, const String &path );
	virtual void SetMouseCursor( const String &cursor_name );
	virtual void SetClipboardText( const String &text );
	virtual void GetClipboardText( String &text );
	*/
};

#endif // RMLUI_CORE_SYSTEMINTERFACE_H

}

extern "C" {

//
// ui_syscalls.c
//
void			trap_Print( const char *string );
void			trap_Error(const char *string) __attribute__((noreturn));
int				trap_Milliseconds( void );
void			trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void			trap_Cvar_Update( vmCvar_t *vmCvar );
void			trap_Cvar_Set( const char *var_name, const char *value );
float			trap_Cvar_VariableValue( const char *var_name );
void			trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void			trap_Cvar_SetValue( const char *var_name, float value );
void			trap_Cvar_Reset( const char *name );
void			trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
void			trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
int				trap_Argc( void );
void			trap_Argv( int n, char *buffer, int bufferLength );
void			trap_Cmd_ExecuteText( int exec_when, const char *text );	// don't use EXEC_NOW!
int				trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void			trap_FS_Read( void *buffer, int len, fileHandle_t f );
void			trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void			trap_FS_FCloseFile( fileHandle_t f );
int				trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
int				trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
qhandle_t		trap_R_RegisterModel( const char *name );
qhandle_t		trap_R_RegisterSkin( const char *name );
qhandle_t		trap_R_RegisterShaderNoMip( const char *name );
void			trap_R_ClearScene( void );
void			trap_R_AddRefEntityToScene( const refEntity_t *re );
void			trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void			trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void			trap_R_RenderScene( const refdef_t *fd );
void			trap_R_SetColor( const float *rgba );
void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void			trap_R_DrawTriangle( float x0, float y0, float x1, float y1, float x2, float y2, float s0, float t0, float s1, float t1, float s2, float t2, qhandle_t hShader );
void			trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
void			trap_UpdateScreen( void );
int				trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );
void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
sfxHandle_t		trap_S_RegisterSound( const char *sample, qboolean compressed );
void			trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
void			trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
void			trap_Key_SetBinding( int keynum, const char *binding );
qboolean		trap_Key_IsDown( int keynum );
qboolean		trap_Key_GetOverstrikeMode( void );
void			trap_Key_SetOverstrikeMode( qboolean state );
void			trap_Key_ClearStates( void );
int				trap_Key_GetCatcher( void );
void			trap_Key_SetCatcher( int catcher );
void			trap_GetClipboardData( char *buf, int bufsize );
void			trap_GetClientState( uiClientState_t *state );
void			trap_GetGlconfig( glconfig_t *glconfig );
int				trap_GetConfigString( int index, char* buff, int buffsize );
int				trap_LAN_GetServerCount( int source );
void			trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
void			trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
int				trap_LAN_GetServerPing( int source, int n );
int				trap_LAN_GetPingQueueCount( void );
void			trap_LAN_ClearPing( int n );
void			trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
void			trap_LAN_GetPingInfo( int n, char *buf, int buflen );
void			trap_LAN_LoadCachedServers( void );
void			trap_LAN_SaveCachedServers( void );
void			trap_LAN_MarkServerVisible(int source, int n, qboolean visible);
int				trap_LAN_ServerIsVisible( int source, int n);
qboolean		trap_LAN_UpdateVisiblePings( int source );
int				trap_LAN_AddServer(int source, const char *name, const char *addr);
void			trap_LAN_RemoveServer(int source, const char *addr);
void			trap_LAN_ResetPings(int n);
int				trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
int				trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 );
int				trap_MemoryRemaining( void );
void			trap_GetCDKey( char *buf, int buflen );
void			trap_SetCDKey( char *buf );
void			trap_R_RegisterFont(const char *pFontname, int pointSize, fontInfo_t *font);
void			trap_S_StopBackgroundTrack( void );
void			trap_S_StartBackgroundTrack( const char *intro, const char *loop);
int				trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status		trap_CIN_StopCinematic(int handle);
e_status		trap_CIN_RunCinematic (int handle);
void			trap_CIN_DrawCinematic (int handle);
void			trap_CIN_SetExtents (int handle, int x, int y, int w, int h);
int				trap_RealTime(qtime_t *qtime);
void			trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );
qboolean		trap_VerifyCDKey( const char *key, const char *chksum);

void			trap_SetPbClStatus( int status );

// QC extensions

qhandle_t		trap_R_CreateTextureFromMemory( int width, int height, const void *rgba );
void			trap_R_GetShaderImageDimensions( qhandle_t shader, int nstage, int nimage, int *width, int *height );

// CNQ3 extensions
void			trap_NK_Upload( void *vertexes, int numVertexBytes, void *indexes, int numIndexBytes );
void			trap_NK_Draw( int firstIndex, int numIndexes, qhandle_t shader, const int *scissorRect );

}

#endif // UI_LOCAL_H
