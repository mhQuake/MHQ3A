

struct PS_SHADEVERTEX {
	float4 Position: POSITION0;
	float4 Color : COLOR0;

#if defined (TCGEN_ENVIRONMENT_MAPPED)
	float3 TexCoord: TEXCOORD0;
	float3 Normal: TEXCOORD1;
#elif defined (TCGEN_SKY)
	float3 TexCoord: TEXCOORD0;
	float2 SkyCoord: TEXCOORD1;
#elif defined (TCGEN_FOG)
	float3 TexCoord: TEXCOORD0;
#else
	float2 TexCoord: TEXCOORD0;
#endif

#ifdef TMOD_TURB0
	float2 TurbCoord0: TEXCOORD3;
#endif

#ifdef TMOD_TURB1
	float2 TurbCoord1: TEXCOORD4;
#endif

#ifdef TMOD_TURB2
	float2 TurbCoord2: TEXCOORD5;
#endif

#ifdef TMOD_TURB3
	float2 TurbCoord3: TEXCOORD6;
#endif
};


struct GENERICVERTEX {
	float4 Position: POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord: TEXCOORD0;
};


struct SKYBOXVERTEX {
	float4 Position: POSITION0;
	float2 TexCoord: TEXCOORD0;
};


struct DLIGHTVERTEX {
	float4 Position: POSITION0;
	float3 TexCoord: TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 MVPMatrix : register(VSREG_MVPMATRIX);
float4x4 ProjectionMatrix : register(VSREG_PROJECTIONMATRIX);
float4x4 WorldMatrix : register(VSREG_MODELVIEWMATRIX);
float4x4 SkyMatrix : register(VSREG_SKYMATRIX);
float4 RTInverseSize : register(VSREG_INVERSERT);
float4 moveOrigin : register(VSREG_MOVEORIGIN);

float4 GetPositionWithHalfPixelCorrection (float4x4 mvp, float4 inPosition)
{
	// correct the dreaded half-texel offset
	float4 outPosition = mul (mvp, inPosition);
	outPosition.xy += RTInverseSize.xy * outPosition.w;
	return outPosition;
}

// ============================================================================================
// TMOD_TURB
// ============================================================================================
// matches value in gcc v2 math.h
#define M_PI 3.14159265358979323846

float4 tmTurbTime : register(VSREG_TMODTURBTIME);

float2 RB_CalcTurbTexCoords (float4 Position, float time)
{
	return float2 (
		((Position.x + Position.z) * 1.0f / 128.0f * 0.125f + time) * (M_PI * 2.0f),
		(Position.y * 1.0f / 128.0f * 0.125f + time) * (M_PI * 2.0f)
	);
}


// ============================================================================================
// TCGEN_VECTOR
// ============================================================================================
float3 tcGenVector0 : register(VSREG_TCGENVEC0);
float3 tcGenVector1 : register(VSREG_TCGENVEC1);

float2 RB_CalcVectorTexcoords (float3 Position)
{
	return float2 (
		dot (Position, tcGenVector0),
		dot (Position, tcGenVector1)
	);
}


struct VS_SHADEVERTEX {
	float4 Position: POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord: TEXCOORD0;
	float2 Lightmap: TEXCOORD1;
	float3 Normal: NORMAL0;
};


PS_SHADEVERTEX VSShade (VS_SHADEVERTEX vs_in)
{
	PS_SHADEVERTEX vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, vs_in.Position);
	vs_out.Color = vs_in.Color.bgra; 	// color was loaded as rgba so switch it back to bgra

#if defined (TCGEN_ENVIRONMENT_MAPPED)
	vs_out.TexCoord = vs_in.Position.xyz;
	vs_out.Normal = vs_in.Normal;
#elif defined (TCGEN_SKY)
	vs_out.Position.z = vs_out.Position.w;
	vs_out.TexCoord = mul (SkyMatrix, vs_in.Position).xyz;
	vs_out.SkyCoord = vs_in.TexCoord;
#elif defined (TCGEN_LIGHTMAP)
	vs_out.TexCoord = vs_in.Lightmap;
#elif defined (TCGEN_VECTOR)
	vs_out.TexCoord = RB_CalcVectorTexcoords (vs_in.Position.xyz);
#elif defined (TCGEN_IDENTITY)
	vs_out.TexCoord = float2 (0, 0);
#elif defined (TCGEN_FOG)
	vs_out.TexCoord = vs_in.Position.xyz;
#else
	vs_out.TexCoord = vs_in.TexCoord;
#endif

#ifdef TMOD_TURB0
	vs_out.TurbCoord0 = RB_CalcTurbTexCoords (vs_in.Position, tmTurbTime.x);
#endif

#ifdef TMOD_TURB1
	vs_out.TurbCoord1 = RB_CalcTurbTexCoords (vs_in.Position, tmTurbTime.y);
#endif

#ifdef TMOD_TURB2
	vs_out.TurbCoord2 = RB_CalcTurbTexCoords (vs_in.Position, tmTurbTime.z);
#endif

#ifdef TMOD_TURB3
	vs_out.TurbCoord3 = RB_CalcTurbTexCoords (vs_in.Position, tmTurbTime.w);
#endif

