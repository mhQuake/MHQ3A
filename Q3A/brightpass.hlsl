
struct BRIGHTPASSVERTEX
{
	float4 Position: POSITION0;
	float4 Color : COLOR0;
};


#ifdef VERTEXSHADER
BRIGHTPASSVERTEX VSMain (BRIGHTPASSVERTEX vs_in)
{
	return vs_in;
}
#endif


#ifdef PIXELSHADER
float4 PSMain (BRIGHTPASSVERTEX ps_in) : COLOR0
{
	// color was loaded as rgba so switch it back to bgra
	return ps_in.Color.bgra;
}
#endif

