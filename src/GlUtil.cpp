#include "GlUtil.h"

#include "Gl.h"

static libretro::LoggerComponent* s_logger;

void GlUtil::init(libretro::LoggerComponent* logger)
{
  s_logger = logger;
}

GLuint GlUtil::createTexture(GLsizei width, GLsizei height, GLint internalFormat, GLenum format, GLenum type, GLenum filter)
{
  if (!Gl::ok()) return 0;

  GLuint texture;
  Gl::genTextures(1, &texture);
  Gl::bindTexture(GL_TEXTURE_2D, texture);

  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  Gl::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

  Gl::texImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, NULL);

  if (!Gl::ok())
  {
    Gl::deleteTextures(1, &texture);
    return 0;
  }

  return texture;
}

GLuint GlUtil::createShader(GLenum shaderType, const char* source)
{
  if (!Gl::ok()) return 0;

  GLuint shader = Gl::createShader(shaderType);
  Gl::shaderSource(shader, 1, &source, NULL);
  Gl::compileShader(shader);

  GLint status;
  Gl::getShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status == GL_FALSE)
  {
    char buffer[4096];
    Gl::getShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    s_logger->printf(RETRO_LOG_ERROR, "Error in shader: %s", buffer);
    Gl::deleteShader(shader);
    return 0;
  }

  if (!Gl::ok())
  {
    Gl::deleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint GlUtil::createProgram(const char* vertexShader, const char* fragmentShader)
{
  if (!Gl::ok()) return 0;

  GLuint vs = createShader(GL_VERTEX_SHADER, vertexShader);
  GLuint fs = createShader(GL_FRAGMENT_SHADER, fragmentShader);
  GLuint program = Gl::createProgram();

  Gl::attachShader(program, vs);
  Gl::attachShader(program, fs);
  Gl::linkProgram(program);

  Gl::deleteShader(vs);
  Gl::deleteShader(fs);

  Gl::validateProgram(program);

  GLint status;
  Gl::getProgramiv(program, GL_LINK_STATUS, &status);

  if (status == GL_FALSE)
  {
    char buffer[4096];
    Gl::getProgramInfoLog(program, sizeof(buffer), NULL, buffer);
    s_logger->printf(RETRO_LOG_ERROR, "Error in shader program: %s", buffer);
    Gl::deleteProgram(program);
    return 0;
  }

  if (!Gl::ok())
  {
    Gl::deleteProgram(program);
    return 0;
  }

  return program;
}

GLuint GlUtil::createFramebuffer(GLuint* renderbuffer, GLsizei width, GLsizei height, GLuint texture, bool depth, bool stencil)
{
  if (!Gl::ok()) return 0;

  GLuint framebuffer;
  Gl::genFramebuffers(1, &framebuffer);
  Gl::bindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  Gl::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  *renderbuffer = 0;

  if (depth && stencil)
  {
    Gl::genRenderbuffers(1, renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
    Gl::renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    Gl::framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, 0);
  }
  else if (depth)
  {
    Gl::genRenderbuffers(1, renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
    Gl::renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    Gl::framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    Gl::bindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  if (Gl::checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    if (renderbuffer != 0)
    {
      Gl::deleteRenderbuffers(1, renderbuffer);
    }

    Gl::deleteFramebuffers(1, &framebuffer);
    *renderbuffer = 0;
    return 0;
  }

  Gl::clearColor(0, 0, 0, 1);
  Gl::clear(GL_COLOR_BUFFER_BIT);

  Gl::bindFramebuffer(GL_FRAMEBUFFER, 0);
  return framebuffer;
}

bool GlUtil::Program::init(const char* vertexShader, const char* fragmentShader)
{
  _program = GlUtil::createProgram(vertexShader, fragmentShader);
  return Gl::ok();
}

void GlUtil::Program::destroy()
{
  if (_program != 0)
  {
    Gl::deleteProgram(_program);
    _program = 0;
  }
}

GlUtil::Attribute GlUtil::Program::getAttribute(const char* name) const
{
  return {Gl::getAttribLocation(_program, name)};
}

GlUtil::Uniform GlUtil::Program::getUniform(const char* name) const
{
  return {Gl::getUniformLocation(_program, name)};
}

void GlUtil::Program::use() const
{
  Gl::useProgram(_program);
}

bool GlUtil::Texture::init(GLsizei width, GLsizei height, GLint internalFormat, bool linearFilter)
{
  _texture = GlUtil::createTexture(width, height, internalFormat, GL_RED, GL_UNSIGNED_BYTE, linearFilter ? GL_LINEAR : GL_NEAREST);

  _width = width;
  _height = height;
  _internalFormat = internalFormat;

  return true;
}

void GlUtil::Texture::destroy()
{
  if (_texture != 0)
  {
    Gl::deleteTextures(1, &_texture);
    _texture = 0;
  }
}

void GlUtil::Texture::setData(GLsizei width, GLsizei height, GLsizei pitch, GLenum format, GLenum type, const GLvoid* pixels)
{
  bind();

  GLsizei w = pitch / bpp(type);

  if (w == _width && height <= _height)
  {
    Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, height, format, type, pixels);
    return;
  }

  auto p = (const uint8_t*)pixels;

  if (height > _height)
  {
    height = _height;
  }

  for (GLsizei y = 0; y < height; y++, p += pitch)
  {
    Gl::texSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, format, type, p);
  }
}

void* GlUtil::Texture::getData(GLenum format, GLenum type) const
{
  void* pixels = malloc(_width * _height * bpp(type));

  if (pixels == NULL)
  {
    return NULL;
  }

  bind();
  Gl::getTexImage(GL_TEXTURE_2D, 0, format, type, pixels);
  return pixels;
}

void GlUtil::Texture::bind() const
{
  Gl::bindTexture(GL_TEXTURE_2D, _texture);
}

GLsizei GlUtil::Texture::getWidth() const
{
  return _width;
}

GLsizei GlUtil::Texture::getHeight() const
{
  return _height;
}

void GlUtil::Texture::setUniform(Uniform uniform, int unit) const
{
  Gl::activeTexture(GL_TEXTURE0 + unit);
  bind();
  Gl::uniform1i(uniform.location, unit);
}

GLsizei GlUtil::Texture::bpp(GLenum type)
{
  switch (type)
  {
  case GL_UNSIGNED_BYTE:
  case GL_BYTE:
  case GL_UNSIGNED_BYTE_3_3_2:
  case GL_UNSIGNED_BYTE_2_3_3_REV:
    return 1;

  case GL_UNSIGNED_SHORT:
  case GL_SHORT:
  case GL_HALF_FLOAT:
  case GL_UNSIGNED_SHORT_5_6_5:
  case GL_UNSIGNED_SHORT_5_6_5_REV:
  case GL_UNSIGNED_SHORT_4_4_4_4:
  case GL_UNSIGNED_SHORT_4_4_4_4_REV:
  case GL_UNSIGNED_SHORT_5_5_5_1:
  case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    return 2;

  case GL_UNSIGNED_INT:
  case GL_INT:
  case GL_FLOAT:
  case GL_UNSIGNED_INT_8_8_8_8:
  case GL_UNSIGNED_INT_8_8_8_8_REV:
  case GL_UNSIGNED_INT_10_10_10_2:
  case GL_UNSIGNED_INT_2_10_10_10_REV:
  case GL_UNSIGNED_INT_24_8:
  case GL_UNSIGNED_INT_10F_11F_11F_REV:
  case GL_UNSIGNED_INT_5_9_9_9_REV:
  case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
    return 4;
  
  default:
    return 0;
  }
}

bool GlUtil::VertexBuffer::init()
{
  Gl::genBuffers(1, &_vbo);
  return Gl::ok();
}

void GlUtil::VertexBuffer::destroy()
{
  if (_vbo != 0)
  {
    Gl::deleteBuffers(1, &_vbo);
    _vbo = 0;
  }
}

bool GlUtil::VertexBuffer::setData(const void* data, size_t dataSize)
{
  Gl::bindBuffer(GL_ARRAY_BUFFER, _vbo);
  Gl::bufferData(GL_ARRAY_BUFFER, dataSize, data, GL_STATIC_DRAW);
  return Gl::ok();
}

void GlUtil::VertexBuffer::bind() const
{
  Gl::bindBuffer(GL_ARRAY_BUFFER, _vbo);
}

void GlUtil::VertexBuffer::enable(Attribute attribute, GLint size, GLenum type, GLsizei stride, GLsizei offset) const
{
  Gl::vertexAttribPointer(attribute.location, size, type, GL_FALSE, stride, (const GLvoid*)offset);
  Gl::enableVertexAttribArray(attribute.location);
}

void GlUtil::VertexBuffer::draw(GLenum mode, GLsizei count) const
{
  Gl::drawArrays(mode, 0, count);
}

bool GlUtil::TexturedQuad::init()
{
  return init(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

bool GlUtil::TexturedQuad::init(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1)
{
  if (!VertexBuffer::init())
  {
    return false;
  }

  const Vertex vertices[4] = {
    {x0, y0, u0, v1},
    {x0, y1, u0, v0},
    {x1, y0, u1, v1},
    {x1, y1, u1, v0}
  };

  setData(vertices, sizeof(vertices));
  return true;
}

void GlUtil::TexturedQuad::destroy()
{
  VertexBuffer::destroy();
}

void GlUtil::TexturedQuad::bind() const
{
  VertexBuffer::bind();
}

void GlUtil::TexturedQuad::enablePos(Attribute attribute) const
{
  enable(attribute, 2, GL_FLOAT,sizeof(Vertex), offsetof(Vertex, x));
}

void GlUtil::TexturedQuad::enableUV(Attribute attribute) const
{
  enable(attribute, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, u));
}

void GlUtil::TexturedQuad::draw() const
{
  VertexBuffer::draw(GL_TRIANGLE_STRIP, 4);
}

bool GlUtil::TexturedTriangleBatch::init(const Vertex* vertices, size_t count)
{
  if (!VertexBuffer::init())
  {
    return false;
  }

  setData(vertices, count * sizeof(Vertex));
  _count = count;
  return true;
}

void GlUtil::TexturedTriangleBatch::destroy()
{
  VertexBuffer::destroy();
}

void GlUtil::TexturedTriangleBatch::bind() const
{
  VertexBuffer::bind();
}

void GlUtil::TexturedTriangleBatch::enablePos(Attribute attribute) const
{
  enable(attribute, 2, GL_FLOAT,sizeof(Vertex), offsetof(Vertex, x));
}

void GlUtil::TexturedTriangleBatch::enableUV(Attribute attribute) const
{
  enable(attribute, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, u));
}

void GlUtil::TexturedTriangleBatch::draw() const
{
  VertexBuffer::draw(GL_TRIANGLES, _count);
}
