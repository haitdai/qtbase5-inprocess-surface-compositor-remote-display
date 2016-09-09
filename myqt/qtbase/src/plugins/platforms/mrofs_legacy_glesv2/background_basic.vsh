attribute highp   vec4  inVertex;
uniform mediump mat4    orientationMatrix;
void main()
{
	gl_Position = orientationMatrix * inVertex;
}