	return vs_out;
}


GENERICVERTEX VSGeneric (float4 Position: POSITION0, float4 Color : COLOR0, float2 TexCoord: TEXCOORD0)
{
	GENERICVERTEX vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, Position);
	vs_out.Color = Color.bgra; 	// color was loaded as rgba so switch it back to bgra
	vs_out.TexCoord = TexCoord;

	return vs_out;
}


SKYBOXVERTEX VSSkybox (float4 Position: POSITION0, float2 TexCoord: TEXCOORD0)
{
	SKYBOXVERTEX vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, Position + moveOrigin);
	vs_out.Position.z = vs_out.Position.w;
	vs_out.TexCoord = TexCoord;

	return vs_out;
}


DLIGHTVERTEX VSDynamicLight (float4 Position: POSITION0)
{
	DLIGHTVERTEX vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, Position);
	vs_out.TexCoord = Position.xyz;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
// allow this to be reusable across multiple shader types
#ifdef TEXTURESTAGE
texture tmu0Texture : register(TEXTURESTAGE);
#else
texture tmu0Texture : register(t0);
#endif

#ifdef SAMPLERSTAGE
sampler tmu0Sampler : register(SAMPLERSTAGE) = sampler_state { texture = <tmu0Texture>; };
#else
sampler tmu0Sampler : register(s0) = sampler_state { texture = <tmu0Texture>; };
#endif

float4 tmMatrix0 : register(PSREG_TMODMATRIX0);
float4 tmMatrix1 : register(PSREG_TMODMATRIX1);
float4 tmMatrix2 : register(PSREG_TMODMATRIX2);
float4 tmMatrix3 : register(PSREG_TMODMATRIX3);

float2 tmTranslate0 : register(PSREG_TRANSLATE0);
float2 tmTranslate1 : register(PSREG_TRANSLATE1);
float2 tmTranslate2 : register(PSREG_TRANSLATE2);
float2 tmTranslate3 : register(PSREG_TRANSLATE3);

float4 tmTurbAmp : register(PSREG_TMODTURBAMP);


// ============================================================================================
// TCGEN_FOG
// ============================================================================================
float4 fogDistanceVector : register(PSREG_FOGDISTANCE);
float4 fogDepthVector : register(PSREG_FOGDEPTH);
float eyeT : register(PSREG_FOGEYET);
float eyeOutside : register(PSREG_FOGEYEOUTSIDE);

