"""GPU image view. One texture, one prebaked shader, letterboxed in the widget."""

from __future__ import annotations

import sys
from importlib.resources import files

from PySide6.QtCore import QSize
from PySide6.QtGui import (
    QColor,
    QImage,
    QRhiBuffer,
    QRhiDepthStencilClearValue,
    QRhiGraphicsPipeline,
    QRhiSampler,
    QRhiShaderResourceBinding,
    QRhiShaderStage,
    QRhiTexture,
    QRhiVertexInputAttribute,
    QRhiVertexInputBinding,
    QRhiVertexInputLayout,
    QRhiViewport,
    QShader,
)
from PySide6.QtWidgets import QRhiWidget

from voxprint_lucent.fit import contain_vertices, vertices_bytes
from voxprint_lucent.theme_tokens import ROOT_PLAIN

_CLEAR = QColor(ROOT_PLAIN)
_VERTEX_BYTES = 6 * 4 * 4


def preferred_api() -> QRhiWidget.Api:
    """D3D12 on Windows, Vulkan on Linux, Metal where Qt is running on Apple."""
    if sys.platform == "win32":
        return QRhiWidget.Api.Direct3D12
    if sys.platform == "darwin":
        return QRhiWidget.Api.Metal
    return QRhiWidget.Api.Vulkan


def _load_shader(name: str) -> QShader:
    data = (files("voxprint_lucent.shaders") / name).read_bytes()
    shader = QShader.fromSerialized(data)
    if not shader.isValid():
        raise RuntimeError(f"Invalid shader pack: {name}")
    return shader


