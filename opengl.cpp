#include "debug.h"
#include "opengl.h"
#include "util.h"
#include "color.h"

#ifdef ANDROID
static auto g_VertexShaderSource = R"(#version 100
  attribute vec2 in_Position;
  attribute vec2 in_UV;
  attribute vec4 in_Color;

  uniform mat4 u_ProjectionMatrix;

  varying vec2 v2f_UV;
  varying vec4 v2f_Color;

  void main() {
    gl_Position = u_ProjectionMatrix * vec4(in_Position, 0.0, 1.0);
    v2f_UV = in_UV;
    v2f_Color = in_Color;
  }
)";
static auto g_FragmentShaderSource = R"(#version 100
  precision mediump float;

  varying vec2 v2f_UV;
  varying vec4 v2f_Color;

  uniform sampler2D u_Texture;

  void main() {
    gl_FragColor = v2f_Color * texture2D(u_Texture, v2f_UV);
  }
)";
#else
static auto g_VertexShaderSource = R"(#version 330 core
  layout(location = 0) in vec2 in_Position;
  layout(location = 1) in vec2 in_UV;
  layout(location = 2) in vec4 in_Color;

  uniform mat4 u_ProjectionMatrix;

  out vec2 v2f_UV;
  out vec4 v2f_Color;

  void main() {
    gl_Position = u_ProjectionMatrix * vec4(in_Position, 0.0, 1.0);
    v2f_UV = in_UV;
    v2f_Color = in_Color;
  }
)";
static auto g_FragmentShaderSource = R"(#version 330 core
  in vec2 v2f_UV;
  in vec4 v2f_Color;

  uniform sampler2D u_Texture;

  layout(location = 0) out vec4 out_Color;

  void main() {
    out_Color = v2f_Color * texture(u_Texture, v2f_UV);
  }
)";
#endif

static GLuint g_Program;
static GLint g_LocProjectionMatrix;

static GLuint g_VAO;
static GLuint g_VBO;
static GLuint g_IBO;

static GLuint g_WhiteTexture;

const char* openglErrorCode(GLenum code) {
  switch (code) {
#define CASE(e)                                                                                                        \
  case GL_##e:                                                                                                         \
    return #e;
    CASE(INVALID_ENUM)
    CASE(INVALID_VALUE)
    CASE(INVALID_OPERATION)
    CASE(INVALID_FRAMEBUFFER_OPERATION)
    CASE(STACK_OVERFLOW)
    CASE(STACK_UNDERFLOW)
    CASE(OUT_OF_MEMORY)
  default:
    break;
#undef CASE
  }
  return "UNKNOWN";
}

void checkOpenglError(const char* file, int line) {
  auto error = glGetError();
  if (error != GL_NO_ERROR)
    FatalLog.get() << "FATAL " << file << ":" << line << " "
                   << "OpenGL error: " << openglErrorCode(error) << " (" << error << ")";
}

GLuint createShader(GLenum type, const char* src) {
  auto shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  GLint err;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &err);
  if(err != GL_TRUE) {
    GLint len;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    auto msg = new char[len];
    glGetShaderInfoLog(shader, len, nullptr, msg);
    FATAL << "OpenGL " << (type == GL_VERTEX_SHADER ? "vertex " : "fragment ") << "shader error: " << msg;
    delete[] msg;
  }
  return shader;
}

void checkProgram(GLuint program, GLenum flag) {
  GLint err;
  glGetProgramiv(g_Program, flag, &err);
  if(err != GL_TRUE) {
    GLint len;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    auto msg = new char[len];
    glGetProgramInfoLog(program, len, nullptr, msg);
    FATAL << "OpenGL program error: " << msg;
    delete[] msg;
  }
}

void initializeGl(GLADloadproc getProcAddr) {
  gladLoadGLLoader(getProcAddr);

  auto vShader = createShader(GL_VERTEX_SHADER, g_VertexShaderSource);
  auto fShader = createShader(GL_FRAGMENT_SHADER, g_FragmentShaderSource);

  g_Program = glCreateProgram();
  glAttachShader(g_Program, vShader);
  glAttachShader(g_Program, fShader);
  #ifdef ANDROID
  glBindAttribLocation(g_Program, 0, "in_Position");
  glBindAttribLocation(g_Program, 1, "in_UV");
  glBindAttribLocation(g_Program, 2, "in_Color");
  #endif
  glLinkProgram(g_Program);
  checkProgram(g_Program, GL_LINK_STATUS);
  glValidateProgram(g_Program);
  checkProgram(g_Program, GL_VALIDATE_STATUS);

  g_LocProjectionMatrix = glGetUniformLocation(g_Program, "u_ProjectionMatrix");

  glUseProgram(g_Program);

  glGenVertexArrays(1, &g_VAO);
  glGenBuffers(1, &g_VBO);
  glGenBuffers(1, &g_IBO);

  glGenTextures(1, &g_WhiteTexture);
  glBindTexture(GL_TEXTURE_2D, g_WhiteTexture);
  GLubyte pixel[4] { 255, 255, 255, 255 };
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  CHECK_OPENGL_ERROR();
}

