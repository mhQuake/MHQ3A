
struct GENERICVERTEX
{
	float4 Position: POSITION0;
	float2 TexCoord: TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 MVPMatrix : register(VSREG_MVPMATRIX);
float4x4 ProjectionMatrix : register(VSREG_PROJECTIONMATRIX);
float4x4 WorldMatrix : register(VSREG_MODELVIEWMATRIX);
float4 moveOrigin : register(VSREG_MOVEORIGIN);
float4 RTInverseSize : register(VSREG_INVERSERT);

float4 GetPositionWithHalfPixelCorrection (float4x4 mvp, float4 inPosition)
{
	// correct the dreaded half-texel offset
	float4 outPosition = mul (mvp, inPosition);
	outPosition.xy += RTInverseSize.xy * outPosition.w;
	return outPosition;
}

GENERICVERTEX VSMain (GENERICVERTEX vs_in)
{
	GENERICVERTEX vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, vs_in.Position + moveOrigin);
	vs_out.Position.z = vs_out.Position.w;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
texture tmu0Texture : register(t0);
sampler tmu0Sampler : register(s0) = sampler_state { texture = <tmu0Texture>; };

float4 identityLight : register(PSREG_IDENTITYLIGHT);

float4 PSMain (GENERICVERTEX ps_in) : COLOR0
{
	// the textures were loaded as RGBA so here we must swizzle them back to BGRA
	return tex2D (tmu0Sampler, ps_in.TexCoord).bgra * identityLight;
}
#endif

