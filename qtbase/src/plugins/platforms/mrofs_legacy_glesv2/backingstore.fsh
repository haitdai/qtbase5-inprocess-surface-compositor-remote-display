#ifdef GL_IMG_texture_stream2
#extension GL_IMG_texture_stream2 : enable
uniform samplerStreamIMG sTexture;
#elif defined(GL_OES_EGL_image_external)
#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES             sTexture;
#endif

varying mediump vec2                   TexCoord;
uniform mediump vec4                   bkColor;
uniform highp vec4                     colorKey;
const mediump float factor = 0.001;
void main(void)
{
#ifdef GL_IMG_texture_stream2
	mediump vec4 c = textureStreamIMG(sTexture, TexCoord);
#elif defined(GL_OES_EGL_image_external)
	mediump vec4 c = texture2D(sTexture, TexCoord);
#endif
	//if (all(lessThan(c, ckUpper)) && all(greaterThan(c, ckLower)))
	mediump vec4 delta = abs(c - colorKey);
	/*
	DHT20140924, no vsync, for Dubhe Frontend,
	maximized FPS is 60+ with colorkey,
	maximized FPS is 70+ without colorkey,
	NOTE:
	1, setting c.a to zero is faster than calling discard
	2, compare rgb independently is faster than calling all+lessThan/greaterThan
	*/
	if(delta.r < factor && delta.g < factor && delta.b < factor)
		c.a = 0.0;
	gl_FragColor = c;
}