static const char* debugSourceText(GLenum source) {
  switch (source) {
#define CASE(suffix, text)                                                                                             \
  case GL_DEBUG_SOURCE_##suffix:                                                                                       \
    return text;
    CASE(API, "API")
    CASE(WINDOW_SYSTEM, "window system")
    CASE(SHADER_COMPILER, "shader compiler")
    CASE(THIRD_PARTY, "third party")
    CASE(APPLICATION, "application")
    CASE(OTHER, "other")
#undef CASE
  }

  return "unknown";
}

static const char* debugTypeText(GLenum type) {
  switch (type) {
#define CASE(suffix, text)                                                                                             \
  case GL_DEBUG_TYPE_##suffix:                                                                                         \
    return text;
    CASE(ERROR, "error")
    CASE(DEPRECATED_BEHAVIOR, "deprecated behavior")
    CASE(UNDEFINED_BEHAVIOR, "undefined behavior")
    CASE(PORTABILITY, "portability issue")
    CASE(PERFORMANCE, "performance issue")
    CASE(MARKER, "marker")
    CASE(PUSH_GROUP, "push group")
    CASE(POP_GROUP, "pop group")
    CASE(OTHER, "other issue")
#undef CASE
  }

  return "unknown";
}

static const char* debugSeverityText(GLenum severity) {
  switch (severity) {
#define CASE(suffix, text)                                                                                             \
  case GL_DEBUG_SEVERITY_##suffix:                                                                                     \
    return text;
    CASE(HIGH, "HIGH")
    CASE(MEDIUM, "medium")
    CASE(LOW, "low")
    CASE(NOTIFICATION, "notification")
#undef CASE
  }

  return "unknown";
}

enum class OpenglVendor { nvidia, amd, intel, unknown };

static OpenglVendor s_vendor = OpenglVendor::unknown;

static void APIENTRY debugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                         const GLchar* message, const void* userParam) {
  // TODO: ignore non-significant error/warning codes
  if (s_vendor == OpenglVendor::nvidia) {
    // Description of messages:
    // https://github.com/tksuoran/RenderStack/blob/master/libraries/renderstack_graphics/source/configuration.cpp#L90
    if (isOneOf(id, 0x00020071, 0x00020084, 0x00020061, 0x00020004, 0x00020072, 0x00020074, 0x00020092))
      return;
  }

  bool isSevere = severity == GL_DEBUG_SEVERITY_HIGH && type != GL_DEBUG_TYPE_OTHER;

  char header[1024];
  snprintf(header, sizeof(header), "%sOpengl %s [%s] id:%d source:%s\n", isSevere ? "FATAL: " : "", debugTypeText(type),
           debugSeverityText(severity), id, debugSourceText(source));
  (isSevere ? FatalLog : InfoLog).get() << header << message;
}

bool installOpenglDebugHandler() {
#ifndef OSX
#ifndef RELEASE
  static bool isInitialized = false, properlyInitialized = false;
  if (isInitialized)
    return properlyInitialized;
  isInitialized = true;

  auto vendor = toLower((const char*)glGetString(GL_VENDOR));
  if (vendor.find("intel") != string::npos)
    s_vendor = OpenglVendor::intel;
  else if (vendor.find("nvidia") != string::npos)
    s_vendor = OpenglVendor::nvidia;
  else if (vendor.find("amd") != string::npos)
    s_vendor = OpenglVendor::amd;

  if (isOpenglExtensionAvailable("KHR_debug")) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    properlyInitialized = true;
    return true;
  }
#endif
#endif
  return false;
}

bool isOpenglExtensionAvailable(const char* text) {
  auto* exts = (const char*)glGetString(GL_EXTENSIONS);
  return strstr(exts, text) != nullptr;
}

void setupOpenglView(int width, int height, float zoom) {
  setMatrix(Mat4::ortho(0.0f, (float)width / zoom, (float)height / zoom, 0.0f, -1.0f, 1.0f));
  glViewport(0, 0, width, height);
  CHECK_OPENGL_ERROR();
}

// void glColor(const Color& col) {
//   glColor4f((float)col.r / 255, (float)col.g / 255, (float)col.b / 255, (float)col.a / 255);
// }

