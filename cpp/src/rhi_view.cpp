// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "rhi_view.h"

#include "display_hdr.h"
#include "theme.h"

#include <QResizeEvent>

#if defined(LUCENT_HAS_RHI)
#include <QFile>
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <algorithm>
#include <array>
#endif

namespace lucent {

RhiSurface::RhiSurface(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void RhiSurface::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (canvas_ != nullptr) {
        canvas_->setGeometry(rect());
    }
}

void RhiSurface::notifyFrame() {
    emit frameReady();
}

void RhiSurface::notifyFailed() {
    emit failed();
}

#if defined(LUCENT_HAS_RHI)

QShader loadShader(const QString &name) {
    QFile file(findShareDir() + QStringLiteral("/shaders/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

float ndcY(float viewY, float height, bool yUp) {
    const float unit = (viewY / height) * 2.0F - 1.0F;
    return yUp ? -unit : unit;
}

std::array<float, 24> quadFor(const Placement &placement, int imageW, int imageH, float viewW, float viewH, bool yUp) {
    const float left = static_cast<float>(-placement.originX * placement.scale);
    const float top = static_cast<float>(-placement.originY * placement.scale);
    const float right = left + static_cast<float>(imageW) * static_cast<float>(placement.scale);
    const float bottom = top + static_cast<float>(imageH) * static_cast<float>(placement.scale);
    const auto xOf = [viewW](float viewX) { return (viewX / viewW) * 2.0F - 1.0F; };
    const auto yOf = [viewH, yUp](float viewY) { return ndcY(viewY, viewH, yUp); };
    const float x0 = xOf(left);
    const float x1 = xOf(right);
    const float yTop = yOf(top);
    const float yBottom = yOf(bottom);
    return {
        x0, yBottom, 0.0F, 1.0F, x1, yBottom, 1.0F, 1.0F, x0, yTop, 0.0F, 0.0F,
        x0, yTop,    0.0F, 0.0F, x1, yBottom, 1.0F, 1.0F, x1, yTop, 1.0F, 0.0F,
    };
}

class Canvas : public QRhiWidget {
public:
    Canvas(RhiSurface *surface, QShader vertex, QShader fragment)
        : QRhiWidget(surface), surface_(surface), vertex_(std::move(vertex)), fragment_(std::move(fragment)) {
#ifdef Q_OS_WIN
        setApi(QRhiWidget::Api::Direct3D12);
#elif defined(Q_OS_MACOS)
        setApi(QRhiWidget::Api::Metal);
#else
        setApi(QRhiWidget::Api::Vulkan);
#endif
        connect(this, &QRhiWidget::renderFailed, surface_, [surface] { surface->notifyFailed(); });
    }

    void setImage(const QImage &image) {
        if (image.format() == QImage::Format_RGBA16FPx4 || image.format() == QImage::Format_RGBX16FPx4) {
            image_ = image.convertToFormat(QImage::Format_RGBA16FPx4);
        } else {
            image_ = image.convertToFormat(QImage::Format_RGBA8888);
        }
        imageDirty_ = true;
        textureDirty_ = true;
        update();
    }

    void setPlacement(const Placement &placement) {
        placement_ = placement;
        quadDirty_ = true;
        update();
    }

    void initialize(QRhiCommandBuffer *) override {
        if (rhi() == nullptr || image_.isNull() || !vertex_.isValid() || !fragment_.isValid()) {
            return;
        }
        if (rhiToken_ != rhi()) {
            releaseResources();
            rhiToken_ = rhi();
            imageDirty_ = true;
            textureDirty_ = true;
            quadDirty_ = true;
        }
        ensurePipeline();
    }

    void render(QRhiCommandBuffer *cb) override {
        if (rhi() == nullptr || renderTarget() == nullptr || colorTexture() == nullptr || image_.isNull()) {
            return;
        }
        if (pipeline_ == nullptr) {
            ensurePipeline();
        }
        if (pipeline_ == nullptr || texture_ == nullptr || vertices_ == nullptr) {
            return;
        }
        const QSize pixels = colorTexture()->pixelSize();
        QRhiResourceUpdateBatch *batch = nullptr;
        const bool yUp = rhi()->isYUpInNDC();
        if (imageDirty_ || quadDirty_ || yUp != yUp_) {
            batch = rhi()->nextResourceUpdateBatch();
            if (imageDirty_) {
                batch->uploadTexture(texture_, image_);
                imageDirty_ = false;
            }
            if (quadDirty_ || yUp != yUp_) {
                const std::array<float, 24> quad = quadFor(placement_, image_.width(), image_.height(),
                                                            static_cast<float>(std::max(1, width())),
                                                            static_cast<float>(std::max(1, height())), yUp);
                batch->updateDynamicBuffer(vertices_, 0, static_cast<int>(quad.size() * sizeof(float)), quad.data());
                quadDirty_ = false;
                yUp_ = yUp;
            }
        }
        cb->beginPass(renderTarget(), QColor(QStringLiteral("#17171c")), {1.0F, 0}, batch);
        cb->setGraphicsPipeline(pipeline_);
        cb->setViewport(QRhiViewport(0, 0, pixels.width(), pixels.height()));
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput input(vertices_, 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(6);
        cb->endPass();
        if (!reported_) {
            reported_ = true;
            QMetaObject::invokeMethod(surface_, [surface = surface_] { surface->notifyFrame(); }, Qt::QueuedConnection);
        }
    }

    void releaseResources() override {
        delete pipeline_;
        delete bindings_;
        delete vertices_;
        delete texture_;
        delete sampler_;
        pipeline_ = nullptr;
        bindings_ = nullptr;
        vertices_ = nullptr;
        texture_ = nullptr;
        sampler_ = nullptr;
        rhiToken_ = nullptr;
    }

private:
    void ensurePipeline() {
        QRhi *device = rhi();
        if (device == nullptr || image_.isNull() || renderTarget() == nullptr) {
            return;
        }
        if (pipeline_ != nullptr && texture_ != nullptr && !textureDirty_) {
            return;
        }
        if (pipeline_ != nullptr) {
            releaseResources();
            rhiToken_ = device;
        }
        const bool half = image_.format() == QImage::Format_RGBA16FPx4;
        texture_ = device->newTexture(half ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, image_.size());
        if (!texture_->create()) {
            return;
        }
        sampler_ = device->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!sampler_->create()) {
            return;
        }
        vertices_ = device->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 24 * sizeof(float));
        if (!vertices_->create()) {
            return;
        }
        bindings_ = device->newShaderResourceBindings();
        bindings_->setBindings({QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage, texture_, sampler_)});
        if (!bindings_->create()) {
            return;
        }
        pipeline_ = device->newGraphicsPipeline();
        pipeline_->setShaderStages({{QRhiShaderStage::Vertex, vertex_}, {QRhiShaderStage::Fragment, fragment_}});
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(16)});
        layout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 8),
        });
        pipeline_->setVertexInputLayout(layout);
        pipeline_->setShaderResourceBindings(bindings_);
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
        pipeline_->setSampleCount(sampleCount());
        if (!pipeline_->create()) {
            releaseResources();
            return;
        }
        textureDirty_ = false;
        imageDirty_ = true;
        quadDirty_ = true;
    }

    RhiSurface *surface_ = nullptr;
    QShader vertex_;
    QShader fragment_;
    QImage image_;
    Placement placement_;
    QRhi *rhiToken_ = nullptr;
    QRhiGraphicsPipeline *pipeline_ = nullptr;
    QRhiShaderResourceBindings *bindings_ = nullptr;
    QRhiBuffer *vertices_ = nullptr;
    QRhiTexture *texture_ = nullptr;
    QRhiSampler *sampler_ = nullptr;
    bool imageDirty_ = false;
    bool textureDirty_ = true;
    bool quadDirty_ = true;
    bool yUp_ = false;
    bool reported_ = false;
};

