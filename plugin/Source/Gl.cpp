/* SeriousHL - OpenGL straight from opengl32.dll. See Gl.h. */
#include "StdH.h"
#include "Gl.h"

namespace gl {

F_glOrtho xglOrtho = NULL;
F_glVertex2f xglVertex2f = NULL;
F_glVertex3f xglVertex3f = NULL;
F_glBegin xglBegin = NULL;
F_glEnd xglEnd = NULL;
F_glVertex3fv xglVertex3fv = NULL;
F_glTexCoord2f xglTexCoord2f = NULL;
F_glColor4f xglColor4f = NULL;
F_glBindTexture xglBindTexture = NULL;
F_glGenTextures xglGenTextures = NULL;
F_glTexParameteri xglTexParameteri = NULL;
F_glTexImage2D xglTexImage2D = NULL;
F_glMatrixMode xglMatrixMode = NULL;
F_glPushMatrix xglPushMatrix = NULL;
F_glPopMatrix xglPopMatrix = NULL;
F_glLoadIdentity xglLoadIdentity = NULL;
F_glFrustum xglFrustum = NULL;
F_glViewport xglViewport = NULL;
F_glDisable xglDisable = NULL;
F_glEnable xglEnable = NULL;
F_glDepthFunc xglDepthFunc = NULL;
F_glDepthRange xglDepthRange = NULL;
F_glDepthMask xglDepthMask = NULL;
F_glColorMask xglColorMask = NULL;
F_glPolygonMode xglPolygonMode = NULL;
F_glShadeModel xglShadeModel = NULL;
F_glTexEnvi xglTexEnvi = NULL;
F_glAlphaFunc xglAlphaFunc = NULL;
F_glBlendFunc xglBlendFunc = NULL;
F_glPushAttrib xglPushAttrib = NULL;
F_glPopAttrib xglPopAttrib = NULL;
F_glPushClientAttrib xglPushClientAttrib = NULL;
F_glPopClientAttrib xglPopClientAttrib = NULL;
F_glGetIntegerv xglGetIntegerv = NULL;
F_glGetFloatv xglGetFloatv = NULL;
F_glReadPixels xglReadPixels = NULL;
F_glIsEnabled xglIsEnabled = NULL;
F_wglGetCurrentContext xwglGetCurrentContext = NULL;
F_wglGetProcAddress xwglGetProcAddress = NULL;

BOOL Load(void) {
  static BOOL bTried = FALSE, bLoaded = FALSE;
  if (bTried) return bLoaded;
  bTried = TRUE;
  HMODULE hGL = GetModuleHandleA("opengl32.dll");
  if (hGL == NULL) return FALSE;
  if ((xglOrtho = (F_glOrtho)GetProcAddress(hGL, "glOrtho")) == NULL) return FALSE;
  if ((xglVertex2f = (F_glVertex2f)GetProcAddress(hGL, "glVertex2f")) == NULL) return FALSE;
  if ((xglVertex3f = (F_glVertex3f)GetProcAddress(hGL, "glVertex3f")) == NULL) return FALSE;
  if ((xglBegin = (F_glBegin)GetProcAddress(hGL, "glBegin")) == NULL) return FALSE;
  if ((xglEnd = (F_glEnd)GetProcAddress(hGL, "glEnd")) == NULL) return FALSE;
  if ((xglVertex3fv = (F_glVertex3fv)GetProcAddress(hGL, "glVertex3fv")) == NULL) return FALSE;
  if ((xglTexCoord2f = (F_glTexCoord2f)GetProcAddress(hGL, "glTexCoord2f")) == NULL) return FALSE;
  if ((xglColor4f = (F_glColor4f)GetProcAddress(hGL, "glColor4f")) == NULL) return FALSE;
  if ((xglBindTexture = (F_glBindTexture)GetProcAddress(hGL, "glBindTexture")) == NULL) return FALSE;
  if ((xglGenTextures = (F_glGenTextures)GetProcAddress(hGL, "glGenTextures")) == NULL) return FALSE;
  if ((xglTexParameteri = (F_glTexParameteri)GetProcAddress(hGL, "glTexParameteri")) == NULL) return FALSE;
  if ((xglTexImage2D = (F_glTexImage2D)GetProcAddress(hGL, "glTexImage2D")) == NULL) return FALSE;
  if ((xglMatrixMode = (F_glMatrixMode)GetProcAddress(hGL, "glMatrixMode")) == NULL) return FALSE;
  if ((xglPushMatrix = (F_glPushMatrix)GetProcAddress(hGL, "glPushMatrix")) == NULL) return FALSE;
  if ((xglPopMatrix = (F_glPopMatrix)GetProcAddress(hGL, "glPopMatrix")) == NULL) return FALSE;
  if ((xglLoadIdentity = (F_glLoadIdentity)GetProcAddress(hGL, "glLoadIdentity")) == NULL) return FALSE;
  if ((xglFrustum = (F_glFrustum)GetProcAddress(hGL, "glFrustum")) == NULL) return FALSE;
  if ((xglViewport = (F_glViewport)GetProcAddress(hGL, "glViewport")) == NULL) return FALSE;
  if ((xglDisable = (F_glDisable)GetProcAddress(hGL, "glDisable")) == NULL) return FALSE;
  if ((xglEnable = (F_glEnable)GetProcAddress(hGL, "glEnable")) == NULL) return FALSE;
  if ((xglDepthFunc = (F_glDepthFunc)GetProcAddress(hGL, "glDepthFunc")) == NULL) return FALSE;
  if ((xglDepthRange = (F_glDepthRange)GetProcAddress(hGL, "glDepthRange")) == NULL) return FALSE;
  if ((xglDepthMask = (F_glDepthMask)GetProcAddress(hGL, "glDepthMask")) == NULL) return FALSE;
  if ((xglColorMask = (F_glColorMask)GetProcAddress(hGL, "glColorMask")) == NULL) return FALSE;
  if ((xglPolygonMode = (F_glPolygonMode)GetProcAddress(hGL, "glPolygonMode")) == NULL) return FALSE;
  if ((xglShadeModel = (F_glShadeModel)GetProcAddress(hGL, "glShadeModel")) == NULL) return FALSE;
  if ((xglTexEnvi = (F_glTexEnvi)GetProcAddress(hGL, "glTexEnvi")) == NULL) return FALSE;
  if ((xglAlphaFunc = (F_glAlphaFunc)GetProcAddress(hGL, "glAlphaFunc")) == NULL) return FALSE;
  if ((xglBlendFunc = (F_glBlendFunc)GetProcAddress(hGL, "glBlendFunc")) == NULL) return FALSE;
  if ((xglPushAttrib = (F_glPushAttrib)GetProcAddress(hGL, "glPushAttrib")) == NULL) return FALSE;
  if ((xglPopAttrib = (F_glPopAttrib)GetProcAddress(hGL, "glPopAttrib")) == NULL) return FALSE;
  if ((xglPushClientAttrib = (F_glPushClientAttrib)GetProcAddress(hGL, "glPushClientAttrib")) == NULL) return FALSE;
  if ((xglPopClientAttrib = (F_glPopClientAttrib)GetProcAddress(hGL, "glPopClientAttrib")) == NULL) return FALSE;
  if ((xglGetIntegerv = (F_glGetIntegerv)GetProcAddress(hGL, "glGetIntegerv")) == NULL) return FALSE;
  if ((xglGetFloatv = (F_glGetFloatv)GetProcAddress(hGL, "glGetFloatv")) == NULL) return FALSE;
  if ((xglReadPixels = (F_glReadPixels)GetProcAddress(hGL, "glReadPixels")) == NULL) return FALSE;
  if ((xglIsEnabled = (F_glIsEnabled)GetProcAddress(hGL, "glIsEnabled")) == NULL) return FALSE;
  if ((xwglGetCurrentContext = (F_wglGetCurrentContext)GetProcAddress(hGL, "wglGetCurrentContext")) == NULL) return FALSE;
  if ((xwglGetProcAddress = (F_wglGetProcAddress)GetProcAddress(hGL, "wglGetProcAddress")) == NULL) return FALSE;
  bLoaded = TRUE;
  return TRUE;
};

}; // namespace
