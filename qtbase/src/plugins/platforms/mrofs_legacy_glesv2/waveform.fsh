#ifdef GL_IMG_texture_stream2
#extension GL_IMG_texture_stream2 : enable
uniform samplerStreamIMG sTexture;
#elif defined(GL_OES_EGL_image_external)
#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES             sTexture;
#endif

varying mediump vec2                   TexCoord;
void main(void)
{
#ifdef GL_IMG_texture_stream2
	gl_FragColor = textureStreamIMG(sTexture, TexCoord);
#elif defined(GL_OES_EGL_image_external)
	gl_FragColor = texture2D(sTexture, TexCoord);
#endif
}