float2 RB_CalcFogTexcoords (float3 Position)
{
	float s = dot (Position, fogDistanceVector.xyz) + fogDistanceVector.w;
	float t = dot (Position, fogDepthVector.xyz) + fogDepthVector.w;

	if (eyeOutside > 0)
	{
		if (t < 1.0f)
		{
			// point is outside, so no fogging
			t = 1.0f / 32.0f;
		}
		else
		{
			// cut the distance at the fog plane
			t = 1.0f / 32.0f + 30.0f / 32.0f * t / (t - eyeT);
		}
	}
	else
	{
		if (t < 0)
		{
			// point is outside, so no fogging
			t = 1.0f / 32.0f;
		}
		else t = 31.0f / 32.0f;
	}

	return float2 (s, t);
}


// ============================================================================================
// TCGEN_TEXTURE with shader->isSky
// ============================================================================================
float2 RB_CalcCloudTexCoords (float3 skyVec, float radiusWorld, float heightCloud)
{
	float3 sqrSkyVec = skyVec * skyVec;
	float3 v1 = sqrSkyVec * radiusWorld * heightCloud * 2.0;
	float3 v2 = sqrSkyVec * heightCloud * heightCloud;

	float p = (1.0 / (2.0 * dot (skyVec, skyVec))) *
		(-2.0 * skyVec.z * radiusWorld +
		2.0 * sqrt (sqrSkyVec.z * radiusWorld * radiusWorld +
		v1.x + v1.y + v1.z +
		v2.x + v2.y + v2.z));

	float3 v = (skyVec * p) + float3 (0.0, 0.0, radiusWorld);
	float2 st = normalize (v).xy;

	return acos (st);
}


// ============================================================================================
// TCGEN_ENVIRONMENT_MAPPED
// ============================================================================================
float3 viewOrigin : register(PSREG_VIEWORIGIN);

float2 RB_CalcEnvironmentTexCoords (float3 Position, float3 Normal)
{
	float3 viewer = normalize (viewOrigin - Position);
	float2 reflected = reflect (viewer, Normal).yz;

	return float2 (0.5f + reflected.x * 0.5f, 0.5f - reflected.y * 0.5f);
}


// ============================================================================================
// MAIN
// ============================================================================================
float4 r_gamma : register(PSREG_GAMMA);
float4 r_brightness : register(PSREG_BRIGHTNESS);

float4 GetGammaAndBrightness (float4 color)
{
	return float4 (pow (max (color.rgb, 0.0f), r_gamma.x) * r_brightness.x, color.a);
}


float3 HUEtoRGB (in float H)
{
	float R = abs (H * 6 - 3) - 1;
	float G = 2 - abs (H * 6 - 2);
	float B = 2 - abs (H * 6 - 4);
	return saturate (float3 (R, G, B));
}


float3 HSLtoRGB (in float3 HSL)
{
	float3 RGB = HUEtoRGB (HSL.x);
		float C = (1 - abs (2 * HSL.z - 1)) * HSL.y;
	return (RGB - 0.5) * C + HSL.z;
}


float Epsilon = 1e-10;

