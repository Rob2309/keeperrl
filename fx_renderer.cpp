#include "fx_renderer.h"

#include "fx_manager.h"
#include "fx_defs.h"
#include "fx_particle_system.h"
#include "fx_draw_buffers.h"

#include "opengl.h"
#include "framebuffer.h"
#include "renderer.h"

namespace fx {

static constexpr int nominalSize = Renderer::nominalSize;

struct FXRenderer::SystemDrawInfo {
  bool empty() const {
    return numParticles == 0;
  }

  IRect worldRect; // in pixels
  IVec2 fboPos;
  int firstParticle = 0, numParticles = 0;
};

FXRenderer::FXRenderer(DirectoryPath dataPath, FXManager& mgr) : mgr(mgr), texturesPath(dataPath) {
  useFramebuffer = false;/*isOpenglFeatureAvailable(OpenglFeature::FRAMEBUFFER) &&
                   isOpenglFeatureAvailable(OpenglFeature::SEPARATE_BLEND_FUNC);*/
  drawBuffers = make_unique<DrawBuffers>();
}

void FXRenderer::loadTextures() {
  textures.clear();
  textureScales.clear();
  textures.reserve(EnumInfo<TextureName>::size);
  textureScales.reserve(textures.size());

  for (auto texName : ENUM_ALL(TextureName)) {
    auto& tdef = mgr[texName];
    auto path = texturesPath.file(tdef.fileName);
    int id = -1;
    for (int n = 0; n < (int)textures.size(); n++)
      if (textures[n].getPath() == path) {
        id = n;
        break;
      }

    if (id == -1) {
      id = textures.size();
      textures.emplace_back(path);
      textures.back().setParams(Texture::Filter::linear, Texture::Wrapping::clamp);
      auto tsize = textures.back().getSize(), rsize = textures.back().getRealSize();
      FVec2 scale(float(tsize.x) / float(rsize.x), float(tsize.y) / float(rsize.y));
      textureScales.emplace_back(scale);
    }
    textureIds[texName] = id;
  }
}

FXRenderer::~FXRenderer() {}

void FXRenderer::applyTexScale() {
  auto& elements = drawBuffers->elements;
  auto& vertices = drawBuffers->vertices;

  for (auto& elem : elements) {
    auto scale = textureScales[textureIds[elem.texName]];
    if (scale == FVec2(1.0f))
      continue;

    int end = elem.firstVertex + elem.numVertices;
    for (int i = elem.firstVertex; i < end; i++) {
      vertices[i].uv[0] *= scale.x;
      vertices[i].uv[1] *= scale.y;
    }
  }
}

IRect FXRenderer::visibleTiles(const View& view) {
  float scale = 1.0f / (view.zoom * nominalSize);

  FVec2 topLeft = -view.offset * scale;
  FVec2 size = FVec2(view.size) * scale;

  IVec2 iTopLeft(floor(topLeft.x), floor(topLeft.y));
  IVec2 iSize(ceil(size.x), ceil(size.y));

  return IRect(iTopLeft - IVec2(1, 1), iTopLeft + iSize + IVec2(1, 1));
}

IRect FXRenderer::boundingBox(const DrawParticle* particles, int count) {
  if (count == 0)
    return IRect();

  PROFILE;
  FVec2 min = particles[0].positions[0];
  FVec2 max = min;

  // TODO: what to do with invalid data ?
  for (int n = 0; n < count; n++) {
    auto& particle = particles[n];
    for (auto& pt : particle.positions) {
      min = vmin(min, pt);
      max = vmax(max, pt);
    }
  }

  return {IVec2(min) - IVec2(1), IVec2(max) + IVec2(2)};
}

IVec2 FXRenderer::allocateFboSpace() {
  IVec2 size = orderedBlendFBO ? IVec2(orderedBlendFBO->width, orderedBlendFBO->height) : IVec2(512, 256);

  vector<pair<int, int>> ids;
  ids.reserve(systemDraws.size());

  bool orderByHeight = false;
  for (int n = 0; n < systemDraws.size(); n++) {
    auto& draw = systemDraws[n];
    if (!draw.empty()) {
      int w = draw.worldRect.width(), h = draw.worldRect.height();
      while (size.x < w)
        size.x *= 2;
      while (size.y < h)
        size.y *= 2;
      ids.emplace_back(orderByHeight ? -h : 0, n);
    }
  }

  std::sort(begin(ids), end(ids));

  const IVec2 maxSize(2048);

  bool doesntFit = true;
  while (doesntFit) {
    IVec2 pos;
    int maxHeight = 0;

    doesntFit = false;

    for (auto idPair : ids) {
      int id = idPair.second;
      auto& draw = systemDraws[id];
      int w = draw.worldRect.width(), h = draw.worldRect.height();

      if (pos.x + w > size.x)
        pos = {0, pos.y + maxHeight};

      if (pos.y + h > size.y) {
        if (size == maxSize) { // not enought space in FBO, dropping FXes...
          draw.numParticles = 0;
        } else {
          if (h > size.y || size.x >= size.y)
            size.y *= 2;
          else
            size.x *= 2;
          size = vmin(size, maxSize);
          doesntFit = true;
          break;
        }
      }

      draw.fboPos = pos;
      pos.x += w;
      maxHeight = max(maxHeight, h);
    }
  }

  return size;
}

void FXRenderer::prepareOrdered() {
  PROFILE;
  auto& systems = mgr.getSystems();
  systemDraws.clear();
  systemDraws.resize(systems.size());
  orderedParticles.clear();

  for (int n = 0; n < systems.size(); n++) {
    auto& system = systems[n];
    if (system.isDead || !system.orderedDraw)
      continue;

    auto& def = mgr[system.defId];
    int first = (int)orderedParticles.size();
    for (int ssid = 0; ssid < system.subSystems.size(); ssid++)
      mgr.genQuads(orderedParticles, n, ssid);
    int count = (int)orderedParticles.size() - first;

    if (count > 0) {
      auto rect = boundingBox(&orderedParticles[first], count);
      systemDraws[n] = {rect, IVec2(), first, count};
    }
  }

  if (useFramebuffer) {
    auto fboSize = allocateFboSpace();
    if (!orderedBlendFBO || orderedBlendFBO->width != fboSize.x || orderedBlendFBO->height != fboSize.y) {
      INFO << "FX: creating FBO for ordered rendering (" << fboSize.x << ", " << fboSize.y << ")";
      orderedBlendFBO.reset();
      orderedAddFBO.reset();
      orderedBlendFBO = make_unique<Framebuffer>(fboSize.x, fboSize.y);
      orderedAddFBO = make_unique<Framebuffer>(fboSize.x, fboSize.y);
    }

    // Positioning particles for FBO
    for (int n = 0; n < (int)systemDraws.size(); n++) {
      auto& draw = systemDraws[n];
      if (draw.empty())
        continue;
      FVec2 offset(draw.fboPos - draw.worldRect.min());

      for (int p = 0; p < draw.numParticles; p++) {
        auto& particle = orderedParticles[draw.firstParticle + p];
        for (auto& pos : particle.positions)
          pos += offset;
      }
    }

    drawBuffers->clear();
    drawBuffers->add(orderedParticles.data(), orderedParticles.size());
    applyTexScale();

    drawParticles(FVec2(), *orderedBlendFBO, *orderedAddFBO);
  }
}

void FXRenderer::printSystemDrawsInfo() const {
  for (int n = 0; n < (int)systemDraws.size(); n++) {
    auto& sys = systemDraws[n];
    if (sys.empty())
      continue;

    INFO << "FX System #" << n << ": " << " rect:(" << sys.worldRect.x() << ", "
         << sys.worldRect.y() << ") - (" << sys.worldRect.ex() << ", "
         << sys.worldRect.ey() << ")";
  }
}

void FXRenderer::setView(float zoom, float offsetX, float offsetY, int w, int h) {
  worldView = View{zoom, {offsetX, offsetY}, {w, h}};
  fboView = visibleTiles(worldView);
  auto size = fboView.size() * nominalSize;

  if (useFramebuffer) {
    if (!blendFBO || blendFBO->width != size.x || blendFBO->height != size.y) {
      INFO << "FX: creating FBO (" << size.x << ", " << size.y << ")";
      orderedBlendFBO.reset();
      orderedAddFBO.reset();
      blendFBO = make_unique<Framebuffer>(size.x, size.y);
      addFBO = make_unique<Framebuffer>(size.x, size.y);
    }
  }
}

void FXRenderer::drawParticles(FVec2 viewOffset, Framebuffer& blendFBO, Framebuffer& addFBO) {
  PROFILE;
  int viewPortSize[4];
  glGetIntegerv(GL_VIEWPORT, viewPortSize);
  IVec2 viewSize(blendFBO.width, blendFBO.height);
  auto prevMatrix = getMatrix();

  blendFBO.bind();

  auto scissor = getScissor();
  setScissor(false);
  auto prevTex = getTexture();
  setColor(Color::WHITE);
  setupOpenglView(blendFBO.width, blendFBO.height, 1.0f);

  clear(0.0f, 0.0f, 0.0f, 1.0f);
  setBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
  drawParticles({1.0f, viewOffset, viewSize}, BlendMode::normal);

  addFBO.bind();
  clear(0.0f, 0.0f, 0.0f, 0.0f);

  // TODO: Each effect could control how alpha builds up
  setBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  drawParticles({1.0f, viewOffset, viewSize}, BlendMode::additive);

  Framebuffer::unbind();

  bindTexture(prevTex);
  setScissor(scissor);
  setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  setMatrix(prevMatrix);
  glViewport(viewPortSize[0], viewPortSize[1], viewPortSize[2], viewPortSize[3]);
}

static void drawTexturedQuad(const FRect& rect, const FRect& trect) {
  setMode(GL_TRIANGLES);

  auto vBase = getIndexBase();

  setUv(trect.x(), 1.0f - trect.ey());
  addVertex(rect.x(), rect.ey());
  setUv(trect.ex(), 1.0f - trect.ey());
  addVertex(rect.ex(), rect.ey());
  setUv(trect.ex(), 1.0f - trect.y());
  addVertex(rect.ex(), rect.y());
  setUv(trect.x(), 1.0f - trect.y());
  addVertex(rect.x(), rect.y());

  addIndex(vBase + 0);
  addIndex(vBase + 1);
  addIndex(vBase + 2);
  addIndex(vBase + 0);
  addIndex(vBase + 2);
  addIndex(vBase + 3);
}

void FXRenderer::drawOrdered(const int* ids, int count, float offsetX, float offsetY, Color color) {
  PROFILE;

  auto depthTest = getDepthTest();
  setDepthTest(false);

  auto cullFace = getCullFace();
  setCullFace(false);

  auto prevTex = getTexture();

  setColor(color);

  if (useFramebuffer) {
    tempRects.clear();

    FVec2 fboSize(orderedBlendFBO->width, orderedBlendFBO->height);
    auto invSize = vinv(fboSize);

    // Gathering rectangles to draw
    for (int n = 0; n < count; n++) {
      auto id = ids[n];
      if (id < 0 || id >= systemDraws.size())
        continue;
      auto& draw = systemDraws[id];
      if (draw.empty())
        continue;

      // TODO: some rects are only additive or only blend; filter them
      FRect rect(draw.worldRect);
      rect = rect * worldView.zoom + worldView.offset + FVec2(offsetX, offsetY);
      auto trect = FRect(IRect(draw.fboPos, draw.fboPos + draw.worldRect.size())) * invSize;
      tempRects.emplace_back(rect);
      tempRects.emplace_back(trect);
    }
    if (!tempRects.empty()) {
      int defaultMode = 0, defaultCombine = 0;
      // glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &defaultMode);
      // glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_RGB, &defaultCombine);

      setBlendFunc(GL_ONE, GL_SRC_ALPHA);
      bindTexture(orderedBlendFBO->texId);

      for (int n = 0; n < tempRects.size(); n += 2)
        drawTexturedQuad(tempRects[n], tempRects[n + 1]);

      // Here we're performing blend-add:
      // - for high alpha values we're blending
      // - for low alpha values we're adding
      // For this to work nicely, additive textures need properly prepared alpha channel
      bindTexture(orderedAddFBO->texId);
      setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // These states multiply alpha by itself
      // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
      // glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
      // glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
      // glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE);
      // glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
      // glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

      for (int n = 0; n < tempRects.size(); n += 2)
        drawTexturedQuad(tempRects[n], tempRects[n + 1]);

      // Here we should really multiply by (1 - a), not (1 - a^2), but it looks better
      setBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE);

      for (int n = 0; n < tempRects.size(); n += 2)
        drawTexturedQuad(tempRects[n], tempRects[n + 1]);

      // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, defaultMode);
      // glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, defaultCombine);
    }
  } else {
    drawBuffers->clear();
    for (int n = 0; n < count; n++) {
      auto id = ids[n];
      if (id < 0 || id >= systemDraws.size())
        continue;
      auto& draw = systemDraws[id];
      if (draw.empty())
        continue;
      CHECK(draw.firstParticle + draw.numParticles <= orderedParticles.size());
      drawBuffers->add(&orderedParticles[draw.firstParticle], draw.numParticles);
    }

    auto view = worldView;
    view.offset += FVec2(offsetX, offsetY);

    applyTexScale();
    setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawParticles(view, BlendMode::normal);
    // TODO: blend add support
    setBlendFunc(GL_ONE, GL_ONE);
    drawParticles(view, BlendMode::additive);
  }