bool isOpenglFeatureAvailable(OpenglFeature feature) {
  switch (feature) {
  case OpenglFeature::FRAMEBUFFER: // GL 3.0
    return glDeleteFramebuffers && glGenFramebuffers && glBindFramebuffer &&
                      glFramebufferTexture2D && glDrawBuffers && glCheckFramebufferStatus &&
        isOpenglExtensionAvailable("ARB_framebuffer_object");
  case OpenglFeature::SEPARATE_BLEND_FUNC: // GL 1.4
    return glBlendFuncSeparate;
  case OpenglFeature::DEBUG: // GL 4.4
    return glDebugMessageCallback && glDebugMessageControl &&
        isOpenglExtensionAvailable("KHR_debug");
  }
#undef ON_WINDOWS
}

struct DrawCommand {
  enum class Command {
    EnableBlend,
    EnableScissor,
    EnableDepthTest,
    EnableCullFace,
    SetScissorRect,
    SetDepthFunc,
    SetBlendFunc,
    SetBlendFuncSeparate,
    SetLineWidth,
    SetPointSize,
    SetMatrix,
    BindTexture,
    BindFramebuffer,
    Clear,
    Draw,
  } command;
  union DrawData {
    bool enable;
    struct ScissorRect {
      int x, y, w, h;
    } scissorRect;
    GLenum depthFunc;
    struct BlendFunc {
      GLenum src;
      GLenum dst;
    } blendFunc;
    struct BlendFuncSeparate {
      GLenum srcRgb;
      GLenum dstRgb;
      GLenum srcA;
      GLenum dstA;
    } blendFuncSeparate;
    float lineWidth;
    float pointSize;
    Mat4 matrix;
    GLuint texture;
    GLuint framebuffer;
    struct Clear {
      bool depth;
      float r, g, b, a;
    } clear;
    struct Draw {
      GLenum mode;
      size_t start;
      size_t count;
    } draw;
  } data;

  DrawCommand() : command{Command::Draw}, data{false} { }
};

static std::vector<Vertex> g_Vertices;
static std::vector<GLuint> g_Indices;
static std::vector<DrawCommand> g_DrawCommands;

static size_t g_CurrentIndexStart;
static GLenum g_CurrentMode;
static Mat4 g_CurrentMatrix;
static bool g_BlendEnabled;
static bool g_ScissorEnabled;
static bool g_DepthTestEnabled;
static bool g_CullFaceEnabled;
static GLuint g_CurrentTexture;
static Color g_CurrentColor;
static float g_CurrentUv[2];

static size_t g_VBOSize = 0;
static size_t g_IBOSize = 0;

static void flushDrawcall() {
  if(g_CurrentIndexStart < g_Indices.size()) {
    DrawCommand cmd; 
    cmd.command = DrawCommand::Command::Draw;
    cmd.data.draw = DrawCommand::DrawData::Draw { g_CurrentMode, g_CurrentIndexStart, g_Indices.size() - g_CurrentIndexStart };
    g_DrawCommands.push_back(cmd);
    g_CurrentIndexStart = g_Indices.size();
  }
}

void setMatrix(Mat4 m) {
  flushDrawcall();
  g_CurrentMatrix = m;
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetMatrix;
  cmd.data.matrix = m;
  g_DrawCommands.push_back(cmd);
}
Mat4 getMatrix() {
  return g_CurrentMatrix;
}

bool getBlend() {
  return g_BlendEnabled;
}
void setBlend(bool enabled) {
  if(enabled != g_BlendEnabled) {
    flushDrawcall();
    g_BlendEnabled = enabled;
    DrawCommand cmd;
    cmd.command = DrawCommand::Command::EnableBlend;
    cmd.data.enable = enabled;
    g_DrawCommands.push_back(cmd);
  }
}

bool getScissor() {
  return g_ScissorEnabled;
}
void setScissor(bool enabled) {
  if(g_ScissorEnabled != enabled) {
    flushDrawcall();
    g_ScissorEnabled = enabled;
    DrawCommand cmd;
    cmd.command = DrawCommand::Command::EnableScissor;
    cmd.data.enable = enabled;
    g_DrawCommands.push_back(cmd);
  }
}
void setScissorRect(int x, int y, int w, int h) {
  flushDrawcall();
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetScissorRect;
  cmd.data.scissorRect = DrawCommand::DrawData::ScissorRect { x, y, w, h };
  g_DrawCommands.push_back(cmd);
}

