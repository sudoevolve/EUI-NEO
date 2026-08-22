#include "core/render/opengl/opengl_backend.h"

#include "core/render/render_types.h"

#include <algorithm>
#include <cstdint>

#include <glad/glad.h>

namespace core::render::opengl {

namespace {

struct TextureResourceHeader {
    GLuint texture = 0;
};

struct ImageTextureResource {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
};

struct LayerTextureResource {
    GLuint texture = 0;
    GLuint framebuffer = 0;
    // width/height are allocation capacity; content dimensions track the
    // active viewport inside that texture.
    int width = 0;
    int height = 0;
    int contentWidth = 0;
    int contentHeight = 0;
};

GLuint textureIdFromHandle(RenderBackend::TextureHandle handle) {
    auto* resource = static_cast<TextureResourceHeader*>(handle);
    return resource != nullptr ? resource->texture : 0;
}

} // namespace

OpenGLRenderBackend::LayerHandle OpenGLRenderBackend::createLayer(int width, int height) {
    flushRoundedRectBatch();
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    auto* resource = new LayerTextureResource();
    if (!resizeLayer(resource, width, height)) {
        delete resource;
        return nullptr;
    }
    return resource;
}

bool OpenGLRenderBackend::resizeLayer(LayerHandle handle, int width, int height) {
    flushRoundedRectBatch();
    auto* resource = static_cast<LayerTextureResource*>(handle);
    if (resource == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const bool hasStorage = resource->texture != 0 && resource->framebuffer != 0;
    if (hasStorage && resource->width >= width && resource->height >= height) {
        resource->contentWidth = width;
        resource->contentHeight = height;
        return true;
    }
    const int allocationWidth = !hasStorage || width > resource->width
        ? std::max(width, resource->width + std::max(1, resource->width / 2))
        : resource->width;
    const int allocationHeight = !hasStorage || height > resource->height
        ? std::max(height, resource->height + std::max(1, resource->height / 2))
        : resource->height;
    GLint previousFramebuffer = 0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    // A retained layer can be sampled by the backbuffer draw submitted in the
    // previous frame. OpenGL defers deletion while that work is in flight,
    // which makes rapid layer resizes accumulate every old allocation. The
    // layer handle remains cached; synchronize only when replacing its storage.
    if (resource->texture != 0 || resource->framebuffer != 0) {
        glFinish();
    }
    if (resource->texture != 0) {
        glDeleteTextures(1, &resource->texture);
        resource->texture = 0;
    }
    if (resource->framebuffer != 0) {
        glDeleteFramebuffers(1, &resource->framebuffer);
        resource->framebuffer = 0;
    }

    glGenTextures(1, &resource->texture);
    if (resource->texture == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(std::max(0, previousFramebuffer)));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(std::max(0, previousTexture)));
        resetStateCache();
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, resource->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 allocationWidth, allocationHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &resource->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, resource->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resource->texture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(std::max(0, previousFramebuffer)));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(std::max(0, previousTexture)));
    resetStateCache();

    if (!complete) {
        if (resource->texture != 0) {
            glDeleteTextures(1, &resource->texture);
            resource->texture = 0;
        }
        if (resource->framebuffer != 0) {
            glDeleteFramebuffers(1, &resource->framebuffer);
            resource->framebuffer = 0;
        }
        resetStateCache();
        return false;
    }
    resource->width = allocationWidth;
    resource->height = allocationHeight;
    resource->contentWidth = width;
    resource->contentHeight = height;
    return true;
}

void OpenGLRenderBackend::destroyLayer(LayerHandle handle) {
    flushRoundedRectBatch();
    auto* resource = static_cast<LayerTextureResource*>(handle);
    if (resource == nullptr) {
        return;
    }
    if (resource->texture != 0 || resource->framebuffer != 0) {
        glFinish();
    }
    if (resource->texture != 0) {
        glDeleteTextures(1, &resource->texture);
        resource->texture = 0;
    }
    if (resource->framebuffer != 0) {
        glDeleteFramebuffers(1, &resource->framebuffer);
        resource->framebuffer = 0;
    }
    delete resource;
    resetStateCache();
}

bool OpenGLRenderBackend::beginLayerFrame(LayerHandle handle, int width, int height) {
    flushRoundedRectBatch();
    auto* resource = static_cast<LayerTextureResource*>(handle);
    if (resource == nullptr) {
        return false;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &layerPreviousFramebuffer_);
    glGetIntegerv(GL_VIEWPORT, layerPreviousViewport_);
    if (!resizeLayer(resource, width, height)) {
        return false;
    }
    layerFrameActive_ = true;
    glBindFramebuffer(GL_FRAMEBUFFER, resource->framebuffer);
    glViewport(0, 0, width, height);
    resetStateCache();
    return true;
}

