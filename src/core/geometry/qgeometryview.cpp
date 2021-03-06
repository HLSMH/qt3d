/****************************************************************************
**
** Copyright (C) 2020 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeometryview.h"
#include "qgeometryview_p.h"
#include "qgeometry_p.h"
#include "buffervisitor_p.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/private/vector3d_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace {

class FindExtremePoints : public Buffer3fVisitor
{
public:
    FindExtremePoints()
        : Buffer3fVisitor()
          , xMin(0.0f), xMax(0.0f), yMin(0.0f), yMax(0.0f), zMin(0.0f), zMax(0.0f)
    { }

    float xMin, xMax, yMin, yMax, zMin, zMax;
    Vector3D xMinPt, xMaxPt, yMinPt, yMaxPt, zMinPt, zMaxPt;

    void visit(uint ndx, float x, float y, float z) override
    {
        if (ndx) {
            if (x < xMin) {
                xMin = x;
                xMinPt = Vector3D(x, y, z);
            }
            if (x > xMax) {
                xMax = x;
                xMaxPt = Vector3D(x, y, z);
            }
            if (y < yMin) {
                yMin = y;
                yMinPt = Vector3D(x, y, z);
            }
            if (y > yMax) {
                yMax = y;
                yMaxPt = Vector3D(x, y, z);
            }
            if (z < zMin) {
                zMin = z;
                zMinPt = Vector3D(x, y, z);
            }
            if (z > zMax) {
                zMax = z;
                zMaxPt = Vector3D(x, y, z);
            }
        } else {
            xMin = xMax = x;
            yMin = yMax = y;
            zMin = zMax = z;
            xMinPt = xMaxPt = yMinPt = yMaxPt = zMinPt = zMaxPt = Vector3D(x, y, z);
        }
    }
};

class FindMaxDistantPoint : public Buffer3fVisitor
{
public:
    FindMaxDistantPoint()
        : Buffer3fVisitor()
    { }

    float maxLengthSquared = 0.0f;
    bool setReferencePoint = false;
    bool hasNoPoints = true;
    Vector3D maxDistPt;
    Vector3D referencePt;

    void visit(uint ndx, float x, float y, float z) override
    {
        Q_UNUSED(ndx)
        const Vector3D p = Vector3D(x, y, z);

        if (hasNoPoints && setReferencePoint) {
            maxLengthSquared = 0.0f;
            referencePt = p;
        }
        const float lengthSquared = (p - referencePt).lengthSquared();
        if ( lengthSquared >= maxLengthSquared ) {
            maxDistPt = p;
            maxLengthSquared = lengthSquared;
        }
        hasNoPoints = false;
    }
};

std::pair<QVector3D, QVector3D> calculateLocalBoundingVolume(QGeometryView *node)
{
    // The Bounding volume will only be computed if the position Buffer
    // isDirty

    if (!node->isEnabled())
        return {};

    if (node->primitiveType() == QGeometryView::Patches)
        return {};

    QGeometry *geom = node->geometry();
    QGeometryPrivate *dgeom = QGeometryPrivate::get(geom);

    if (!geom)
        return {};

    int drawVertexCount = node->vertexCount(); // may be 0, gets changed below if so

    QAttribute *positionAttribute = dgeom->m_boundingVolumePositionAttribute;
    const QVector<Qt3DCore::QAttribute *> attributes = geom->attributes();

    // Use the default position attribute if attribute is null
    if (!positionAttribute) {
        for (QAttribute *attr : attributes) {
            if (attr->name() == QAttribute::defaultPositionAttributeName()) {
                positionAttribute = attr;
                break;
            }
        }
    }

    if (!positionAttribute
        || positionAttribute->attributeType() != QAttribute::VertexAttribute
        || positionAttribute->vertexBaseType() != QAttribute::Float
        || positionAttribute->vertexSize() < 3) {
        qWarning("calculateLocalBoundingVolume: Position attribute not suited for bounding volume computation");
        return {};
    }

    Qt3DCore::QBuffer *buf = positionAttribute->buffer();
    // No point in continuing if the positionAttribute doesn't have a suitable buffer
    if (!buf) {
        qWarning("calculateLocalBoundingVolume: Position attribute not referencing a valid buffer");
        return {};
    }

    // Check if there is an index attribute.
    QAttribute *indexAttribute = nullptr;
    Qt3DCore::QBuffer *indexBuf = nullptr;

    for (const auto attr : attributes) {
        if (attr->attributeType() == QAttribute::IndexAttribute) {
            indexBuf = attr->buffer();
            if (indexBuf) {
                indexAttribute = attr;

                if (!drawVertexCount)
                    drawVertexCount = static_cast<int>(indexAttribute->count());

                static const QAttribute::VertexBaseType validIndexTypes[] = {
                    QAttribute::UnsignedShort,
                    QAttribute::UnsignedInt,
                    QAttribute::UnsignedByte
                };

                if (std::find(std::begin(validIndexTypes),
                              std::end(validIndexTypes),
                              indexAttribute->vertexBaseType()) == std::end(validIndexTypes)) {
                    qWarning() << "calculateLocalBoundingVolume: Unsupported index attribute type" << indexAttribute->name() << indexAttribute->vertexBaseType();
                    return {};
                }

                break;
            }
        }
    }

    if (!indexAttribute && !drawVertexCount)
        drawVertexCount = static_cast<int>(positionAttribute->count());

    // Buf will be set to not dirty once it's loaded
    // in a job executed after this one
    // We need to recompute the bounding volume
    // If anything in the GeometryRenderer has changed
    BoundingVolumeCalculator reader;
    if (reader.apply(positionAttribute, indexAttribute, drawVertexCount,
                     node->primitiveRestartEnabled(), node->restartIndexValue()))
        return {reader.min(), reader.max()};

    return {};
}

}


bool BoundingVolumeCalculator::apply(QAttribute *positionAttribute,
                                     QAttribute *indexAttribute,
                                     int drawVertexCount,
                                     bool primitiveRestartEnabled,
                                     int primitiveRestartIndex)
{
    m_radius = -1.f;

    FindExtremePoints findExtremePoints;
    if (!findExtremePoints.apply(positionAttribute, indexAttribute, drawVertexCount,
                                 primitiveRestartEnabled, primitiveRestartIndex)) {
        return false;
    }

    m_min = QVector3D(findExtremePoints.xMin, findExtremePoints.yMin, findExtremePoints.zMin);
    m_max = QVector3D(findExtremePoints.xMax, findExtremePoints.yMax, findExtremePoints.zMax);

    FindMaxDistantPoint maxDistantPointY;
    maxDistantPointY.setReferencePoint = true;
    if (!maxDistantPointY.apply(positionAttribute, indexAttribute, drawVertexCount,
                                primitiveRestartEnabled, primitiveRestartIndex)) {
        return false;
    }
    if (maxDistantPointY.hasNoPoints)
        return false;

    //const Vector3D x = maxDistantPointY.referencePt;
    const Vector3D y = maxDistantPointY.maxDistPt;

    FindMaxDistantPoint maxDistantPointZ;
    maxDistantPointZ.setReferencePoint = false;
    maxDistantPointZ.referencePt = y;
    if (!maxDistantPointZ.apply(positionAttribute, indexAttribute, drawVertexCount,
                                primitiveRestartEnabled, primitiveRestartIndex))
        return false;
    const Vector3D z = maxDistantPointZ.maxDistPt;
    const Vector3D center = (y + z) * .5f;

    FindMaxDistantPoint maxDistantPointCenter;
    maxDistantPointCenter.setReferencePoint = false;
    maxDistantPointCenter.referencePt = center;
    if (!maxDistantPointCenter.apply(positionAttribute, indexAttribute, drawVertexCount,
                                     primitiveRestartEnabled, primitiveRestartIndex))
        return false;

    const float radius = (center - maxDistantPointCenter.maxDistPt).length();

    if (center == Vector3D{} && radius < 0.f)
        return false;

    m_radius = radius;
    m_center = QVector3D{ center.x(), center.y(), center.z() };

    return true;
}


QGeometryViewPrivate::QGeometryViewPrivate()
    : QNodePrivate()
    , m_instanceCount(1)
    , m_vertexCount(0)
    , m_indexOffset(0)
    , m_firstInstance(0)
    , m_firstVertex(0)
    , m_indexBufferByteOffset(0)
    , m_restartIndexValue(-1)
    , m_verticesPerPatch(0)
    , m_primitiveRestart(false)
    , m_geometry(nullptr)
    , m_primitiveType(QGeometryView::Triangles)
    , m_dirty(false)
{
}

QGeometryViewPrivate::~QGeometryViewPrivate()
{
}

QGeometryViewPrivate *QGeometryViewPrivate::get(QGeometryView *q)
{
    return q->d_func();
}

void QGeometryViewPrivate::update()
{
    if (!m_blockNotifications)
        m_dirty = true;
    QNodePrivate::update();
}

/*!
    \qmltype GeometryView
    \instantiates Qt3DCore::QGeometryView
    \inqmlmodule Qt3D.Core
    \inherits Node
    \since 6.0
    \brief Encapsulates geometry details.

    A GeometryView holds all the information necessary to handle
    a Geometry. A Geometry holds the coordinates of the geometry data -
    GeometryView specifies how to interpret that data.
 */

