
// shader registers
#define STR_EXPAND(tok) #tok
#define DEFINE_SHADER_REGISTER(n) {#n, "c"STR_EXPAND (n)}

// vertex shader regs
#define VSREG_MVPMATRIX				0
#define VSREG_PROJECTIONMATRIX		4
#define VSREG_MODELVIEWMATRIX		8
#define VSREG_SKYMATRIX				12
#define VSREG_MOVEORIGIN			16
#define VSREG_TMODTURBTIME			17
#define VSREG_TCGENVEC0				18
#define VSREG_TCGENVEC1				19
#define VSREG_INVERSERT				20

// pixel shader regs
#define PSREG_TMODMATRIX0			0	// this is a 2x2 matrix stuffed into a single float4
#define PSREG_TMODMATRIX1			1	// this is a 2x2 matrix stuffed into a single float4
#define PSREG_TMODMATRIX2			2	// this is a 2x2 matrix stuffed into a single float4
#define PSREG_TMODMATRIX3			3	// this is a 2x2 matrix stuffed into a single float4
#define PSREG_TRANSLATE0			4
#define PSREG_TRANSLATE1			5
#define PSREG_TRANSLATE2			6
#define PSREG_TRANSLATE3			7
#define PSREG_TMODTURBAMP			8
#define PSREG_FOGDISTANCE			9
#define PSREG_FOGDEPTH				10
#define PSREG_FOGEYET				11
#define PSREG_FOGEYEOUTSIDE			12
#define PSREG_VIEWORIGIN			13
#define PSREG_DLRADIUS				14
#define PSREG_DLORIGIN				15
#define PSREG_DLCOLOUR				16
#define PSREG_IDENTITYLIGHT			17
#define PSREG_GAMMA					18
#define PSREG_BRIGHTNESS			19

typedef struct program_s
{
	IDirect3DVertexDeclaration9 *vd;
	IDirect3DVertexShader9 *vs;
	IDirect3DPixelShader9 *ps;
} program_t;

extern program_t r_genericProgram;
extern program_t r_skyboxProgram;
extern program_t r_dlightProgram;

extern IDirect3DVertexDeclaration9 *r_stageDeclaration;

#define VDECL(Stream, Offset, Type, Usage, UsageIndex) {Stream, Offset, Type, D3DDECLMETHOD_DEFAULT, Usage, UsageIndex}

void R_CreateHLSLProgramFromShader (shader_t *sh);
void GL_InitPrograms (void);
void GL_ShutdownPrograms (void);

void RB_SetVertexDeclaration (IDirect3DVertexDeclaration9 *vd);
void RB_SetVertexShader (IDirect3DVertexShader9 *vs);
void RB_SetPixelShader (IDirect3DPixelShader9 *ps);
void RB_SetProgram (program_t *prog);


typedef struct genericvert_s
{
	float xyz[3];
	DWORD color;
	float uv[2];
} genericvert_t;


typedef struct stagestaticvert_s
{
	float xyz[3];
	float normal[3];
	float st[2];
	float lm[2];
} stagestaticvert_t;


typedef struct stagedynamicvert_s
{
	union
	{
		DWORD color;
		byte rgba[4];
	};
} stagedynamicvert_t;


// most vertex data is written into this now rather than written to tess and copied
extern stagestaticvert_t r_stagestaticverts[];
extern stagedynamicvert_t r_stagedynamicverts[];


void RB_SetupTCModScroll (int modstage, const float scrollSpeed[2]);
void RB_SetupTCModScale (int modstage, const float scale[2]);
void RB_SetupTCModStretch (int modstage, const waveForm_t *wf);
void RB_SetupTCModRotate (int modstage, float rotateSpeed);
void RB_SetupTCModTransform (int modstage, texModInfo_t *tmi);
void RB_SetupTCGenFog (void);

void RB_DeformTessGeometry (void);

void RB_CalcModulateColorsByFog (void);
void RB_CalcModulateAlphasByFog (void);
void RB_CalcModulateRGBAsByFog (void);

void RB_CalcWaveAlpha (const waveForm_t *wf, unsigned char *dstColors);
void RB_CalcWaveColor (const waveForm_t *wf, unsigned char *dstColors);

void RB_CalcSpecularAlpha (void);
void RB_CalcDiffuseColor (void);
void RB_CalcPortalAlpha (void);


typedef struct fogparms_s
{
	vec4_t DistanceVector;
	vec4_t DepthVector;
	float eyeT;
	qboolean eyeOutside;
} fogparms_t;


void RB_CalcFogParms (fog_t *fog, fogparms_t *parms);
void RB_CalcFogTexCoord (fogparms_t *parms, float *v, float *st);

extern IDirect3DVertexBuffer9 *r_stageStaticVertexBuffer;
extern IDirect3DVertexBuffer9 *r_stageDynamicVertexBuffer;
extern IDirect3DIndexBuffer9 *r_stageIndexBuffer;

// these are our buffer sizes
#define	BUFFER_MAX_VERTEXES	0x10000
#define	BUFFER_MAX_INDEXES	0x100000

void R_ProgramOnLostDevice (void);
void R_ProgramOnResetDevice (void);


