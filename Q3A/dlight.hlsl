
struct PS_DLIGHT
{
	float4 Position: POSITION0;
	float3 TexCoord: TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 MVPMatrix : register(VSREG_MVPMATRIX);
float4x4 ProjectionMatrix : register(VSREG_PROJECTIONMATRIX);
float4x4 WorldMatrix : register(VSREG_MODELVIEWMATRIX);
float4 RTInverseSize : register(VSREG_INVERSERT);

float4 GetPositionWithHalfPixelCorrection (float4x4 mvp, float4 inPosition)
{
	// correct the dreaded half-texel offset
	float4 outPosition = mul (mvp, inPosition);
	outPosition.xy += RTInverseSize.xy * outPosition.w;
	return outPosition;
}

PS_DLIGHT VSMain (float4 Position: POSITION0)
{
	PS_DLIGHT vs_out;

	vs_out.Position = GetPositionWithHalfPixelCorrection (MVPMatrix, Position);
	vs_out.TexCoord = Position.xyz;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float radius : register(PSREG_DLRADIUS);
float3 origin : register(PSREG_DLORIGIN);
float3 colour : register(PSREG_DLCOLOUR);

float4 PSMain (PS_DLIGHT ps_in) : COLOR0
{
	float dist = radius - length (ps_in.TexCoord - origin);

	clip (dist);

	// the original engine did inverse-square falloff; this does linear falloff but properly
	return float4 (colour * (dist / radius), 0);
}
#endif