  bindTexture(prevTex);
  setCullFace(cullFace);
  setDepthTest(depthTest);
  setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void FXRenderer::drawUnordered(Layer layer) {
  PROFILE;
  tempParticles.clear();
  drawBuffers->clear();

  auto& systems = mgr.getSystems();
  for (int n = 0; n < systems.size(); n++) {
    if (systems[n].isDead)
      continue;
    auto& system = systems[n];
    auto& ssdef = mgr[system.defId];
    if (system.orderedDraw)
      continue;

    for (int ssid = 0; ssid < system.subSystems.size(); ssid++)
      if (ssdef[ssid].layer == layer)
        mgr.genQuads(tempParticles, n, ssid);
  }

  drawBuffers->add(tempParticles.data(), tempParticles.size());
  if (drawBuffers->empty())
    return;
  applyTexScale();

  CHECK_OPENGL_ERROR();

  auto depthTest = getDepthTest();
  auto cullFace = getCullFace();
  auto prevTex = getTexture();
  setDepthTest(false);
  setCullFace(false);

  if (useFramebuffer && blendFBO && addFBO) {
    drawParticles(-FVec2(fboView.min() * nominalSize), *blendFBO, *addFBO);
    setColor(Color::WHITE);

    int defaultMode = 0, defaultCombine = 0;
    // glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &defaultMode);
    // glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_RGB, &defaultCombine);

    // TODO: positioning is wrong for non-integral zoom values
    FVec2 c1 = FVec2(fboView.min() * nominalSize * worldView.zoom) + worldView.offset;
    FVec2 c2 = FVec2(fboView.max() * nominalSize * worldView.zoom) + worldView.offset;

    setBlendFunc(GL_ONE, GL_SRC_ALPHA);
    bindTexture(blendFBO->texId);
    addQuad(c1.x, c1.y, c2.x, c2.y);

    // Here we're performing blend-add:
    // - for high alpha values we're blending
    // - for low alpha values we're adding
    // For this to work nicely, additive textures need properly prepared alpha channel
    bindTexture(addFBO->texId);
    setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // These states multiply alpha by itself
    // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    // glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    // glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
    // glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE);
    // glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    // glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    addQuad(c1.x, c1.y, c2.x, c2.y);

    // Here we should really multiply by (1 - a), not (1 - a^2), but it looks better
    setBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE);
    addQuad(c1.x, c1.y, c2.x, c2.y);

    // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, defaultMode);
    // glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, defaultCombine);
  } else {
    setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawParticles(worldView, BlendMode::normal);
    // TODO: blend add support
    setBlendFunc(GL_ONE, GL_ONE);
    drawParticles(worldView, BlendMode::additive);
  }

