#include "framebuffer.h"

#include "debug.h"
#include "opengl.h"


static unsigned allocFramebuffer() {
  GLuint id = 0;
  CHECK_OPENGL_ERROR();
  glGenFramebuffers(1, &id);
  CHECK_OPENGL_ERROR();
  return id;
}

static unsigned allocTexture(int width, int height) {
  GLuint id = 0;
  glGenTextures(1, &id);
  CHECK_OPENGL_ERROR();
  // TODO: something better than 8bits per component for blending ?
  // TODO: power of 2 textures
  glBindTexture(GL_TEXTURE_2D, id);
  CHECK_OPENGL_ERROR();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  CHECK_OPENGL_ERROR();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  CHECK_OPENGL_ERROR();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  CHECK_OPENGL_ERROR();
  glBindTexture(GL_TEXTURE_2D, 0);
  CHECK_OPENGL_ERROR();
  return id;
}

Framebuffer::Framebuffer(int width, int height)
    : width(width), height(height), id(allocFramebuffer()), texId(allocTexture(width, height)) {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  CHECK_OPENGL_ERROR();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
  CHECK_OPENGL_ERROR();
  GLenum drawTargets[1] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, drawTargets);

  auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  CHECK(ret == GL_FRAMEBUFFER_COMPLETE) << ret;
  unbind();
}

void Framebuffer::bind() {
  bindFramebuffer(id);
  CHECK_OPENGL_ERROR();
}

void Framebuffer::unbind() {
  bindFramebuffer(0);
  CHECK_OPENGL_ERROR();
}

Framebuffer::~Framebuffer() {
  if (id)
    glDeleteFramebuffers(1, &id);
  if (texId)
    glDeleteTextures(1, &texId);
  CHECK_OPENGL_ERROR();
}