bool getDepthTest() {
  return g_DepthTestEnabled;
}
void setDepthTest(bool enabled) {
  if(enabled != g_DepthTestEnabled) {
    flushDrawcall();
    g_DepthTestEnabled = enabled;
    DrawCommand cmd;
    cmd.command = DrawCommand::Command::EnableDepthTest;
    cmd.data.enable = enabled;
    g_DrawCommands.push_back(cmd);
  }
}
void setDepthFunc(GLenum func) {
  flushDrawcall();
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetDepthFunc;
  cmd.data.depthFunc = func;
  g_DrawCommands.push_back(cmd);
}

bool getCullFace() {
  return g_CullFaceEnabled;
}
void setCullFace(bool enabled) {
  if(enabled != g_CullFaceEnabled) {
    flushDrawcall();
    g_CullFaceEnabled = enabled;
    DrawCommand cmd;
    cmd.command = DrawCommand::Command::EnableCullFace;
    cmd.data.enable = enabled;
    g_DrawCommands.push_back(cmd);
  }
}

void setLineWidth(float w) {
  flushDrawcall();
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetLineWidth;
  cmd.data.lineWidth = w;
  g_DrawCommands.push_back(cmd);
}
void setPointSize(float s) {
  flushDrawcall();
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetPointSize;
  cmd.data.pointSize = s;
  g_DrawCommands.push_back(cmd);
}

void bindTexture(GLuint tex) {
  if(tex == 0)
    tex = g_WhiteTexture;
  if(tex != g_CurrentTexture) {
    flushDrawcall();
    g_CurrentTexture = tex;
    DrawCommand cmd;
    cmd.command = DrawCommand::Command::BindTexture;
    cmd.data.texture = tex;
    g_DrawCommands.push_back(cmd);
  }
}
GLuint getTexture() {
  return g_CurrentTexture;
}

void bindFramebuffer(GLuint fb) {
  flushDrawcall();
  DrawCommand cmd;
  cmd.command = DrawCommand::Command::BindFramebuffer;
  cmd.data.framebuffer = fb;
  g_DrawCommands.push_back(cmd);
}

void setMode(GLenum mode) {
  if(mode != g_CurrentMode) {
    flushDrawcall();
    g_CurrentMode = mode;
  }
}

void setColor(const Color& c) {
  g_CurrentColor = c;
}

void setUv(float u, float v) {
  g_CurrentUv[0] = u;
  g_CurrentUv[1] = v;
}
void addVertex(float x, float y, float z) {
  g_Vertices.push_back(Vertex { { x, y }, { g_CurrentUv[0], g_CurrentUv[1] }, { g_CurrentColor.r, g_CurrentColor.g, g_CurrentColor.b, g_CurrentColor.a } });
}
void addQuad(float x, float y, float ex, float ey) {
  setMode(GL_TRIANGLES);

  auto vBase = g_Vertices.size();

  g_Vertices.push_back(Vertex { { x, ey }, { 0.0f, 0.0f }, { g_CurrentColor.r, g_CurrentColor.g, g_CurrentColor.b, g_CurrentColor.a } });
  g_Vertices.push_back(Vertex { { ex, ey }, { 1.0f, 0.0f }, { g_CurrentColor.r, g_CurrentColor.g, g_CurrentColor.b, g_CurrentColor.a } });
  g_Vertices.push_back(Vertex { { ex, y }, { 1.0f, 1.0f }, { g_CurrentColor.r, g_CurrentColor.g, g_CurrentColor.b, g_CurrentColor.a } });
  g_Vertices.push_back(Vertex { { x, y }, { 0.0f, 1.0f }, { g_CurrentColor.r, g_CurrentColor.g, g_CurrentColor.b, g_CurrentColor.a } });

  g_Indices.push_back(vBase + 0);
  g_Indices.push_back(vBase + 1);
  g_Indices.push_back(vBase + 2);

  g_Indices.push_back(vBase + 0);
  g_Indices.push_back(vBase + 2);
  g_Indices.push_back(vBase + 3);
}
void addVertices(Vertex* vertex, size_t count) {
  auto vBase = g_Vertices.size();

  g_Vertices.insert(g_Vertices.end(), vertex, vertex + count);
  
  g_Indices.reserve(g_Indices.size() + count);
  for(int i = 0; i < count; i++) {
    g_Indices.push_back(vBase + i);
  }
}

GLuint getIndexBase() {
  return g_Vertices.size();
}
void addIndex(GLuint index) {
  g_Indices.push_back(index);
}

void clear(float r, float g, float b, float a, bool depth) {
  flushDrawcall();

  DrawCommand cmd;
  cmd.command = DrawCommand::Command::Clear;
  cmd.data.clear = DrawCommand::DrawData::Clear { depth, r, g, b, a };
  g_DrawCommands.push_back(cmd);
}

