#ifndef GLOD_STUB_H
#define GLOD_STUB_H

#include <windows.h>
#include <GL/gl.h>

#define GLOD_NO_ERROR 0
#define GLOD_DISCRETE 0
#define GLOD_QUEUE_GREEDY 0
#define GLOD_BORDER_UNLOCK 0
#define GLOD_OPERATOR_EDGE_COLLAPSE 0
#define GLOD_TRIANGLE_BUDGET 1
#define GLOD_ERROR_THRESHOLD 2
#define GLOD_ADAPT_MODE 0
#define GLOD_ERROR_MODE 1
#define GLOD_OBJECT_SPACE_ERROR 0
#define GLOD_OBJECT_SPACE_ERROR_THRESHOLD 0
#define GLOD_MAX_TRIANGLES 1
#define GLOD_NUM_PATCHES 0
#define GLOD_PATCH_SIZES 1
#define GLOD_PATCH_NAMES 2

struct glodVBOAttrib
{
    void* p;
    int size;
    int stride;
    GLenum type;
};

struct glodVBO
{
    GLuint mVBO;
    GLuint mIBO;
    GLuint mVAO;
    int mVertexStride;
    int mVertexOffset;
    int mNormalOffset;
    int mTexCoordOffset;
    glodVBOAttrib mV;
    glodVBOAttrib mN;
    glodVBOAttrib mT;
};

inline void glodInit() {}
inline GLuint glodGetError() { return GLOD_NO_ERROR; }
inline void glodNewGroup(GLuint) {}
inline void glodNewObject(GLuint, GLuint, GLuint) {}
inline void glodDeleteObject(GLuint) {}
inline void glodDeleteGroup(GLuint) {}
inline void glodInsertElements(GLuint, GLuint, GLenum, GLuint, GLenum, GLvoid*, GLuint, GLfloat, glodVBO*) {}
inline void glodBuildObject(GLuint) {}
inline void glodGroupParameteri(GLuint, GLuint, GLint) {}
inline void glodGroupParameterf(GLuint, GLuint, GLfloat) {}
inline void glodAdaptGroup(GLuint) {}
inline void glodGetObjectParameteriv(GLuint, GLuint, GLint*) {}
inline void glodFillElements(GLuint, GLuint, GLenum, GLvoid*, glodVBO*) {}

#endif