/*!
    \class Qt3DCore::QGeometryView
    \inmodule Qt3DCore
    \since 6.0
    \brief Encapsulates geometry details.

    A GeometryView holds all the information necessary to handle
    a Geometry. A Geometry holds the coordinates of the geometry data -
    GeometryView specifies how to interpret that data.
 */


/*!
    \enum QGeometryView::PrimitiveType

    The type of the primitive.

    \value Points   List of points
    \value Lines    List of lines
    \value LineLoop Connected group of lines connected at ends forming a loop
    \value LineStrip Connected group of lines
    \value Triangles List of triangles
    \value TriangleStrip List of connected triangles
    \value TriangleFan List of connected triagles where all triangles share the first vertex
    \value LinesAdjacency Allows geometry shader to access adjacent lines in a line list
    \value TrianglesAdjacency Allows geometry shader to access adjacent triangles in a triangle list
    \value LineStripAdjacency Allows geometry shader to access adjacent lines in a line strip
    \value TriangleStripAdjacency Allows geometry shader to access adjacent triangles in a triangle strip
    \value Patches Only primitive type accepted by tesselation shader where a patch consists of arbitrary number of vertices
 */

/*!
    \qmlproperty int GeometryView::instanceCount

    Holds the instance count.
 */

/*!
    \qmlproperty int GeometryView::vertexCount

    Holds the vertex count.
 */