class ImageRhiWidget(QRhiWidget):
    """Draws one image with a Qt RHI widget."""
    def __init__(self) -> None:
        """Create an empty RHI surface."""
        super().__init__()
        self.setApi(preferred_api())
        self.setMinimumSize(QSize(320, 200))
        self._rgba: QImage | None = None
        self._image_dirty = False
        self._generation = 0
        self.presented_generation = 0
        self._rhi_id: int | None = None
        self._pipeline = None
        self._vbuf = None
        self._srb = None
        self._tex = None
        self._sampler = None
        self._quad_key: tuple[int, int, bool] | None = None
        self._kept: list[object] = []

    def set_image(self, image: QImage) -> None:
        """Replace the image drawn on the next frame."""
        if image.format() != QImage.Format.Format_RGBA8888:
            image = image.convertToFormat(QImage.Format.Format_RGBA8888)
        self._rgba = image
        self._image_dirty = True
        self._generation += 1
        self.update()

    @property
    def generation(self) -> int:
        """Return how many times the image has been replaced."""
        return self._generation

    def releaseResources(self) -> None:  # noqa: N802 - Qt override
        """Drop GPU resources Qt is done with."""
        for obj in (self._pipeline, self._vbuf, self._srb, self._tex, self._sampler):
            if obj is not None:
                obj.destroy()
        self._pipeline = None
        self._vbuf = None
        self._srb = None
        self._tex = None
        self._sampler = None
        self._rhi_id = None
        self._quad_key = None
        self._kept.clear()

    def initialize(self, cb) -> None:  # noqa: ARG002 - Qt override
        """Prepare the pipeline before the first frame."""
        rhi = self.rhi()
        if rhi is None or self._rgba is None:
            return
        if self._rhi_id != id(rhi):
            self.releaseResources()
            self._rhi_id = id(rhi)
            self._image_dirty = True
        self._ensure_pipeline()

    def render(self, cb) -> None:
        """Draw the current image."""
        if self._rgba is None or self.rhi() is None:
            return
        if self._pipeline is None:
            self._ensure_pipeline()
        if self._pipeline is None or self._tex is None or self._vbuf is None:
            return
        rhi = self.rhi()
        target = self.renderTarget()
        color = self.colorTexture()
        if rhi is None or target is None or color is None:
            return
        size = color.pixelSize()
        batch = self._pending_updates(rhi, size)
        cb.beginPass(target, _CLEAR, QRhiDepthStencilClearValue(), batch)
        cb.setGraphicsPipeline(self._pipeline)
        cb.setViewport(QRhiViewport(0, 0, size.width(), size.height()))
        cb.setShaderResources()
        cb.setVertexInput(0, [(self._vbuf, 0)])
        cb.draw(6)
        cb.endPass()
        self.presented_generation = self._generation

    def _pending_updates(self, rhi, size):  # type: ignore[no-untyped-def]
        y_up = bool(rhi.isYUpInNDC())
        key = (size.width(), size.height(), y_up)
        needs_quad = key != self._quad_key
        if not self._image_dirty and not needs_quad:
            return None
        batch = rhi.nextResourceUpdateBatch()
        if self._image_dirty and self._tex is not None and self._rgba is not None:
            batch.uploadTexture(self._tex, self._rgba)
            self._image_dirty = False
        if needs_quad and self._vbuf is not None and self._rgba is not None:
            vertices = contain_vertices(
                self._rgba.width(),
                self._rgba.height(),
                max(1, size.width()),
                max(1, size.height()),
                y_up=y_up,
            )
            batch.updateDynamicBuffer(self._vbuf, 0, vertices_bytes(vertices))
            self._quad_key = key
        return batch

    def _ensure_pipeline(self) -> None:
        rhi = self.rhi()
        image = self._rgba
        if rhi is None or image is None:
            return
        if self._pipeline is not None and self._tex is not None:
            if self._tex.pixelSize() == image.size():
                return
            self.releaseResources()
            self._rhi_id = id(rhi)
            self._image_dirty = True
        tex = rhi.newTexture(QRhiTexture.Format.RGBA8, image.size())
        if not tex.create():
            return
        sampler = rhi.newSampler(
            QRhiSampler.Filter.Linear,
            QRhiSampler.Filter.Linear,
            QRhiSampler.Filter.None_,
            QRhiSampler.AddressMode.ClampToEdge,
            QRhiSampler.AddressMode.ClampToEdge,
        )
        if not sampler.create():
            return
        vbuf = rhi.newBuffer(
            QRhiBuffer.Type.Dynamic,
            QRhiBuffer.UsageFlag.VertexBuffer,
            _VERTEX_BYTES,
        )
        if not vbuf.create():
            return
        srb = rhi.newShaderResourceBindings()
        srb.setBindings(
            [
                QRhiShaderResourceBinding.sampledTexture(
                    0,
                    QRhiShaderResourceBinding.StageFlag.FragmentStage,
                    tex,
                    sampler,
                )
            ]
        )
        if not srb.create():
            return
        pipeline = rhi.newGraphicsPipeline()
        pipeline.setShaderStages(
            [
                QRhiShaderStage(QRhiShaderStage.Type.Vertex, _load_shader("tex.vert.qsb")),
                QRhiShaderStage(QRhiShaderStage.Type.Fragment, _load_shader("tex.frag.qsb")),
            ]
        )
        binding = QRhiVertexInputBinding(16)
        attr_pos = QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute.Format.Float2, 0)
        attr_uv = QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute.Format.Float2, 8)
        layout = QRhiVertexInputLayout()
        layout.setBindings([binding])
        layout.setAttributes([attr_pos, attr_uv])
        self._kept.extend([binding, attr_pos, attr_uv, layout])
        pipeline.setVertexInputLayout(layout)
        pipeline.setShaderResourceBindings(srb)
        pipeline.setRenderPassDescriptor(self.renderTarget().renderPassDescriptor())
        pipeline.setTopology(QRhiGraphicsPipeline.Topology.Triangles)
        pipeline.setSampleCount(self.sampleCount())
        if not pipeline.create():
            return
        self._tex = tex
        self._sampler = sampler
        self._vbuf = vbuf
        self._srb = srb
        self._pipeline = pipeline
