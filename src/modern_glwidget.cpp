/***************************************************************************
 *   Copyright (C) 2008-2013 by Heiko Koehn - KoehnHeiko@googlemail.com    *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *   Copyright (C) 2014, 2016, 2019-2021, 2023 by Stephen Lyons            *
 *                                               - slysven@virginmedia.com *
 *   Copyright (C) 2025 by Vadim Peretokin - vadim.peretokin@mudlet.org    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "modern_glwidget.h"

#include "Host.h"
#include "TArea.h"
#include "TRoom.h"
#include "TRoomDB.h"
#include "dlgMapper.h"
#include "mudlet.h"

#include "pre_guard.h"
#include <QtEvents>
#include <QDebug>
#include <QPainter>
#include "post_guard.h"

// Modern OpenGL vertex shader with lighting
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec3 aNormal;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

// Lighting uniforms (from original glwidget.cpp)
uniform vec3 uLight0Pos = vec3(5000.0, 4000.0, 1000.0);
uniform vec3 uLight1Pos = vec3(5000.0, 1000.0, 1000.0);
uniform vec3 uLight0Diffuse = vec3(0.507, 0.507, 0.507);
uniform vec3 uLight1Diffuse = vec3(0.501, 0.901, 0.501);
uniform vec3 uLight0Ambient = vec3(0.403, 0.403, 0.403);
uniform vec3 uLight1Ambient = vec3(0.4501, 0.4501, 0.4501);

out vec4 vertexColor;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vec3 worldNormal = normalize(uNormalMatrix * aNormal);
    
    // Calculate lighting (similar to original fixed-function pipeline)
    vec3 ambient = uLight0Ambient + uLight1Ambient;
    
    // Light 0
    vec3 lightDir0 = normalize(uLight0Pos);
    float diff0 = max(dot(worldNormal, lightDir0), 0.0);
    vec3 diffuse0 = diff0 * uLight0Diffuse;
    
    // Light 1  
    vec3 lightDir1 = normalize(uLight1Pos);
    float diff1 = max(dot(worldNormal, lightDir1), 0.0);
    vec3 diffuse1 = diff1 * uLight1Diffuse;
    
    // Combine lighting
    vec3 lighting = ambient + diffuse0 + diffuse1;
    lighting = clamp(lighting, 0.0, 1.0);
    
    // Apply lighting similar to glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE)
    // The input color IS the material ambient/diffuse, not a base color to be dimmed
    vec3 materialAmbientDiffuse = aColor.rgb;
    
    // Calculate final color: ambient contribution + diffuse contribution
    vec3 ambientContrib = (uLight0Ambient + uLight1Ambient) * materialAmbientDiffuse;
    vec3 diffuseContrib = (diffuse0 + diffuse1) * materialAmbientDiffuse;
    
    // Combine with higher brightness to match original
    vec3 finalColor = ambientContrib + diffuseContrib;
    
    vertexColor = vec4(finalColor, aColor.a);
    
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

// Modern OpenGL fragment shader
static const char* fragmentShaderSource = R"(
#version 330 core
in vec4 vertexColor;
out vec4 FragColor;

void main()
{
    FragColor = vertexColor;
}
)";

ModernGLWidget::ModernGLWidget(TMap* pMap, Host* pHost, QWidget* parent)
: QOpenGLWidget(parent), mVertexBuffer(QOpenGLBuffer::VertexBuffer), mColorBuffer(QOpenGLBuffer::VertexBuffer), mNormalBuffer(QOpenGLBuffer::VertexBuffer), mpMap(pMap), mpHost(pHost)
{
    if (mpHost->mBgColor_2.alpha() < 255) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAttribute(Qt::WA_AlwaysStackOnTop);
    } else {
        setAttribute(Qt::WA_OpaquePaintEvent);
    }
}

ModernGLWidget::~ModernGLWidget()
{
    cleanup();
}

void ModernGLWidget::cleanup()
{
    makeCurrent();
    delete mShaderProgram;
    mShaderProgram = nullptr;
    mVertexBuffer.destroy();
    mColorBuffer.destroy();
    mNormalBuffer.destroy();
    mVAO.destroy();
    doneCurrent();
}

QSize ModernGLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize ModernGLWidget::sizeHint() const
{
    return QSize(400, 400);
}

void ModernGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    const QColor color(mpHost->mBgColor_2);
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());

    // Initialize default view parameters
    xRot = 1;
    yRot = 5;
    zRot = 10;

    // Enable features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearDepth(1.0);

    is2DView = false;

    // Initialize shaders and buffers
    if (!initializeShaders()) {
        qWarning() << "Failed to initialize shaders";
        return;
    }

    setupBuffers();
}

bool ModernGLWidget::initializeShaders()
{
    mShaderProgram = new QOpenGLShaderProgram(this);

    // Add vertex shader
    if (!mShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Failed to compile vertex shader:" << mShaderProgram->log();
        return false;
    }

    // Add fragment shader
    if (!mShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qWarning() << "Failed to compile fragment shader:" << mShaderProgram->log();
        return false;
    }

    // Link shader program
    if (!mShaderProgram->link()) {
        qWarning() << "Failed to link shader program:" << mShaderProgram->log();
        return false;
    }

    // Get uniform locations
    mUniformMVP = mShaderProgram->uniformLocation("uMVP");
    mUniformModel = mShaderProgram->uniformLocation("uModel");
    mUniformNormalMatrix = mShaderProgram->uniformLocation("uNormalMatrix");

    if (mUniformMVP == -1) {
        qWarning() << "Failed to get MVP uniform location:" << mUniformMVP;
        return false;
    }

    // Note: uModel and uNormalMatrix may be -1 if optimized out by driver
    qDebug() << "ModernGLWidget: Shaders initialized successfully. Uniform locations - MVP:" << mUniformMVP << "Model:" << mUniformModel << "Normal:" << mUniformNormalMatrix;
    return true;
}

void ModernGLWidget::setupBuffers()
{
    // Create VAO
    mVAO.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVAO);

    // Create vertex buffer
    mVertexBuffer.create();
    mVertexBuffer.bind();
    mVertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Create color buffer
    mColorBuffer.create();
    mColorBuffer.bind();
    mColorBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Create normal buffer
    mNormalBuffer.create();
    mNormalBuffer.bind();
    mNormalBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Configure vertex attribute pointers (will be set during rendering)
}

void ModernGLWidget::updateMatrices()
{
    // Set up projection matrix with fixed FOV
    mProjectionMatrix.setToIdentity();
    const float aspectRatio = static_cast<float>(width()) / static_cast<float>(height());
    // Keep FOV constant at 60 degrees, adjust camera distance with scale instead
    mProjectionMatrix.perspective(60.0f, aspectRatio, 0.0001f, 10000.0f);

    // Set up view matrix (camera)
    mViewMatrix.setToIdentity();

    // Original uses xRot, yRot, zRot as camera position offsets, not rotation angles
    // gluLookAt(px * 0.1 + xRot, py * 0.1 + yRot, pz * 0.1 + zRot, px * 0.1, py * 0.1, pz * 0.1, 0.0, 1.0, 0.0);

    // Calculate camera position with offsets (scale appropriately for our coordinate system)
    const float px = static_cast<float>(mMapCenterX) * 0.1f;
    const float py = static_cast<float>(mMapCenterY) * 0.1f;
    const float pz = static_cast<float>(mMapCenterZ) * 0.1f;

    // Camera position with offsets, scaled by mScale for zoom
    // The original offsets are scaled by the zoom factor to maintain proper distance
    const float scaleMultiplier = 1.0f / mScale;
    const float cameraX = px + (xRot * scaleMultiplier);
    const float cameraY = py + (yRot * scaleMultiplier);
    const float cameraZ = pz + (zRot * scaleMultiplier);

    // Target position (map center)
    const float targetX = px;
    const float targetY = py;
    const float targetZ = pz;

    // Create view matrix to look at target from camera position
    mViewMatrix.lookAt(QVector3D(cameraX, cameraY, cameraZ), QVector3D(targetX, targetY, targetZ), QVector3D(0.0f, 1.0f, 0.0f));

    // Scale the world to match original rendering
    mViewMatrix.scale(0.1f, 0.1f, 0.1f);

    // Model matrix will be set per object during rendering
    mModelMatrix.setToIdentity();

    // qDebug() << "ModernGLWidget: View matrix updated. Camera distance:" << cameraDistance
    //          << "Scale:" << mScale << "Center:" << mMapCenterX << mMapCenterY << mMapCenterZ;
}

void ModernGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    updateMatrices();
}

void ModernGLWidget::paintGL()
{
    if (!mpMap || !mShaderProgram) {
        return;
    }

    float px, py, pz;
    if (mRID != mpMap->mRoomIdHash.value(mpMap->mProfileName) && mShiftMode) {
        mShiftMode = false;
    }

    int ox, oy, oz;
    if (!mShiftMode) {
        mRID = mpMap->mRoomIdHash.value(mpMap->mProfileName);
        TRoom* pRID = mpMap->mpRoomDB->getRoom(mRID);
        if (!pRID) {
            glClearDepth(1.0);
            glDepthFunc(GL_LESS);
            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            QPainter painter(this);
            painter.setPen(QColorConstants::White);
            painter.setFont(QFont("Bitstream Vera Sans Mono", 30));
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            QString message;
            if (mpMap->mpRoomDB) {
                if (mpMap->mpRoomDB->isEmpty()) {
                    message = tr("No rooms in the map - load another one, or start mapping from scratch to begin.");
                } else {
                    message = tr("You have a map loaded (%n room(s)), but Mudlet does not know where you are at the moment.", "", mpMap->mpRoomDB->size());
                }
            } else {
                message = tr("You do not have a map yet - load one, or start mapping from scratch to begin.");
            }
            painter.drawText(0, 0, (width() - 1), (height() - 1), Qt::AlignCenter | Qt::TextWordWrap, message);
            painter.end();

            return;
        }
        mAID = pRID->getArea();
        ox = pRID->x();
        oy = pRID->y();
        oz = pRID->z();
        mMapCenterX = ox;
        mMapCenterY = oy;
        mMapCenterZ = oz;

    } else {
        ox = mMapCenterX;
        oy = mMapCenterY;
        oz = mMapCenterZ;
    }

    px = static_cast<float>(ox);
    py = static_cast<float>(oy);
    pz = static_cast<float>(oz);

    TArea* pArea = mpMap->mpRoomDB->getArea(mAID);
    if (!pArea) {
        return;
    }

    if (pArea->gridMode) {
        xRot = 0.0;
        yRot = 0.0;
        zRot = 15.0;
    }

    zmax = static_cast<float>(pArea->max_z);
    zmin = static_cast<float>(pArea->min_z);

    // Clear the screen
    const QColor color(mpHost->mBgColor_2);
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update transformation matrices
    updateMatrices();

    // Use our shader program
    mShaderProgram->bind();

    // Render the map
    renderRooms();

    // Render connections between rooms
    renderConnections();

    mShaderProgram->release();
    
    // Draw label to identify this as the modern OpenGL implementation
    QPainter painter(this);
    painter.setPen(QPen(QColor(255, 255, 255, 200))); // Semi-transparent white
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(10, height() - 20, "Modern OpenGL Mapper");
    painter.end();
}

void ModernGLWidget::renderRooms()
{
    if (!mpMap || !mpMap->mpRoomDB) {
        qDebug() << "ModernGLWidget: No map or room database";
        return;
    }

    TArea* pArea = mpMap->mpRoomDB->getArea(mAID);
    if (!pArea) {
        qDebug() << "ModernGLWidget: No area found for ID:" << mAID;
        return;
    }

    float pz = static_cast<float>(mMapCenterZ);
    float px = static_cast<float>(mMapCenterX);
    float py = static_cast<float>(mMapCenterY);

    QSetIterator<int> itRoom(pArea->getAreaRooms());
    while (itRoom.hasNext()) {
        int currentRoomId = itRoom.next();
        TRoom* pR = mpMap->mpRoomDB->getRoom(currentRoomId);
        if (!pR) {
            continue;
        }

        auto rx = static_cast<float>(pR->x());
        auto ry = static_cast<float>(pR->y());
        auto rz = static_cast<float>(pR->z());

        // Level filtering logic from original
        if (rz > pz) {
            if (abs(rz - pz) > mShowTopLevels) {
                continue;
            }
        }
        if (rz < pz) {
            if (abs(rz - pz) > mShowBottomLevels) {
                continue;
            }
        }

        // Check special room states
        bool isCurrentRoom = (rz == pz) && (rx == px) && (ry == py);
        bool isTargetRoom = (currentRoomId == mTargetRoomId);
        bool belowOrAtLevel = (rz <= pz);

        // 1. Render main room cube using correct planeColor logic
        if (isCurrentRoom) {
            // Current room: red
            renderCube(rx, ry, rz, 1.0f / scale, 1.0f, 0.0f, 0.0f, 1.0f);
        } else if (isTargetRoom) {
            // Target room: green
            renderCube(rx, ry, rz, 1.0f / scale, 0.0f, 1.0f, 0.0f, 1.0f);
        } else {
            // Normal room: use planeColor logic based on z-level relationship
            QColor roomColor = getPlaneColor(static_cast<int>(rz), belowOrAtLevel);
            renderCube(rx, ry, rz, 1.0f / scale, roomColor.redF(), roomColor.greenF(), roomColor.blueF(),
                       1.0f); // Full alpha
        }

        // 2. Render thin environment color overlay on top
        // Disable depth testing like the original to prevent clipping
        glDisable(GL_DEPTH_TEST);

        QColor envColor = getEnvironmentColor(pR);
        float overlayZ = rz + 0.25f; // Slightly above the main cube
        renderCube(rx,
                   ry,
                   overlayZ,
                   0.75f / scale, // Slightly smaller and thinner
                   envColor.redF(),
                   envColor.greenF(),
                   envColor.blueF(),
                   0.8f); // Semi-transparent overlay

        // 3. Render up/down exit indicators on the overlay
        renderUpDownIndicators(pR, rx, ry, overlayZ + 0.1f);

        // Re-enable depth testing for subsequent rendering
        glEnable(GL_DEPTH_TEST);
    }
}

void ModernGLWidget::renderConnections()
{
    if (!mpMap || !mpMap->mpRoomDB) {
        return;
    }

    TArea* pArea = mpMap->mpRoomDB->getArea(mAID);
    if (!pArea) {
        return;
    }

    float pz = static_cast<float>(mMapCenterZ);

    // Collect all lines to draw
    QVector<float> lineVertices;
    QVector<float> lineColors;

    QSetIterator<int> itRoom(pArea->getAreaRooms());
    while (itRoom.hasNext()) {
        TRoom* pR = mpMap->mpRoomDB->getRoom(itRoom.next());
        if (!pR) {
            continue;
        }

        auto rx = static_cast<float>(pR->x());
        auto ry = static_cast<float>(pR->y());
        auto rz = static_cast<float>(pR->z());

        // Level filtering logic (same as rooms)
        if (rz > pz) {
            if (abs(rz - pz) > mShowTopLevels) {
                continue;
            }
        }
        if (rz < pz) {
            if (abs(rz - pz) > mShowBottomLevels) {
                continue;
            }
        }

        // Get all exits for this room
        QList<int> exitList;
        exitList.push_back(pR->getNorth());
        exitList.push_back(pR->getNortheast());
        exitList.push_back(pR->getEast());
        exitList.push_back(pR->getSoutheast());
        exitList.push_back(pR->getSouth());
        exitList.push_back(pR->getSouthwest());
        exitList.push_back(pR->getWest());
        exitList.push_back(pR->getNorthwest());
        exitList.push_back(pR->getUp());
        exitList.push_back(pR->getDown());

        // Check if this is the current room
        bool isCurrentRoom = (rz == pz) && (rx == static_cast<float>(mMapCenterX)) && (ry == static_cast<float>(mMapCenterY));

        // Color for connections: red if current room, gray otherwise
        float r, g, b;
        if (isCurrentRoom) {
            r = 1.0f;
            g = 0.0f;
            b = 0.0f; // Red
        } else {
            r = 0.3f;
            g = 0.3f;
            b = 0.3f; // Gray
        }

        for (int i = 0; i < exitList.size(); ++i) {
            int k = exitList[i];
            if (k == -1) {
                continue;
            }

            TRoom* pExit = mpMap->mpRoomDB->getRoom(k);
            if (!pExit) {
                continue;
            }

            bool areaExit = (pExit->getArea() != mAID);

            if (!areaExit) {
                // Normal connection within same area
                auto ex = static_cast<float>(pExit->x());
                auto ey = static_cast<float>(pExit->y());
                auto ez = static_cast<float>(pExit->z());

                // Add line from current room to exit room
                lineVertices << rx << ry << rz; // Start point
                lineVertices << ex << ey << ez; // End point

                // Add colors for both vertices
                lineColors << r << g << b << 1.0f; // Start color
                lineColors << r << g << b << 1.0f; // End color

            } else {
                // Area exit - draw directional stub
                float dx = rx, dy = ry, dz = rz;

                // Calculate direction offset based on exit type
                if (i == 0) { // North
                    dy += 1.0f;
                } else if (i == 1) { // Northeast
                    dx += 1.0f;
                    dy += 1.0f;
                } else if (i == 2) { // East
                    dx += 1.0f;
                } else if (i == 3) { // Southeast
                    dx += 1.0f;
                    dy -= 1.0f;
                } else if (i == 4) { // South
                    dy -= 1.0f;
                } else if (i == 5) { // Southwest
                    dx -= 1.0f;
                    dy -= 1.0f;
                } else if (i == 6) { // West
                    dx -= 1.0f;
                } else if (i == 7) { // Northwest
                    dx -= 1.0f;
                    dy += 1.0f;
                } else if (i == 8) { // Up
                    dz += 1.0f;
                } else if (i == 9) { // Down
                    dz -= 1.0f;
                }

                // Add line from current room to direction offset
                lineVertices << rx << ry << rz; // Start point
                lineVertices << dx << dy << dz; // End point (offset)

                // Use different color for area exits (greenish)
                lineColors << 85.0f / 255.0f << 170.0f / 255.0f << 0.0f << 1.0f; // Start color
                lineColors << 85.0f / 255.0f << 170.0f / 255.0f << 0.0f << 1.0f; // End color

                // Render green area exit cube at the destination position (with lighting)
                renderCube(dx, dy, dz, 1.0f / scale, 85.0f / 255.0f, 170.0f / 255.0f, 0.0f, 1.0f);

                // Render smaller environment overlay rectangle on top (like regular rooms)
                glDisable(GL_DEPTH_TEST);
                QColor envColor = getEnvironmentColor(pExit);
                float overlayZ = dz + 0.25f;
                renderCube(dx,
                           dy,
                           overlayZ,
                           0.5f / scale, // Much smaller overlay
                           envColor.redF(),
                           envColor.greenF(),
                           envColor.blueF(),
                           0.8f);
                glEnable(GL_DEPTH_TEST);
            }
        }
    }

    // Render all collected lines
    if (!lineVertices.isEmpty()) {
        renderLines(lineVertices, lineColors);
    }
}

void ModernGLWidget::renderCube(float x, float y, float z, float size, float r, float g, float b, float a)
{
    // Create cube vertices and normals matching original glwidget.cpp
    // Using the exact same vertex order and normals as the original
    const float s = size;

    QVector<float> vertices;
    QVector<float> normals;
    QVector<float> colors;

    // Bottom face (glNormal3f(0.57735, -0.57735, 0.57735), etc.)
    vertices << x + s << y - s << z + s;
    normals << 0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z + s;
    normals << -0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z - s;
    normals << -0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z + s;
    normals << 0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z - s;
    normals << -0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z - s;
    normals << 0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;

    // Front face
    vertices << x + s << y + s << z + s;
    normals << 0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z + s;
    normals << -0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z + s;
    normals << -0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z + s;
    normals << 0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z + s;
    normals << -0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z + s;
    normals << 0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;

    // Back face
    vertices << x - s << y + s << z - s;
    normals << -0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z - s;
    normals << 0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z - s;
    normals << 0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z - s;
    normals << -0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z - s;
    normals << 0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z - s;
    normals << -0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;

    // Right face
    vertices << x + s << y + s << z - s;
    normals << 0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z + s;
    normals << 0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z + s;
    normals << 0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z - s;
    normals << 0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z + s;
    normals << 0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y - s << z - s;
    normals << 0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;

    // Left face
    vertices << x - s << y + s << z + s;
    normals << -0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z - s;
    normals << -0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z - s;
    normals << -0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z + s;
    normals << -0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z - s;
    normals << -0.57735f << -0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y - s << z + s;
    normals << -0.57735f << -0.57735f << 0.57735f;
    colors << r << g << b << a;

    // Top face
    vertices << x + s << y + s << z - s;
    normals << 0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z - s;
    normals << -0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z + s;
    normals << -0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z - s;
    normals << 0.57735f << 0.57735f << -0.57735f;
    colors << r << g << b << a;
    vertices << x - s << y + s << z + s;
    normals << -0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;
    vertices << x + s << y + s << z + s;
    normals << 0.57735f << 0.57735f << 0.57735f;
    colors << r << g << b << a;

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVAO);

    // Upload vertex data
    mVertexBuffer.bind();
    mVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Upload color data
    mColorBuffer.bind();
    mColorBuffer.allocate(colors.data(), colors.size() * sizeof(float));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    // Upload normal data
    mNormalBuffer.bind();
    mNormalBuffer.allocate(normals.data(), normals.size() * sizeof(float));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(2);

    // Set uniforms
    QMatrix4x4 mvp = mProjectionMatrix * mViewMatrix * mModelMatrix;
    mShaderProgram->setUniformValue(mUniformMVP, mvp);
    mShaderProgram->setUniformValue(mUniformModel, mModelMatrix);

    // Normal matrix (inverse transpose of model matrix)
    QMatrix3x3 normalMatrix = mModelMatrix.normalMatrix();
    mShaderProgram->setUniformValue(mUniformNormalMatrix, normalMatrix);

    // Draw the cube
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
}

// Implement slot methods (same interface as original)
void ModernGLWidget::slot_showAllLevels()
{
    mShowTopLevels = 999999;
    mShowBottomLevels = 999999;
    update();
}

void ModernGLWidget::slot_shiftDown()
{
    mShiftMode = true;
    mMapCenterY--;
    update();
}

void ModernGLWidget::slot_shiftUp()
{
    mShiftMode = true;
    mMapCenterY++;
    update();
}

void ModernGLWidget::slot_shiftLeft()
{
    mShiftMode = true;
    mMapCenterX--;
    update();
}

void ModernGLWidget::slot_shiftRight()
{
    mShiftMode = true;
    mMapCenterX++;
    update();
}

void ModernGLWidget::slot_shiftZup()
{
    mShiftMode = true;
    mMapCenterZ++;
    update();
}

void ModernGLWidget::slot_shiftZdown()
{
    mShiftMode = true;
    mMapCenterZ--;
    update();
}

void ModernGLWidget::slot_singleLevelView()
{
    mShowTopLevels = 0;
    mShowBottomLevels = 0;
    update();
}

void ModernGLWidget::slot_showMoreUpperLevels()
{
    mShowTopLevels += 1;
    update();
}

void ModernGLWidget::slot_showLessUpperLevels()
{
    mShowTopLevels--;
    if (mShowTopLevels < 0) {
        mShowTopLevels = 0;
    }
    update();
}

void ModernGLWidget::slot_showMoreLowerLevels()
{
    mShowBottomLevels++;
    update();
}

void ModernGLWidget::slot_showLessLowerLevels()
{
    mShowBottomLevels--;
    if (mShowBottomLevels < 0) {
        mShowBottomLevels = 0;
    }
    update();
}

void ModernGLWidget::slot_defaultView()
{
    xRot = 1.0;
    yRot = 5.0;
    zRot = 10.0;
    mScale = 1.0;
    is2DView = false;
    update();
}

void ModernGLWidget::slot_sideView()
{
    xRot = 7.0;
    yRot = -10.0;
    zRot = 0.0;
    mScale = 1.0;
    is2DView = false;
    update();
}

void ModernGLWidget::slot_topView()
{
    xRot = 0.0;
    yRot = 0.0;
    zRot = 15.0;
    mScale = 1.0;
    is2DView = true;
    update();
}

void ModernGLWidget::slot_setScale(int angle)
{
    mScale = 150 / (static_cast<float>(angle) + 300.0f);
    update();
}

void ModernGLWidget::slot_setCameraPositionX(int angle)
{
    angle /= 10; // qNormalizeAngle equivalent
    xRot = angle;
    is2DView = false;
    update();
}

void ModernGLWidget::slot_setCameraPositionY(int angle)
{
    angle /= 10; // qNormalizeAngle equivalent
    yRot = angle;
    is2DView = false;
    update();
}

void ModernGLWidget::slot_setCameraPositionZ(int angle)
{
    angle /= 10; // qNormalizeAngle equivalent
    zRot = angle;
    is2DView = false;
    update();
}

void ModernGLWidget::setViewCenter(int areaId, int xPos, int yPos, int zPos)
{
    mAID = areaId;
    mShiftMode = true;
    mMapCenterX = xPos;
    mMapCenterY = yPos;
    mMapCenterZ = zPos;
    update();
}

void ModernGLWidget::wheelEvent(QWheelEvent* e)
{
    // Implement wheel event handling similar to original
    const int delta = e->angleDelta().y();
    if (delta > 0) {
        mScale *= 1.1f;
    } else {
        mScale *= 0.9f;
    }

    // Clamp scale to reasonable bounds to prevent zoom issues
    mScale = qBound(0.01f, mScale, 100.0f);

    update();
}

void ModernGLWidget::mousePressEvent(QMouseEvent* event)
{
    // Implement mouse handling (placeholder)
    QOpenGLWidget::mousePressEvent(event);
}

void ModernGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Implement mouse handling (placeholder)
    QOpenGLWidget::mouseMoveEvent(event);
}

void ModernGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    // Implement mouse handling (placeholder)
    QOpenGLWidget::mouseReleaseEvent(event);
}

void ModernGLWidget::renderLines(const QVector<float>& vertices, const QVector<float>& colors)
{
    if (vertices.isEmpty() || colors.isEmpty()) {
        return;
    }

    // Create dummy normals for lines (pointing up)
    QVector<float> normals;
    for (int i = 0; i < vertices.size() / 3; ++i) {
        normals << 0.0f << 0.0f << 1.0f; // Up vector for all line vertices
    }

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVAO);

    // Upload vertex data
    mVertexBuffer.bind();
    mVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Upload color data
    mColorBuffer.bind();
    mColorBuffer.allocate(colors.data(), colors.size() * sizeof(float));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    // Upload normal data
    mNormalBuffer.bind();
    mNormalBuffer.allocate(normals.data(), normals.size() * sizeof(float));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(2);

    // Set uniforms
    QMatrix4x4 mvp = mProjectionMatrix * mViewMatrix * mModelMatrix;
    mShaderProgram->setUniformValue(mUniformMVP, mvp);
    mShaderProgram->setUniformValue(mUniformModel, mModelMatrix);

    QMatrix3x3 normalMatrix = mModelMatrix.normalMatrix();
    mShaderProgram->setUniformValue(mUniformNormalMatrix, normalMatrix);

    // Draw the lines
    glDrawArrays(GL_LINES, 0, vertices.size() / 3);
}

void ModernGLWidget::renderTriangles(const QVector<float>& vertices, const QVector<float>& colors)
{
    if (vertices.isEmpty() || colors.isEmpty() || vertices.size() % 3 != 0 || colors.size() % 4 != 0) {
        qDebug() << "ModernGLWidget::renderTriangles: Invalid vertex or color array size";
        return;
    }

    // Check that we have the right ratio: 3 floats per vertex, 4 floats per color
    if (vertices.size() / 3 != colors.size() / 4) {
        qDebug() << "ModernGLWidget::renderTriangles: Vertex count doesn't match color count";
        return;
    }

    // Create dummy normals for triangles (pointing up)
    QVector<float> normals;
    for (int i = 0; i < vertices.size() / 3; ++i) {
        normals << 0.0f << 0.0f << 1.0f; // Up vector for all triangle vertices
    }

    QOpenGLVertexArrayObject::Binder vaoBinder(&mVAO);

    // Upload vertex data
    mVertexBuffer.bind();
    mVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Upload color data
    mColorBuffer.bind();
    mColorBuffer.allocate(colors.data(), colors.size() * sizeof(float));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    // Upload normal data
    mNormalBuffer.bind();
    mNormalBuffer.allocate(normals.data(), normals.size() * sizeof(float));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(2);

    // Set uniforms
    QMatrix4x4 mvp = mProjectionMatrix * mViewMatrix * mModelMatrix;
    mShaderProgram->setUniformValue(mUniformMVP, mvp);
    mShaderProgram->setUniformValue(mUniformModel, mModelMatrix);

    QMatrix3x3 normalMatrix = mModelMatrix.normalMatrix();
    mShaderProgram->setUniformValue(mUniformNormalMatrix, normalMatrix);

    // Draw the triangles
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
}

void ModernGLWidget::renderUpDownIndicators(TRoom* pRoom, float x, float y, float z)
{
    if (!pRoom) {
        return;
    }

    QVector<float> triangleVertices;
    QVector<float> triangleColors;

    // Gray color for indicators (same as original)
    float gray[] = {128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};

    // Triangle size (from original: ±0.95/scale for points, ±0.25/scale for base)
    float pointSize = 0.95f / scale;
    float baseSize = 0.25f / scale;

    // Down arrow (if room has down exit)
    if (pRoom->getDown() > -1) {
        // Triangle pointing down: top-left, top-right, bottom-center
        triangleVertices << (x - pointSize) << (y - baseSize) << z; // Top-left
        triangleVertices << (x + pointSize) << (y - baseSize) << z; // Top-right
        triangleVertices << x << (y - pointSize) << z;              // Bottom-center

        // Add gray color for all three vertices
        for (int i = 0; i < 3; ++i) {
            triangleColors << gray[0] << gray[1] << gray[2] << gray[3];
        }
    }

    // Up arrow (if room has up exit)
    if (pRoom->getUp() > -1) {
        // Triangle pointing up: bottom-left, bottom-right, top-center
        triangleVertices << (x - pointSize) << (y + baseSize) << z; // Bottom-left
        triangleVertices << (x + pointSize) << (y + baseSize) << z; // Bottom-right
        triangleVertices << x << (y + pointSize) << z;              // Top-center

        // Add gray color for all three vertices
        for (int i = 0; i < 3; ++i) {
            triangleColors << gray[0] << gray[1] << gray[2] << gray[3];
        }
    }

    // Render the triangles if we have any
    if (!triangleVertices.isEmpty()) {
        renderTriangles(triangleVertices, triangleColors);
    }
}

QColor ModernGLWidget::getPlaneColor(int zLevel, bool belowOrAtLevel)
{
    // Both color arrays from original glwidget.cpp
    static const float planeColor[][4] = {{0.5f, 0.6f, 0.5f, 0.2f},
                                          {0.233f, 0.498f, 0.113f, 0.2f},
                                          {0.666f, 0.333f, 0.498f, 0.2f},
                                          {0.5f, 0.333f, 0.666f, 0.2f},
                                          {0.69f, 0.458f, 0.0f, 0.2f},
                                          {0.333f, 0.0f, 0.49f, 0.2f},
                                          {133.0f / 255.0f, 65.0f / 255.0f, 98.0f / 255.0f, 0.2f},
                                          {0.3f, 0.3f, 0.0f, 0.2f},
                                          {0.6f, 0.2f, 0.6f, 0.2f},
                                          {0.6f, 0.6f, 0.2f, 0.2f},
                                          {0.4f, 0.1f, 0.4f, 0.2f},
                                          {0.4f, 0.4f, 0.1f, 0.2f},
                                          {0.3f, 0.1f, 0.3f, 0.2f},
                                          {0.3f, 0.3f, 0.1f, 0.2f},
                                          {0.2f, 0.1f, 0.2f, 0.2f},
                                          {0.2f, 0.2f, 0.1f, 0.2f},
                                          {0.24f, 0.1f, 0.5f, 0.2f},
                                          {0.1f, 0.1f, 0.0f, 0.2f},
                                          {0.54f, 0.6f, 0.2f, 0.2f},
                                          {0.2f, 0.2f, 0.5f, 0.2f},
                                          {0.6f, 0.6f, 0.2f, 0.2f},
                                          {0.6f, 0.4f, 0.6f, 0.2f},
                                          {0.4f, 0.4f, 0.1f, 0.2f},
                                          {0.4f, 0.2f, 0.4f, 0.2f},
                                          {0.2f, 0.2f, 0.0f, 0.2f},
                                          {0.2f, 0.1f, 0.3f, 0.2f}};

    static const float planeColor2[][4] = {{0.9f, 0.5f, 0.0f, 1.0f},
                                           {165.0f / 255.0f, 102.0f / 255.0f, 167.0f / 255.0f, 1.0f},
                                           {170.0f / 255.0f, 10.0f / 255.0f, 127.0f / 255.0f, 1.0f},
                                           {203.0f / 255.0f, 135.0f / 255.0f, 101.0f / 255.0f, 1.0f},
                                           {154.0f / 255.0f, 154.0f / 255.0f, 115.0f / 255.0f, 1.0f},
                                           {107.0f / 255.0f, 154.0f / 255.0f, 100.0f / 255.0f, 1.0f},
                                           {154.0f / 255.0f, 184.0f / 255.0f, 111.0f / 255.0f, 1.0f},
                                           {67.0f / 255.0f, 154.0f / 255.0f, 148.0f / 255.0f, 1.0f},
                                           {154.0f / 255.0f, 118.0f / 255.0f, 151.0f / 255.0f, 1.0f},
                                           {208.0f / 255.0f, 213.0f / 255.0f, 164.0f / 255.0f, 1.0f},
                                           {213.0f / 255.0f, 169.0f / 255.0f, 158.0f / 255.0f, 1.0f},
                                           {139.0f / 255.0f, 209.0f / 255.0f, 0.0f, 1.0f},
                                           {163.0f / 255.0f, 209.0f / 255.0f, 202.0f / 255.0f, 1.0f},
                                           {158.0f / 255.0f, 156.0f / 255.0f, 209.0f / 255.0f, 1.0f},
                                           {209.0f / 255.0f, 144.0f / 255.0f, 162.0f / 255.0f, 1.0f},
                                           {209.0f / 255.0f, 183.0f / 255.0f, 78.0f / 255.0f, 1.0f},
                                           {111.0f / 255.0f, 209.0f / 255.0f, 88.0f / 255.0f, 1.0f},
                                           {95.0f / 255.0f, 120.0f / 255.0f, 209.0f / 255.0f, 1.0f},
                                           {31.0f / 255.0f, 209.0f / 255.0f, 126.0f / 255.0f, 1.0f},
                                           {1.0f, 170.0f / 255.0f, 1.0f, 1.0f},
                                           {158.0f / 255.0f, 105.0f / 255.0f, 158.0f / 255.0f, 1.0f},
                                           {68.0f / 255.0f, 189.0f / 255.0f, 189.0f / 255.0f, 1.0f},
                                           {0.1f, 0.69f, 0.49f, 1.0f},
                                           {0.0f, 0.15f, 1.0f, 1.0f},
                                           {0.12f, 0.02f, 0.20f, 1.0f},
                                           {0.0f, 0.3f, 0.1f, 1.0f}};

    int ef = abs(zLevel % 26);
    const float* color;

    // Original logic from line 1382 and 1389:
    // rz <= pz: glColor4f(planeColor[ef]) - use planeColor for rooms below/at level
    // rz > pz:  glColor4f(planeColor2[ef]) - use planeColor2 for rooms above level
    // Try using the brighter array as default since most rooms are likely at the same level
    if (belowOrAtLevel) {
        color = planeColor2[ef]; // Use bright colors for rooms at/below level
        // qDebug() << "Using planeColor2[" << ef << "] for room at/below level";
    } else {
        color = planeColor[ef]; // Use darker colors for rooms above level
        // qDebug() << "Using planeColor[" << ef << "] for room above level";
    }

    return QColor(static_cast<int>(color[0] * 255),
                  static_cast<int>(color[1] * 255),
                  static_cast<int>(color[2] * 255),
                  255); // Use full alpha for room colors
}

QColor ModernGLWidget::getEnvironmentColor(TRoom* pRoom)
{
    if (!pRoom || !mpMap || !mpHost) {
        return QColor(128, 128, 128); // Default gray
    }

    QColor roomColor;
    int roomEnvironment = pRoom->environment;

    // Same logic as T2DMap.cpp
    if (mpMap->mEnvColors.contains(roomEnvironment)) {
        roomEnvironment = mpMap->mEnvColors[roomEnvironment];
    } else {
        if (!mpMap->mCustomEnvColors.contains(roomEnvironment)) {
            roomEnvironment = 1;
        }
    }

    switch (roomEnvironment) {
    case 1:
        roomColor = mpHost->mRed_2;
        break;
    case 2:
        roomColor = mpHost->mGreen_2;
        break;
    case 3:
        roomColor = mpHost->mYellow_2;
        break;
    case 4:
        roomColor = mpHost->mBlue_2;
        break;
    case 5:
        roomColor = mpHost->mMagenta_2;
        break;
    case 6:
        roomColor = mpHost->mCyan_2;
        break;
    case 7:
        roomColor = mpHost->mWhite_2;
        break;
    case 8:
        roomColor = mpHost->mBlack_2;
        break;
    case 9:
        roomColor = mpHost->mLightRed_2;
        break;
    case 10:
        roomColor = mpHost->mLightGreen_2;
        break;
    case 11:
        roomColor = mpHost->mLightYellow_2;
        break;
    case 12:
        roomColor = mpHost->mLightBlue_2;
        break;
    case 13:
        roomColor = mpHost->mLightMagenta_2;
        break;
    case 14:
        roomColor = mpHost->mLightCyan_2;
        break;
    case 15:
        roomColor = mpHost->mLightWhite_2;
        break;
    case 16:
        roomColor = mpHost->mLightBlack_2;
        break;
    default: // user defined room color
        if (mpMap->mCustomEnvColors.contains(roomEnvironment)) {
            roomColor = mpMap->mCustomEnvColors[roomEnvironment];
        } else {
            if (16 < roomEnvironment && roomEnvironment < 232) {
                quint8 const base = roomEnvironment - 16;
                quint8 r = base / 36;
                quint8 g = (base - (r * 36)) / 6;
                quint8 b = (base - (r * 36)) - (g * 6);

                r = r == 0 ? 0 : (r - 1) * 40 + 95;
                g = g == 0 ? 0 : (g - 1) * 40 + 95;
                b = b == 0 ? 0 : (b - 1) * 40 + 95;
                roomColor = QColor(r, g, b, 255);
            } else if (231 < roomEnvironment && roomEnvironment < 256) {
                quint8 const k = ((roomEnvironment - 232) * 10) + 8;
                roomColor = QColor(k, k, k, 255);
            } else {
                roomColor = mpHost->mRed_2; // fallback
            }
        }
    }

    return roomColor;
}