/*!
    \qmlproperty int GeometryView::indexOffset

    Holds the base vertex.
 */

/*!
    \qmlproperty int GeometryView::firstInstance

    Holds the base instance.
 */

/*!
    \qmlproperty int GeometryView::firstVertex

    Holds the first vertex.
 */

/*!
    \qmlproperty int GeometryView::indexBufferByteOffset

    Holds the byte offset into the index buffer.
 */

/*!
    \qmlproperty int GeometryView::restartIndex

    Holds the restart index.
 */

/*!
    \qmlproperty int GeometryView::verticesPerPatch

    Holds vertices per patch.
 */

/*!
    \qmlproperty bool GeometryView::primitiveRestart

    Holds the primitive restart flag.
 */

/*!
    \qmlproperty Geometry GeometryView::geometry

    Holds the geometry.
 */

/*!
    \qmlproperty enumeration GeometryView::primitiveType

    Holds the primitive type.

    \list
    \li QGeometryView.Points
    \li QGeometryView.Lines
    \li QGeometryView.LineLoop
    \li QGeometryView.LineStrip
    \li QGeometryView.Triangles
    \li QGeometryView.TriangleStrip
    \li QGeometryView.TriangleFan
    \li QGeometryView.LinesAdjacency
    \li QGeometryView.TrianglesAdjacency
    \li QGeometryView.LineStripAdjacency
    \li QGeometryView.TriangleStripAdjacency
    \li QGeometryView.Patches
    \endlist
    \sa Qt3DCore::QGeometryView::PrimitiveType
 */


/*!
    Constructs a new QGeometryView with \a parent.
 */
QGeometryView::QGeometryView(QNode *parent)
    : QNode(*new QGeometryViewPrivate(), parent)
{
}

/*!
    \internal
 */
QGeometryView::~QGeometryView()
{
}

/*!
    \internal
 */