void OpenGLRenderBackend::endLayerFrame() {
    flushRoundedRectBatch();
    if (layerFrameActive_) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(std::max(0, layerPreviousFramebuffer_)));
        glViewport(layerPreviousViewport_[0],
                   layerPreviousViewport_[1],
                   layerPreviousViewport_[2],
                   layerPreviousViewport_[3]);
        layerFrameActive_ = false;
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, cacheFramebuffer_ != 0 ? cacheFramebuffer_ : 0);
        glViewport(0, 0, framebufferWidth_, framebufferHeight_);
    }
    resetStateCache();
}

OpenGLRenderBackend::TextureHandle OpenGLRenderBackend::layerTexture(LayerHandle handle) {
    return handle;
}

OpenGLRenderBackend::TextureHandle OpenGLRenderBackend::createTexture(const unsigned char* pixels,
                                                                      int width,
                                                                      int height) {
    flushRoundedRectBatch();
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }

    auto* resource = new ImageTextureResource();
    glGenTextures(1, &resource->texture);
    if (resource->texture == 0) {
        delete resource;
        return nullptr;
    }

    resource->width = width;
    resource->height = height;
    glBindTexture(GL_TEXTURE_2D, resource->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    resetStateCache();
    return resource;
}

bool OpenGLRenderBackend::updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) {
    flushRoundedRectBatch();
    auto* resource = static_cast<ImageTextureResource*>(handle);
    if (resource == nullptr || resource->texture == 0 || pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, resource->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (resource->width != width || resource->height != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        resource->width = width;
        resource->height = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    resetStateCache();
    return true;
}

void OpenGLRenderBackend::destroyTexture(TextureHandle handle) {
    flushRoundedRectBatch();
    auto* resource = static_cast<ImageTextureResource*>(handle);
    if (resource == nullptr) {
        return;
    }
    if (resource->texture != 0) {
        glDeleteTextures(1, &resource->texture);
        resource->texture = 0;
    }
    delete resource;
    resetStateCache();
}

void OpenGLRenderBackend::drawTexture(TextureHandle handle,
                                      const float* vertices,
                                      std::size_t vertexFloatCount,
                                      const core::Color& tint,
                                      const core::Rect& rect,
                                      float radius,
                                      float blur,
                                      int windowWidth,
                                      int windowHeight) {
    const GLuint texture = textureIdFromHandle(handle);
    if (texture == 0 || vertices == nullptr || vertexFloatCount < 42 ||
        tint.a <= 0.001f || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    flushRoundedRectBatch();
    if (!ensureImageResources()) {
        return;
    }

    setStandardAlphaBlend();

    useProgram(imageShaderProgram_);
    glUniform2f(imageWindowSizeLocation_, static_cast<float>(std::max(1, windowWidth)),
                static_cast<float>(std::max(1, windowHeight)));
    glUniform4f(imageTintLocation_, tint.r, tint.g, tint.b, tint.a);
    glUniform4f(imageRectLocation_, rect.x, rect.y, rect.width, rect.height);
    glUniform1f(imageRadiusLocation_, radius);
    glUniform1f(imageBlurLocation_, blur);
    glUniform1i(imageTextureLocation_, 0);

    bindVertexArray(imageVao_);
    bindArrayBuffer(imageVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexFloatCount * sizeof(float)), vertices);
    activeTextureUnit(0);
    bindTexture2D(texture);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexFloatCount / 7));
}

void OpenGLRenderBackend::drawLayerTexture(TextureHandle handle,
                                           const float* vertices,
                                           std::size_t vertexFloatCount,
                                           const core::Rect& rect,
                                           int windowWidth,
                                           int windowHeight) {
    const GLuint texture = textureIdFromHandle(handle);
    if (texture == 0 || vertices == nullptr || vertexFloatCount < 42 ||
        windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    flushRoundedRectBatch();
    if (!ensureImageResources()) {
        return;
    }

    const auto* resource = static_cast<const LayerTextureResource*>(handle);
    const float uScale = resource->width > 0
        ? static_cast<float>(resource->contentWidth) / static_cast<float>(resource->width)
        : 1.0f;
    const float vScale = resource->height > 0
        ? static_cast<float>(resource->contentHeight) / static_cast<float>(resource->height)
        : 1.0f;
    float adjustedVertices[42];
    const float* uploadVertices = vertices;
    if (vertexFloatCount == 42 && (uScale < 1.0f || vScale < 1.0f)) {
        std::copy(vertices, vertices + 42, adjustedVertices);
        for (std::size_t offset = 0; offset < 42; offset += 7) {
            adjustedVertices[offset + 5] *= uScale;
            adjustedVertices[offset + 6] *= vScale;
        }
        uploadVertices = adjustedVertices;
    }

    setPremultipliedAlphaBlend();

    useProgram(imageShaderProgram_);
    glUniform2f(imageWindowSizeLocation_, static_cast<float>(std::max(1, windowWidth)),
                static_cast<float>(std::max(1, windowHeight)));
    glUniform4f(imageTintLocation_, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4f(imageRectLocation_, rect.x, rect.y, rect.width, rect.height);
    glUniform1f(imageRadiusLocation_, 0.0f);
    glUniform1f(imageBlurLocation_, 0.0f);
    glUniform1i(imageTextureLocation_, 0);

    bindVertexArray(imageVao_);
    bindArrayBuffer(imageVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexFloatCount * sizeof(float)), uploadVertices);
    activeTextureUnit(0);
    bindTexture2D(texture);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexFloatCount / 7));
}