float3 RGBtoHCV (in float3 RGB)
{
	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (RGB.g < RGB.b) ? float4 (RGB.bg, -1.0, 2.0 / 3.0) : float4 (RGB.gb, 0.0, -1.0 / 3.0);
		float4 Q = (RGB.r < P.x) ? float4 (P.xyw, RGB.r) : float4 (RGB.r, P.yzx);
		float C = Q.x - min (Q.w, Q.y);
	float H = abs ((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
	return float3 (H, C, Q.x);
}


float3 RGBtoHSL (in float3 RGB)
{
	float3 HCV = RGBtoHCV (RGB);
		float L = HCV.z - HCV.y * 0.5;
	float S = HCV.y / (1 - abs (L * 2 - 1) + Epsilon);
	return float3 (HCV.x, S, L);
}


float4 Desaturate (float4 Colour)
{
	float desaturation = 0.5f;

	// hack hack hack - because light can overbright we scale it down, then apply the desaturation, then bring it back up again
	// otherwise we get clamping issues in the conversion funcs if any of the channels are above 1
	return float4 (HSLtoRGB (RGBtoHSL (Colour.rgb * 0.1f) * float3 (1.0f, desaturation, 1.0f)) * 10.0f, Colour.a);
}


float4 PSShade (PS_SHADEVERTEX ps_in) : COLOR0
{
	// read the initial texcoord
#if defined (TCGEN_SKY)
	float2 st = RB_CalcCloudTexCoords (ps_in.TexCoord, ps_in.SkyCoord.x, ps_in.SkyCoord.y);
#elif defined (TCGEN_ENVIRONMENT_MAPPED)
	float2 st = RB_CalcEnvironmentTexCoords (ps_in.TexCoord, ps_in.Normal);
#elif defined (TCGEN_FOG)
	float2 st = RB_CalcFogTexcoords (ps_in.TexCoord);
#else
	float2 st = ps_in.TexCoord;
#endif

	// apply the TMODs in order; a shader can have up to 4 TMODs, one from each set
	// all tmods aside from turb are generalized as a 3x2 (or 2x3) matrix multiplication
	// ideally so should turb be (a stretch and squeeze effect) so that we can generalize everything
	// and precalc them as one stage only on the CPU
	// tmods MUST stay in the pixel shader because they could be applied to a tcgen that's evaluated
	// in the pixel shader.  in theory we could 
#if defined (TMOD_TURB0)
	st += sin (ps_in.TurbCoord0) * tmTurbAmp.x;
#elif defined (TMOD_TRANSFORM0)
	st = mul (float2x2 (tmMatrix0.xzyw), st) + tmTranslate0;
#endif

#if defined (TMOD_TURB1)
	st += sin (ps_in.TurbCoord1) * tmTurbAmp.y;
#elif defined (TMOD_TRANSFORM1)
	st = mul (float2x2 (tmMatrix1.xzyw), st) + tmTranslate1;
#endif

#if defined (TMOD_TURB2)
	st += sin (ps_in.TurbCoord2) * tmTurbAmp.z;
#elif defined TMOD_TRANSFORM2
	st = mul (float2x2 (tmMatrix2.xzyw), st) + tmTranslate2;
#endif

#if defined (TMOD_TURB3)
	st += sin (ps_in.TurbCoord3) * tmTurbAmp.w;
#elif defined (TMOD_TRANSFORM3)
	st = mul (float2x2 (tmMatrix3.xzyw), st) + tmTranslate3;
#endif

	// read the texture with the final generated and modified texcoord;
	// the textures were loaded as RGBA so here we must swizzle them back to BGRA
#if defined (TCGEN_LIGHTMAP)
	// lighting is not gamma-adjusted but does scale for overbrighting (alpha is not scaled)
	return Desaturate (tex2D (tmu0Sampler, st).bgra) * ps_in.Color * float4 (2.0f, 2.0f, 2.0f, 1.0f);
#else
	return GetGammaAndBrightness (tex2D (tmu0Sampler, st).bgra) * ps_in.Color;
#endif
}


float4 PSGeneric (GENERICVERTEX ps_in) : COLOR0
{
	// the textures were loaded as RGBA so here we must swizzle them back to BGRA
	return GetGammaAndBrightness (tex2D (tmu0Sampler, ps_in.TexCoord).bgra) * ps_in.Color;
}


float4 identityLight : register(PSREG_IDENTITYLIGHT);

float4 PSSkybox (SKYBOXVERTEX ps_in) : COLOR0
{
	// the textures were loaded as RGBA so here we must swizzle them back to BGRA
	return GetGammaAndBrightness (tex2D (tmu0Sampler, ps_in.TexCoord).bgra) * identityLight;
}


float radius : register(PSREG_DLRADIUS);
float3 origin : register(PSREG_DLORIGIN);
float3 colour : register(PSREG_DLCOLOUR);

float4 PSDynamicLight (DLIGHTVERTEX ps_in) : COLOR0
{
	float dist = radius - length (ps_in.TexCoord - origin);

	clip (dist);

	// the original engine did inverse-square falloff; this does linear falloff but properly
	return GetGammaAndBrightness (float4 (colour * (dist / radius), 0));
}
#endif

