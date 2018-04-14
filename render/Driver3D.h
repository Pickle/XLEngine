#ifndef DRIVER3D_H_INCLUDED
#define DRIVER3D_H_INCLUDED

#include "SDL.h"

#if defined(USE_GLEW)
#include <GL/glew.h>
#endif /* defined(USE_GLEW) */

#define GL_GLEXT_PROTOTYPES
#if defined(USE_GLES)
    #define glFogi          glFogf
    #define glClearDepth    glClearDepthf
    #define glFrustum       glFrustumf
    #define glMapBuffer     glMapBufferOES
    #define glUnmapBuffer   glUnmapBufferOES
    #define GL_BGRA         GL_BGRA_EXT
    #define GL_RGBA8        GL_RGBA
    #define GL_CLAMP        GL_CLAMP_TO_EDGE
    #define GL_WRITE_ONLY   GL_WRITE_ONLY_OES
    #define GL_UNSIGNED_INT_8_8_8_8_REV  GL_UNSIGNED_BYTE

    #include "SDL_opengles.h"
#else
    #include "SDL_opengl.h"
#endif

#endif // DRIVER3D_H_INCLUDED