bool OpenGLRenderBackend::ensureImageResources() {
    if (imageShaderProgram_ != 0 && imageVao_ != 0 && imageVbo_ != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aScreenPos;\n"
        "layout(location = 1) in vec2 aLocalPos;\n"
        "layout(location = 2) in vec2 aUV;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vLocalPos;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vLocalPos = aLocalPos;\n"
        "    vUV = aUV;\n"
        "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "in vec2 vLocalPos;\n"
        "in vec2 vUV;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform vec4 uTint;\n"
        "uniform vec4 uRect;\n"
        "uniform float uRadius;\n"
        "uniform float uBlur;\n"
        "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
        "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
        "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
        "}\n"
        "void main() {\n"
        "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
        "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
        "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    vec4 sampled = texture(uTexture, vUV);\n"
        "    if (uBlur > 0.01) {\n"
        "        vec2 pixelStep = max(fwidth(vUV), 1.0 / vec2(textureSize(uTexture, 0)));\n"
        "        vec2 blurStep = pixelStep * uBlur * 0.5;\n"
        "        sampled *= 4.0;\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2( blurStep.x, 0.0), 0.0, 1.0)) * 2.0;\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2(-blurStep.x, 0.0), 0.0, 1.0)) * 2.0;\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2(0.0,  blurStep.y), 0.0, 1.0)) * 2.0;\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2(0.0, -blurStep.y), 0.0, 1.0)) * 2.0;\n"
        "        sampled += texture(uTexture, clamp(vUV + blurStep, 0.0, 1.0));\n"
        "        sampled += texture(uTexture, clamp(vUV - blurStep, 0.0, 1.0));\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2( blurStep.x, -blurStep.y), 0.0, 1.0));\n"
        "        sampled += texture(uTexture, clamp(vUV + vec2(-blurStep.x,  blurStep.y), 0.0, 1.0));\n"
        "        sampled /= 16.0;\n"
        "    }\n"
        "    FragColor = vec4(sampled.rgb * uTint.rgb, sampled.a * uTint.a * shapeAlpha);\n"
        "}\n";

    const GLuint vertexShader = compileImageShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileImageShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    imageShaderProgram_ = glCreateProgram();
    glAttachShader(imageShaderProgram_, vertexShader);
    glAttachShader(imageShaderProgram_, fragmentShader);
    glLinkProgram(imageShaderProgram_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(imageShaderProgram_, GL_LINK_STATUS, &linked);
    if (!linked) {
        releaseImageResources();
        return false;
    }

    imageWindowSizeLocation_ = glGetUniformLocation(imageShaderProgram_, "uWindowSize");
    imageTextureLocation_ = glGetUniformLocation(imageShaderProgram_, "uTexture");
    imageTintLocation_ = glGetUniformLocation(imageShaderProgram_, "uTint");
    imageRectLocation_ = glGetUniformLocation(imageShaderProgram_, "uRect");
    imageRadiusLocation_ = glGetUniformLocation(imageShaderProgram_, "uRadius");
    imageBlurLocation_ = glGetUniformLocation(imageShaderProgram_, "uBlur");

    glGenVertexArrays(1, &imageVao_);
    glGenBuffers(1, &imageVbo_);
    glBindVertexArray(imageVao_);
    glBindBuffer(GL_ARRAY_BUFFER, imageVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 42, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 7, reinterpret_cast<void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 7, reinterpret_cast<void*>(sizeof(float) * 5));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    resetStateCache();

    return imageShaderProgram_ != 0 && imageVao_ != 0 && imageVbo_ != 0;
}

unsigned int OpenGLRenderBackend::compileImageShader(unsigned int type, const char* source) const {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void OpenGLRenderBackend::releaseImageResources() {
    flushRoundedRectBatch();
    if (imageVbo_ != 0) {
        glDeleteBuffers(1, &imageVbo_);
        imageVbo_ = 0;
    }
    if (imageVao_ != 0) {
        glDeleteVertexArrays(1, &imageVao_);
        imageVao_ = 0;
    }
    if (imageShaderProgram_ != 0) {
        glDeleteProgram(imageShaderProgram_);
        imageShaderProgram_ = 0;
    }
    imageWindowSizeLocation_ = -1;
    imageTextureLocation_ = -1;
    imageTintLocation_ = -1;
    imageRectLocation_ = -1;
    imageRadiusLocation_ = -1;
    imageBlurLocation_ = -1;
    resetStateCache();
}

} // namespace core::render::opengl