void setBlendFunc(GLenum src, GLenum dest) {
  flushDrawcall();

  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetBlendFunc;
  cmd.data.blendFunc = DrawCommand::DrawData::BlendFunc { src, dest };
  g_DrawCommands.push_back(cmd);
}
void setBlendFuncSeparate(GLenum srcRgb, GLenum destRgb, GLenum srcA, GLenum dstA) {
  flushDrawcall();

  DrawCommand cmd;
  cmd.command = DrawCommand::Command::SetBlendFuncSeparate;
  cmd.data.blendFuncSeparate = DrawCommand::DrawData::BlendFuncSeparate { srcRgb, destRgb, srcA, dstA };
  g_DrawCommands.push_back(cmd);
}

static void enableFeature(GLenum feature, bool enable) {
  if(enable)
    glEnable(feature);
  else
    glDisable(feature);
}

void emitDrawcalls() {
  if(!g_DrawCommands.empty()) {
    glBindVertexArray(g_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    if(g_VBOSize >= g_Vertices.size())
      glBufferSubData(GL_ARRAY_BUFFER, 0, g_Vertices.size() * sizeof(Vertex), g_Vertices.data());
    else {
      glBufferData(GL_ARRAY_BUFFER, g_Vertices.size() * sizeof(Vertex), g_Vertices.data(), GL_DYNAMIC_DRAW);
      g_VBOSize = g_Vertices.size();
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_IBO);
    if(g_IBOSize >= g_Indices.size())
      glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, g_Indices.size() * sizeof(GLuint), g_Indices.data());
    else {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_Indices.size() * sizeof(GLuint), g_Indices.data(), GL_DYNAMIC_DRAW);
      g_IBOSize = g_Indices.size();
    }

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)16);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glUseProgram(g_Program);

    for(const auto& cmd : g_DrawCommands) {
      switch(cmd.command) {
        case DrawCommand::Command::EnableBlend: enableFeature(GL_BLEND, cmd.data.enable); break;
        case DrawCommand::Command::EnableScissor: enableFeature(GL_SCISSOR_TEST, cmd.data.enable); break;
        case DrawCommand::Command::EnableDepthTest: enableFeature(GL_DEPTH_TEST, cmd.data.enable); break;
        case DrawCommand::Command::EnableCullFace: enableFeature(GL_CULL_FACE, cmd.data.enable); break;
        case DrawCommand::Command::SetScissorRect: glScissor(cmd.data.scissorRect.x, cmd.data.scissorRect.y, cmd.data.scissorRect.w, cmd.data.scissorRect.h); break;
        case DrawCommand::Command::SetDepthFunc: glDepthFunc(cmd.data.depthFunc); break;
        case DrawCommand::Command::SetBlendFunc: glBlendFunc(cmd.data.blendFunc.src, cmd.data.blendFunc.dst); break;
        case DrawCommand::Command::SetBlendFuncSeparate: glBlendFuncSeparate(cmd.data.blendFuncSeparate.srcRgb, cmd.data.blendFuncSeparate.dstRgb, cmd.data.blendFuncSeparate.srcA, cmd.data.blendFuncSeparate.dstA); break;
        case DrawCommand::Command::SetLineWidth: glLineWidth(cmd.data.lineWidth); break;
        case DrawCommand::Command::SetPointSize: glPointSize(cmd.data.pointSize); break;
        case DrawCommand::Command::SetMatrix: glUniformMatrix4fv(g_LocProjectionMatrix, 1, GL_FALSE, cmd.data.matrix.data); break;
        case DrawCommand::Command::BindTexture: glBindTexture(GL_TEXTURE_2D, cmd.data.texture); break;
        case DrawCommand::Command::BindFramebuffer: glBindFramebuffer(GL_FRAMEBUFFER, cmd.data.framebuffer); break;
        case DrawCommand::Command::Clear: 
          {
            GLbitfield mask = GL_COLOR_BUFFER_BIT;
            if(cmd.data.clear.depth)
              mask |= GL_DEPTH_BUFFER_BIT;
            glClearColor(cmd.data.clear.r, cmd.data.clear.g, cmd.data.clear.b, cmd.data.clear.a);
            glClear(mask);
          }
          break;
        case DrawCommand::Command::Draw: glDrawElements(cmd.data.draw.mode, cmd.data.draw.count, GL_UNSIGNED_INT, (void*)(cmd.data.draw.start * sizeof(GLuint))); break;
      }
    }
  }

  g_Vertices.clear();
  g_Indices.clear();
  g_DrawCommands.clear();
  g_CurrentIndexStart = 0;
}