attribute highp   vec4  inVertex;
uniform mediump mat4    orientationMatrix;
attribute mediump vec2  inTexCoord;
varying mediump vec2    TexCoord;
void main()
{
	gl_Position = orientationMatrix * inVertex;
	TexCoord = inTexCoord;
}
