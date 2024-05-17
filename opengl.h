#pragma once

#include <glad/glad.h>
#include "matrix.h"
#include "vertex.h"

struct Color;

// TODO: consistent naming
const char* openglErrorCode(GLenum code);
void checkOpenglError(const char* file, int line);
#define CHECK_OPENGL_ERROR() checkOpenglError(__FILE__, __LINE__)

void initializeGl(GLADloadproc getProcAddr);

// This is very useful for debugging OpenGL-related errors
bool installOpenglDebugHandler();

bool isOpenglExtensionAvailable(const char*);

void setupOpenglView(int width, int height, float zoom);

enum class OpenglFeature { FRAMEBUFFER, SEPARATE_BLEND_FUNC, DEBUG };
bool isOpenglFeatureAvailable(OpenglFeature);

void setMatrix(Mat4 m);
Mat4 getMatrix();

bool getBlend();
void setBlend(bool enabled);

bool getScissor();
void setScissor(bool enabled);
void setScissorRect(int x, int y, int w, int h);

bool getDepthTest();
void setDepthTest(bool enabled);
void setDepthFunc(GLenum func);

bool getCullFace();
void setCullFace(bool enabled);

void setLineWidth(float w);
void setPointSize(float s);

void bindTexture(GLuint tex);
GLuint getTexture();

void bindFramebuffer(GLuint fb);

void setMode(GLenum mode);
void setColor(const Color& c);
void setUv(float u, float v);
void addVertex(float x, float y, float z = 0.0f);
void addQuad(float x, float y, float ex, float ey);
void addVertices(Vertex* vertex, size_t count);

GLuint getIndexBase();
void addIndex(GLuint index);

void clear(float r, float g, float b, float a, bool depth = false);

void setBlendFunc(GLenum src, GLenum dest);
void setBlendFuncSeparate(GLenum srcRgb, GLenum destRgb, GLenum srcA, GLenum dstA);

void emitDrawcalls();