  bindTexture(prevTex);
  setCullFace(cullFace);
  setDepthTest(depthTest);
  setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  CHECK_OPENGL_ERROR();
}

pair<unsigned, unsigned> FXRenderer::fboIds(bool ordered) const {
  if (!ordered && blendFBO && addFBO)
    return make_pair(blendFBO->texId, addFBO->texId);
  if (ordered && orderedBlendFBO && orderedAddFBO)
    return make_pair(orderedBlendFBO->texId, orderedAddFBO->texId);
  return make_pair(0u, 0u);
}

IVec2 FXRenderer::fboSize() const {
  return blendFBO ? IVec2(blendFBO->width, blendFBO->height) : IVec2();
}

void FXRenderer::drawParticles(const View& view, BlendMode blendMode) {
  PROFILE;
  auto prevMatrix = getMatrix();
  auto prevTex = getTexture();

  setMatrix(prevMatrix * Mat4::translation(view.offset.x, view.offset.y, 0.0f) * Mat4::scale(view.zoom, view.zoom, 1.0f));

  setMode(GL_TRIANGLES);

  for (auto& elem : drawBuffers->elements) {
    auto& tdef = mgr[elem.texName];
    if (tdef.blendMode != blendMode)
      continue;
    auto& tex = textures[textureIds[elem.texName]];
    bindTexture(*tex.getTexId());
    addVertices(drawBuffers->vertices.data() + elem.firstVertex, elem.numVertices);
  }

  bindTexture(prevTex);
  setMatrix(prevMatrix);
}
}