QGeometryView::QGeometryView(QGeometryViewPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

/*!
    \property QGeometryView::instanceCount

    Holds the instance count.
 */
int QGeometryView::instanceCount() const
{
    Q_D(const QGeometryView);
    return d->m_instanceCount;
}

/*!
    \property QGeometryView::vertexCount

    Holds the primitive count.
 */
int QGeometryView::vertexCount() const
{
    Q_D(const QGeometryView);
    return d->m_vertexCount;
}

/*!
    \property QGeometryView::indexOffset

    Holds the base vertex.
 */
int QGeometryView::indexOffset() const
{
    Q_D(const QGeometryView);
    return d->m_indexOffset;
}

/*!
    \property QGeometryView::firstInstance

    Holds the base instance.
 */
int QGeometryView::firstInstance() const
{
    Q_D(const QGeometryView);
    return d->m_firstInstance;
}

/*!
    \property QGeometryView::firstVertex

    Holds the base vertex.
 */
int QGeometryView::firstVertex() const
{
    Q_D(const QGeometryView);
    return d->m_firstVertex;
}

/*!
    \property QGeometryView::indexBufferByteOffset

    Holds the byte offset into the index buffer.
 */
int QGeometryView::indexBufferByteOffset() const
{
    Q_D(const QGeometryView);
    return d->m_indexBufferByteOffset;
}

/*!
    \property QGeometryView::restartIndexValue

    Holds the restart index.
 */
int QGeometryView::restartIndexValue() const
{
    Q_D(const QGeometryView);
    return d->m_restartIndexValue;
}

/*!
    \property QGeometryView::verticesPerPatch

    Holds vertices per patch.
 */
int QGeometryView::verticesPerPatch() const
{
    Q_D(const QGeometryView);
    return d->m_verticesPerPatch;
}

/*!
    \property QGeometryView::primitiveRestartEnabled

    Holds the primitive restart flag.
 */
bool QGeometryView::primitiveRestartEnabled() const
{
    Q_D(const QGeometryView);
    return d->m_primitiveRestart;
}

/*!
    \property QGeometryView::geometry

    Holds the geometry.
 */
QGeometry *QGeometryView::geometry() const
{
    Q_D(const QGeometryView);
    return d->m_geometry;
}

/*!
    \property QGeometryView::primitiveType

    Holds the primitive type.
 */
QGeometryView::PrimitiveType QGeometryView::primitiveType() const
{
    Q_D(const QGeometryView);
    return d->m_primitiveType;
}

void QGeometryView::setInstanceCount(int instanceCount)
{
    Q_D(QGeometryView);
    if (d->m_instanceCount == instanceCount)
        return;

    d->m_instanceCount = instanceCount;
    emit instanceCountChanged(instanceCount);
}

void QGeometryView::setVertexCount(int vertexCount)
{
    Q_D(QGeometryView);
    if (d->m_vertexCount == vertexCount)
        return;

    d->m_vertexCount = vertexCount;
    emit vertexCountChanged(vertexCount);
}

void QGeometryView::setIndexOffset(int indexOffset)
{
    Q_D(QGeometryView);
    if (d->m_indexOffset == indexOffset)
        return;

    d->m_indexOffset = indexOffset;
    emit indexOffsetChanged(indexOffset);
}

void QGeometryView::setFirstInstance(int firstInstance)
{
    Q_D(QGeometryView);
    if (d->m_firstInstance == firstInstance)
        return;

    d->m_firstInstance = firstInstance;
    emit firstInstanceChanged(firstInstance);
}

void QGeometryView::setFirstVertex(int firstVertex)
{
    Q_D(QGeometryView);
    if (d->m_firstVertex == firstVertex)
        return;

    d->m_firstVertex = firstVertex;
    emit firstVertexChanged(firstVertex);
}

void QGeometryView::setIndexBufferByteOffset(int offset)
{
    Q_D(QGeometryView);
    if (d->m_indexBufferByteOffset == offset)
        return;

    d->m_indexBufferByteOffset = offset;
    emit indexBufferByteOffsetChanged(offset);
}

void QGeometryView::setRestartIndexValue(int index)
{
    Q_D(QGeometryView);
    if (index == d->m_restartIndexValue)
        return;

    d->m_restartIndexValue = index;
    emit restartIndexValueChanged(index);
}

void QGeometryView::setVerticesPerPatch(int verticesPerPatch)
{
    Q_D(QGeometryView);
    if (d->m_verticesPerPatch != verticesPerPatch) {
        d->m_verticesPerPatch = verticesPerPatch;
        emit verticesPerPatchChanged(verticesPerPatch);
    }
}

void QGeometryView::setPrimitiveRestartEnabled(bool enabled)
{
    Q_D(QGeometryView);
    if (enabled == d->m_primitiveRestart)
        return;

    d->m_primitiveRestart = enabled;
    emit primitiveRestartEnabledChanged(enabled);
}

void QGeometryView::setGeometry(QGeometry *geometry)
{
    Q_D(QGeometryView);
    if (d->m_geometry == geometry)
        return;

    if (d->m_geometry)
        d->unregisterDestructionHelper(d->m_geometry);

    if (geometry && !geometry->parent())
        geometry->setParent(this);

    d->m_geometry = geometry;

    // Ensures proper bookkeeping
    if (d->m_geometry)
        d->registerDestructionHelper(d->m_geometry, &QGeometryView::setGeometry, d->m_geometry);

    emit geometryChanged(geometry);
}

void QGeometryView::setPrimitiveType(QGeometryView::PrimitiveType primitiveType)
{
    Q_D(QGeometryView);
    if (d->m_primitiveType == primitiveType)
        return;

    d->m_primitiveType = primitiveType;
    emit primitiveTypeChanged(primitiveType);
}

QT_END_NAMESPACE
