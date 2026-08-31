#include "core/render/opengl/opengl_backend.h"

#include "core/render/render_types.h"

#include <algorithm>
#include <cstdint>

#include <glad/glad.h>

namespace core::render::opengl {

namespace {

struct TextureResourceHeader {
    GLuint texture = 0;
    unsigned int kind = 0;
};

struct ImageTextureResource {
    GLuint texture = 0;
    unsigned int kind = 0;
    int width = 0;
    int height = 0;
};

struct YuvTextureResource {
    GLuint texture = 0;
    unsigned int kind = 1;
    GLuint textureU = 0;
    GLuint textureV = 0;
    int width = 0;
    int height = 0;
    ImagePixelFormat format = ImagePixelFormat::NV12;
    ImageColorSpace colorSpace = ImageColorSpace::BT709;
    ImageColorRange colorRange = ImageColorRange::Limited;
};

struct LayerTextureResource {
    GLuint texture = 0;
    unsigned int kind = 2;
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

bool uploadPlane(GLuint& texture,
                 GLint internalFormat,
                 GLenum format,
                 GLenum type,
                 int width,
                 int height,
                 const std::uint8_t* pixels,
                 std::uint32_t stride,
                 int bytesPerPixel,
                 bool recreateStorage) {
    if (width <= 0 || height <= 0 || pixels == nullptr || stride < static_cast<std::uint32_t>(width * bytesPerPixel)) {
        return false;
    }
    if (texture == 0) {
        glGenTextures(1, &texture);
        if (texture == 0) {
            return false;
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(stride / static_cast<std::uint32_t>(bytesPerPixel)));
    if (recreateStorage) {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, pixels);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return true;
}

void deleteTexture(GLuint& texture) {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
}

void yuvUniforms(const YuvTextureResource& resource, float* matrix, float* offset) {
    const bool limited = resource.colorRange == ImageColorRange::Limited;
    const bool p010 = resource.format == ImagePixelFormat::P010;
    const float normalization = p010 ? 65535.0f : 255.0f;
    const float sampleScale = p010 ? 64.0f : 1.0f;
    const float maxCode = p010 ? 1023.0f : 255.0f;
    const float lumaRange = p010 ? 876.0f : 219.0f;
    const float chromaRange = p010 ? 896.0f : 224.0f;
    const float yScale = limited
        ? normalization / (lumaRange * sampleScale)
        : normalization / (maxCode * sampleScale);
    const float chromaScale = limited
        ? normalization / (chromaRange * sampleScale)
        : normalization / (maxCode * sampleScale);
    offset[0] = limited ? (p010 ? 4096.0f / normalization : 16.0f / 255.0f) : 0.0f;
    offset[1] = p010 ? 32768.0f / normalization : 0.5f;
    offset[2] = offset[1];
    float rv = 1.5748f;
    float gu = 0.187324f;
    float gv = 0.468124f;
    float bu = 1.8556f;
    if (resource.colorSpace == ImageColorSpace::BT601) {
        rv = 1.4020f; gu = 0.344136f; gv = 0.714136f; bu = 1.7720f;
    } else if (resource.colorSpace == ImageColorSpace::BT2020) {
        rv = 1.4746f; gu = 0.16455f; gv = 0.57135f; bu = 1.8814f;
    }
    matrix[0] = yScale; matrix[1] = 0.0f; matrix[2] = rv * chromaScale;
    matrix[3] = yScale; matrix[4] = -gu * chromaScale; matrix[5] = -gv * chromaScale;
    matrix[6] = yScale; matrix[7] = bu * chromaScale; matrix[8] = 0.0f;
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

OpenGLRenderBackend::TextureHandle OpenGLRenderBackend::createDynamicTexture(const ImageFrame& frame) {
    if (frame.format != ImagePixelFormat::NV12 && frame.format != ImagePixelFormat::I420 &&
        frame.format != ImagePixelFormat::P010) {
        return nullptr;
    }
    auto* resource = new YuvTextureResource();
    if (!updateDynamicTexture(resource, frame)) {
        deleteTexture(resource->texture);
        deleteTexture(resource->textureU);
        deleteTexture(resource->textureV);
        delete resource;
        return nullptr;
    }
    return resource;
}

bool OpenGLRenderBackend::updateDynamicTexture(TextureHandle handle, const ImageFrame& frame) {
    flushRoundedRectBatch();
    auto* resource = static_cast<YuvTextureResource*>(handle);
    if (resource == nullptr || resource->kind != 1 || !frame.valid() ||
        (frame.format != ImagePixelFormat::NV12 && frame.format != ImagePixelFormat::I420 &&
         frame.format != ImagePixelFormat::P010)) {
        return false;
    }
    const int width = static_cast<int>(frame.width);
    const int height = static_cast<int>(frame.height);
    const int chromaWidth = static_cast<int>((frame.width + 1u) / 2u);
    const int chromaHeight = static_cast<int>((frame.height + 1u) / 2u);
    const bool isP010 = frame.format == ImagePixelFormat::P010;
    const GLenum type = isP010 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
    const GLint singleInternal = isP010 ? GL_R16 : GL_R8;
    const GLint dualInternal = isP010 ? GL_RG16 : GL_RG8;
    const int sampleBytes = isP010 ? 2 : 1;
    const bool formatChanged = resource->width > 0 && resource->format != frame.format;
    if (formatChanged) {
        deleteTexture(resource->texture);
        deleteTexture(resource->textureU);
        deleteTexture(resource->textureV);
    }
    const bool storageChanged = formatChanged || resource->width != width || resource->height != height;

    if (!uploadPlane(resource->texture, singleInternal, GL_RED, type, width, height,
                     frame.pixels->data(), frame.stride, sampleBytes,
                     storageChanged)) {
        return false;
    }
    if (frame.format == ImagePixelFormat::I420) {
        const bool chromaSizeChanged = storageChanged;
        if (!uploadPlane(resource->textureU, GL_R8, GL_RED, GL_UNSIGNED_BYTE, chromaWidth, chromaHeight,
                         frame.plane1->data(), frame.stride1, 1, chromaSizeChanged) ||
            !uploadPlane(resource->textureV, GL_R8, GL_RED, GL_UNSIGNED_BYTE, chromaWidth, chromaHeight,
                         frame.plane2->data(), frame.stride2, 1, chromaSizeChanged)) {
            return false;
        }
    } else if (!uploadPlane(resource->textureU, dualInternal, GL_RG, type, chromaWidth, chromaHeight,
                            frame.plane1->data(), frame.stride1, sampleBytes * 2,
                            storageChanged)) {
        return false;
    }
    if (frame.format != ImagePixelFormat::I420) {
        deleteTexture(resource->textureV);
    }
    resource->width = width;
    resource->height = height;
    resource->format = frame.format;
    resource->colorSpace = frame.colorSpace;
    resource->colorRange = frame.colorRange;
    glBindTexture(GL_TEXTURE_2D, 0);
    resetStateCache();
    return true;
}

void OpenGLRenderBackend::destroyTexture(TextureHandle handle) {
    flushRoundedRectBatch();
    auto* header = static_cast<TextureResourceHeader*>(handle);
    if (header == nullptr) {
        return;
    }
    if (header->kind == 1) {
        auto* resource = static_cast<YuvTextureResource*>(handle);
        deleteTexture(resource->texture);
        deleteTexture(resource->textureU);
        deleteTexture(resource->textureV);
        delete resource;
    } else {
        auto* resource = static_cast<ImageTextureResource*>(handle);
        deleteTexture(resource->texture);
        delete resource;
    }
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
    const auto* header = static_cast<const TextureResourceHeader*>(handle);
    const bool yuv = header != nullptr && header->kind == 1;
    glUniform1i(imageYuvModeLocation_,
                yuv && static_cast<const YuvTextureResource*>(handle)->format == ImagePixelFormat::I420
                    ? 2
                    : (yuv ? 1 : 0));
    if (yuv) {
        const auto* resource = static_cast<const YuvTextureResource*>(handle);
        float matrix[9] = {};
        float offset[3] = {};
        yuvUniforms(*resource, matrix, offset);
        glUniformMatrix3fv(imageYuvMatrixLocation_, 1, GL_TRUE, matrix);
        glUniform3fv(imageYuvOffsetLocation_, 1, offset);
        glUniform1i(imageTextureULocation_, 1);
        glUniform1i(imageTextureVLocation_, 2);
        activeTextureUnit(1);
        bindTexture2D(resource->textureU);
        activeTextureUnit(2);
        bindTexture2D(resource->textureV != 0 ? resource->textureV : resource->textureU);
    }

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
    glUniform1i(imageYuvModeLocation_, 0);

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
        "uniform sampler2D uTextureU;\n"
        "uniform sampler2D uTextureV;\n"
        "uniform int uYuvMode;\n"
        "uniform mat3 uYuvMatrix;\n"
        "uniform vec3 uYuvOffset;\n"
        "uniform vec4 uTint;\n"
        "uniform vec4 uRect;\n"
        "uniform float uRadius;\n"
        "uniform float uBlur;\n"
        "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
        "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
        "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
        "}\n"
        "vec4 sampleImage(vec2 uv) {\n"
        "    vec4 sampled = texture(uTexture, uv);\n"
        "    if (uYuvMode == 0) return sampled;\n"
        "    vec2 chroma = texture(uTextureU, uv).rg;\n"
        "    if (uYuvMode == 2) chroma = vec2(texture(uTextureU, uv).r, texture(uTextureV, uv).r);\n"
        "    return vec4(uYuvMatrix * (vec3(sampled.r, chroma) - uYuvOffset), 1.0);\n"
        "}\n"
        "void main() {\n"
        "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
        "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
        "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    vec4 sampled = sampleImage(vUV);\n"
        "    if (uBlur > 0.01) {\n"
        "        vec2 pixelStep = max(fwidth(vUV), 1.0 / vec2(textureSize(uTexture, 0)));\n"
        "        vec2 blurStep = pixelStep * uBlur * 0.5;\n"
        "        sampled *= 4.0;\n"
        "        sampled += sampleImage(clamp(vUV + vec2( blurStep.x, 0.0), 0.0, 1.0)) * 2.0;\n"
        "        sampled += sampleImage(clamp(vUV + vec2(-blurStep.x, 0.0), 0.0, 1.0)) * 2.0;\n"
        "        sampled += sampleImage(clamp(vUV + vec2(0.0,  blurStep.y), 0.0, 1.0)) * 2.0;\n"
        "        sampled += sampleImage(clamp(vUV + vec2(0.0, -blurStep.y), 0.0, 1.0)) * 2.0;\n"
        "        sampled += sampleImage(clamp(vUV + blurStep, 0.0, 1.0));\n"
        "        sampled += sampleImage(clamp(vUV - blurStep, 0.0, 1.0));\n"
        "        sampled += sampleImage(clamp(vUV + vec2( blurStep.x, -blurStep.y), 0.0, 1.0));\n"
        "        sampled += sampleImage(clamp(vUV + vec2(-blurStep.x,  blurStep.y), 0.0, 1.0));\n"
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
    imageYuvModeLocation_ = glGetUniformLocation(imageShaderProgram_, "uYuvMode");
    imageTextureULocation_ = glGetUniformLocation(imageShaderProgram_, "uTextureU");
    imageTextureVLocation_ = glGetUniformLocation(imageShaderProgram_, "uTextureV");
    imageYuvMatrixLocation_ = glGetUniformLocation(imageShaderProgram_, "uYuvMatrix");
    imageYuvOffsetLocation_ = glGetUniformLocation(imageShaderProgram_, "uYuvOffset");

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
    imageYuvModeLocation_ = -1;
    imageTextureULocation_ = -1;
    imageTextureVLocation_ = -1;
    imageYuvMatrixLocation_ = -1;
    imageYuvOffsetLocation_ = -1;
    resetStateCache();
}

} // namespace core::render::opengl
