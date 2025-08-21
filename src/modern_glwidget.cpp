/***************************************************************************
 *   Copyright (C) 2008-2013 by Heiko Koehn - KoehnHeiko@googlemail.com    *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *   Copyright (C) 2014, 2016, 2019-2021, 2023 by Stephen Lyons            *
 *                                               - slysven@virginmedia.com *
 *   Copyright (C) 2024 by Mudlet Development Team                        *
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

#include "mudlet.h"
#include "TArea.h"
#include "TRoomDB.h"
#include "dlgMapper.h"

#include "pre_guard.h"
#include <QtEvents>
#include <QPainter>
#include <QDebug>
#include "post_guard.h"

// Modern OpenGL vertex shader
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uMVP;
out vec4 vertexColor;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vertexColor = aColor;
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

ModernGLWidget::ModernGLWidget(TMap* pMap, Host* pHost, QWidget *parent)
: QOpenGLWidget(parent)
, mVertexBuffer(QOpenGLBuffer::VertexBuffer)
, mColorBuffer(QOpenGLBuffer::VertexBuffer)
, mpMap(pMap)
, mpHost(pHost)
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
    if (mUniformMVP == -1) {
        qWarning() << "Failed to get MVP uniform location";
    }
    
    qDebug() << "ModernGLWidget: Shaders initialized successfully. MVP uniform location:" << mUniformMVP;
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
    
    // Configure vertex attribute pointers (will be set during rendering)
}

void ModernGLWidget::updateMatrices()
{
    // Set up projection matrix
    mProjectionMatrix.setToIdentity();
    const float aspectRatio = static_cast<float>(width()) / static_cast<float>(height());
    mProjectionMatrix.perspective(60.0f * mScale, aspectRatio, 0.0001f, 10000.0f);
    
    // Set up view matrix (camera)
    mViewMatrix.setToIdentity();
    
    // Translate camera away from the map center
    mViewMatrix.translate(0.0f, 0.0f, -30.0f);
    
    // Apply rotations
    mViewMatrix.rotate(xRot, 1, 0, 0);
    mViewMatrix.rotate(yRot, 0, 1, 0);
    mViewMatrix.rotate(zRot, 0, 0, 1);
    
    // Translate to center on the current map position
    mViewMatrix.translate(-static_cast<float>(mMapCenterX), 
                          -static_cast<float>(mMapCenterY), 
                          -static_cast<float>(mMapCenterZ));
    
    // Model matrix will be set per object during rendering
    mModelMatrix.setToIdentity();
    
    qDebug() << "ModernGLWidget: View matrix updated. Camera center:" << mMapCenterX << mMapCenterY << mMapCenterZ 
             << "Rotation:" << xRot << yRot << zRot << "Scale:" << mScale;
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
            painter.drawText(0, 0, (width() -1), (height() -1), Qt::AlignCenter | Qt::TextWordWrap, message);
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

    // Test rendering - draw a simple cube at origin
    renderCube(0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 0.0f, 1.0f, 1.0f); // Large magenta cube at origin
    
    // Render the map
    renderRooms();
    
    mShaderProgram->release();
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
    qDebug() << "ModernGLWidget: Rendering area" << mAID << "with" << pArea->getAreaRooms().size() << "rooms";
    
    // Define room colors (simplified from original)
    constexpr float roomColors[][4] = {
        {0.9f, 0.5f, 0.0f, 1.0f},
        {0.165f, 0.102f, 0.167f, 1.0f},
        {0.170f, 0.010f, 0.127f, 1.0f},
        {0.203f, 0.135f, 0.101f, 1.0f},
        {0.154f, 0.154f, 0.115f, 1.0f},
        {0.107f, 0.154f, 0.100f, 1.0f}
    };
    
    constexpr int numColors = sizeof(roomColors) / sizeof(roomColors[0]);

    QSetIterator<int> itRoom(pArea->getAreaRooms());
    while (itRoom.hasNext()) {
        TRoom* pR = mpMap->mpRoomDB->getRoom(itRoom.next());
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

        // Choose color based on room position
        const int colorIndex = abs(static_cast<int>(rz)) % numColors;
        
        // Check if this is the current room
        bool isCurrentRoom = (rz == pz) && (rx == static_cast<float>(mMapCenterX)) && (ry == static_cast<float>(mMapCenterY));
        
        if (isCurrentRoom) {
            // Render current room in red
            qDebug() << "ModernGLWidget: Rendering current room at" << rx << ry << rz << "size" << (0.8f / scale);
            renderCube(rx, ry, rz, 0.8f / scale, 1.0f, 0.0f, 0.0f, 1.0f);
        } else {
            // Render normal room with level-based color
            renderCube(rx, ry, rz, 0.8f / scale, 
                      roomColors[colorIndex][0], 
                      roomColors[colorIndex][1], 
                      roomColors[colorIndex][2], 
                      roomColors[colorIndex][3]);
        }
    }
}

void ModernGLWidget::renderCube(float x, float y, float z, float size, float r, float g, float b, float a)
{
    // Cube vertices (simplified - just the 8 corners)
    const float s = size;
    QVector<float> vertices = {
        // Front face
        x-s, y-s, z+s,  x+s, y-s, z+s,  x+s, y+s, z+s,
        x-s, y-s, z+s,  x+s, y+s, z+s,  x-s, y+s, z+s,
        // Back face  
        x-s, y-s, z-s,  x-s, y+s, z-s,  x+s, y+s, z-s,
        x-s, y-s, z-s,  x+s, y+s, z-s,  x+s, y-s, z-s,
        // Left face
        x-s, y-s, z-s,  x-s, y-s, z+s,  x-s, y+s, z+s,
        x-s, y-s, z-s,  x-s, y+s, z+s,  x-s, y+s, z-s,
        // Right face
        x+s, y-s, z-s,  x+s, y+s, z-s,  x+s, y+s, z+s,
        x+s, y-s, z-s,  x+s, y+s, z+s,  x+s, y-s, z+s,
        // Top face
        x-s, y+s, z-s,  x-s, y+s, z+s,  x+s, y+s, z+s,
        x-s, y+s, z-s,  x+s, y+s, z+s,  x+s, y+s, z-s,
        // Bottom face
        x-s, y-s, z-s,  x+s, y-s, z-s,  x+s, y-s, z+s,
        x-s, y-s, z-s,  x+s, y-s, z+s,  x-s, y-s, z+s
    };

    // Create color data for all vertices
    QVector<float> colors;
    for (int i = 0; i < vertices.size() / 3; ++i) {
        colors << r << g << b << a;
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

    // Set MVP matrix uniform
    QMatrix4x4 mvp = mProjectionMatrix * mViewMatrix * mModelMatrix;
    mShaderProgram->setUniformValue(mUniformMVP, mvp);

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