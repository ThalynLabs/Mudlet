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
#include "TRoom.h"
#include "TRoomDB.h"
#include "dlgMapper.h"
#include "Host.h"

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
    // Set up projection matrix with fixed FOV
    mProjectionMatrix.setToIdentity();
    const float aspectRatio = static_cast<float>(width()) / static_cast<float>(height());
    // Keep FOV constant at 60 degrees, adjust camera distance with scale instead
    mProjectionMatrix.perspective(60.0f, aspectRatio, 0.0001f, 10000.0f);
    
    // Set up view matrix (camera)
    mViewMatrix.setToIdentity();
    
    // Use scale to control camera distance (inverse relationship for intuitive zoom)
    const float cameraDistance = 30.0f / mScale;
    
    // Translate camera away from the map center
    mViewMatrix.translate(0.0f, 0.0f, -cameraDistance);
    
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

    // Render the map
    renderRooms();
    
    // Render connections between rooms
    renderConnections();
    
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
    // qDebug() << "ModernGLWidget: Rendering area" << mAID << "with" << pArea->getAreaRooms().size() << "rooms";

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

        // Get room color using the same system as 2D map
        QColor roomColor = getRoomColor(pR);
        
        // Check if this is the current room
        bool isCurrentRoom = (rz == pz) && (rx == static_cast<float>(mMapCenterX)) && (ry == static_cast<float>(mMapCenterY));
        
        if (isCurrentRoom) {
            // Render current room in red
            renderCube(rx, ry, rz, 0.8f / scale, 1.0f, 0.0f, 0.0f, 1.0f);
        } else {
            // Render normal room with proper environment color
            renderCube(rx, ry, rz, 0.8f / scale, 
                      roomColor.redF(), 
                      roomColor.greenF(), 
                      roomColor.blueF(), 
                      roomColor.alphaF());
        }
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
            r = 1.0f; g = 0.0f; b = 0.0f; // Red
        } else {
            r = 0.3f; g = 0.3f; b = 0.3f; // Gray
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
                lineVertices << rx << ry << rz;  // Start point
                lineVertices << ex << ey << ez;  // End point
                
                // Add colors for both vertices
                lineColors << r << g << b << 1.0f;  // Start color
                lineColors << r << g << b << 1.0f;  // End color
                
            } else {
                // Area exit - draw directional stub
                float dx = rx, dy = ry, dz = rz;
                
                // Calculate direction offset based on exit type
                if (i == 0) { // North
                    dy += 1.0f;
                } else if (i == 1) { // Northeast
                    dx += 1.0f; dy += 1.0f;
                } else if (i == 2) { // East
                    dx += 1.0f;
                } else if (i == 3) { // Southeast
                    dx += 1.0f; dy -= 1.0f;
                } else if (i == 4) { // South
                    dy -= 1.0f;
                } else if (i == 5) { // Southwest
                    dx -= 1.0f; dy -= 1.0f;
                } else if (i == 6) { // West
                    dx -= 1.0f;
                } else if (i == 7) { // Northwest
                    dx -= 1.0f; dy += 1.0f;
                } else if (i == 8) { // Up
                    dz += 1.0f;
                } else if (i == 9) { // Down
                    dz -= 1.0f;
                }
                
                // Add line from current room to direction offset
                lineVertices << rx << ry << rz;  // Start point
                lineVertices << dx << dy << dz;  // End point (offset)
                
                // Use different color for area exits (greenish)
                lineColors << 85.0f/255.0f << 170.0f/255.0f << 0.0f << 1.0f;  // Start color
                lineColors << 85.0f/255.0f << 170.0f/255.0f << 0.0f << 1.0f;  // End color
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

    // Draw the lines
    glDrawArrays(GL_LINES, 0, vertices.size() / 3);
}

QColor ModernGLWidget::getRoomColor(TRoom* pRoom)
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
    case 1:     roomColor = mpHost->mRed_2;             break;
    case 2:     roomColor = mpHost->mGreen_2;           break;
    case 3:     roomColor = mpHost->mYellow_2;          break;
    case 4:     roomColor = mpHost->mBlue_2;            break;
    case 5:     roomColor = mpHost->mMagenta_2;         break;
    case 6:     roomColor = mpHost->mCyan_2;            break;
    case 7:     roomColor = mpHost->mWhite_2;           break;
    case 8:     roomColor = mpHost->mBlack_2;           break;
    case 9:     roomColor = mpHost->mLightRed_2;        break;
    case 10:    roomColor = mpHost->mLightGreen_2;      break;
    case 11:    roomColor = mpHost->mLightYellow_2;     break;
    case 12:    roomColor = mpHost->mLightBlue_2;       break;
    case 13:    roomColor = mpHost->mLightMagenta_2;    break;
    case 14:    roomColor = mpHost->mLightCyan_2;       break;
    case 15:    roomColor = mpHost->mLightWhite_2;      break;
    case 16:    roomColor = mpHost->mLightBlack_2;      break;
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