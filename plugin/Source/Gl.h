/* SeriousHL - OpenGL straight from opengl32.dll: the engine's own GL pointers aren't exported (and its GL types, an
 * enum GLenum, clash with the Windows SDK's GL/gl.h), so the functions are taken here and the engine's types and enums
 * used. Only on the render thread, in the engine's GL context, with its state pushed and popped around. */
#ifndef SERIOUSHL_GL_H
#define SERIOUSHL_GL_H

#include "StdH.h"

namespace gl {

typedef void (__stdcall *F_glOrtho)(double, double, double, double, double, double);
extern F_glOrtho xglOrtho;
typedef void (__stdcall *F_glVertex3f)(GLfloat, GLfloat, GLfloat);
extern F_glVertex3f xglVertex3f;
typedef void (__stdcall *F_glVertex2f)(GLfloat, GLfloat);
extern F_glVertex2f xglVertex2f;
typedef void (__stdcall *F_glBegin)(GLenum);
extern F_glBegin xglBegin;
typedef void (__stdcall *F_glEnd)(void);
extern F_glEnd xglEnd;
typedef void (__stdcall *F_glVertex3fv)(const GLfloat *);
extern F_glVertex3fv xglVertex3fv;
typedef void (__stdcall *F_glTexCoord2f)(GLfloat, GLfloat);
extern F_glTexCoord2f xglTexCoord2f;
typedef void (__stdcall *F_glColor4f)(GLfloat, GLfloat, GLfloat, GLfloat);
extern F_glColor4f xglColor4f;
typedef void (__stdcall *F_glBindTexture)(GLenum, GLuint);
extern F_glBindTexture xglBindTexture;
typedef void (__stdcall *F_glGenTextures)(GLsizei, GLuint *);
extern F_glGenTextures xglGenTextures;
typedef void (__stdcall *F_glTexParameteri)(GLenum, GLenum, GLint);
extern F_glTexParameteri xglTexParameteri;
typedef void (__stdcall *F_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
extern F_glTexImage2D xglTexImage2D;
typedef void (__stdcall *F_glMatrixMode)(GLenum);
extern F_glMatrixMode xglMatrixMode;
typedef void (__stdcall *F_glPushMatrix)(void);
extern F_glPushMatrix xglPushMatrix;
typedef void (__stdcall *F_glPopMatrix)(void);
extern F_glPopMatrix xglPopMatrix;
typedef void (__stdcall *F_glLoadIdentity)(void);
extern F_glLoadIdentity xglLoadIdentity;
typedef void (__stdcall *F_glFrustum)(double, double, double, double, double, double);
extern F_glFrustum xglFrustum;
typedef void (__stdcall *F_glViewport)(GLint, GLint, GLsizei, GLsizei);
extern F_glViewport xglViewport;
typedef void (__stdcall *F_glDisable)(GLenum);
extern F_glDisable xglDisable;
typedef void (__stdcall *F_glEnable)(GLenum);
extern F_glEnable xglEnable;
typedef void (__stdcall *F_glDepthFunc)(GLenum);
extern F_glDepthFunc xglDepthFunc;
typedef void (__stdcall *F_glDepthRange)(double, double);
extern F_glDepthRange xglDepthRange;
typedef void (__stdcall *F_glDepthMask)(GLboolean);
extern F_glDepthMask xglDepthMask;
typedef void (__stdcall *F_glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
extern F_glColorMask xglColorMask;
typedef void (__stdcall *F_glPolygonMode)(GLenum, GLenum);
extern F_glPolygonMode xglPolygonMode;
typedef void (__stdcall *F_glShadeModel)(GLenum);
extern F_glShadeModel xglShadeModel;
typedef void (__stdcall *F_glTexEnvi)(GLenum, GLenum, GLint);
extern F_glTexEnvi xglTexEnvi;
typedef void (__stdcall *F_glAlphaFunc)(GLenum, GLfloat);
extern F_glAlphaFunc xglAlphaFunc;
typedef void (__stdcall *F_glBlendFunc)(GLenum, GLenum);
extern F_glBlendFunc xglBlendFunc;
typedef void (__stdcall *F_glPushAttrib)(GLbitfield);
extern F_glPushAttrib xglPushAttrib;
typedef void (__stdcall *F_glPopAttrib)(void);
extern F_glPopAttrib xglPopAttrib;
typedef void (__stdcall *F_glPushClientAttrib)(GLbitfield);
extern F_glPushClientAttrib xglPushClientAttrib;
typedef void (__stdcall *F_glPopClientAttrib)(void);
extern F_glPopClientAttrib xglPopClientAttrib;
typedef void (__stdcall *F_glGetIntegerv)(GLenum, GLint *);
extern F_glGetIntegerv xglGetIntegerv;
typedef GLboolean (__stdcall *F_glIsEnabled)(GLenum);
extern F_glIsEnabled xglIsEnabled;
typedef HGLRC (__stdcall *F_wglGetCurrentContext)(void);
extern F_wglGetCurrentContext xwglGetCurrentContext;
typedef PROC (__stdcall *F_wglGetProcAddress)(LPCSTR);
extern F_wglGetProcAddress xwglGetProcAddress;

// FALSE: no opengl32.dll here, or a function missing (then nothing of Half-Life's is drawn).
BOOL Load(void);

}; // namespace

#endif