RhiSurface *RhiSurface::tryCreate(QWidget *parent) {
    if (qEnvironmentVariable("LUCENT_RHI", QStringLiteral("1")) == QStringLiteral("0")) {
        return nullptr;
    }
    const QShader vertex = loadShader(QStringLiteral("tex.vert.qsb"));
    const QShader fragment = loadShader(QStringLiteral("tex.frag.qsb"));
    if (!vertex.isValid() || !fragment.isValid()) {
        return nullptr;
    }
    auto *surface = new RhiSurface(parent);
    auto *canvas = new Canvas(surface, vertex, fragment);
    if (displayPrefersHdr()) {
        canvas->setColorBufferFormat(QRhiWidget::TextureFormat::RGBA16F);
    }
    canvas->setAttribute(Qt::WA_TransparentForMouseEvents);
    canvas->setGeometry(surface->rect());
    surface->canvas_ = canvas;
    return surface;
}

void RhiSurface::setImage(const QImage &image) {
    if (auto *canvas = dynamic_cast<Canvas *>(canvas_)) {
        canvas->setImage(image);
    }
}

void RhiSurface::setPlacement(const Placement &placement) {
    if (auto *canvas = dynamic_cast<Canvas *>(canvas_)) {
        canvas->setPlacement(placement);
    }
}

#else

RhiSurface *RhiSurface::tryCreate(QWidget *) {
    return nullptr;
}

void RhiSurface::setImage(const QImage &) {}

void RhiSurface::setPlacement(const Placement &) {}

#endif

}  // namespace lucent
