#ifndef MUDLET_GLWIDGET_INTEGRATION_H
#define MUDLET_GLWIDGET_INTEGRATION_H

/***************************************************************************
 *   Copyright (C) 2025 by Vadim Peretokin - vadim.peretokin@mudlet.org    *
 *                                                                         *
 *   Integration header for modernized GLWidget                            *
 *   This header allows for runtime selection between GLWidget             *
 *   implementations while maintaining compatibility with existing code.   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "glwidget.h"
#include "modern_glwidget.h"

class TMap;
class Host;
class QWidget;

namespace GLWidgetFactory {
    QOpenGLWidget* createGLWidget(TMap* pMap, Host* pHost, QWidget* parent = nullptr);
    bool isCorrectWidgetType(QOpenGLWidget* widget, Host* pHost);
    QString getWidgetTypeName(QOpenGLWidget* widget);
}

// Factory functions provide runtime widget creation
// Legacy GLWidget class name remains available for existing code

#endif // MUDLET_GLWIDGET_INTEGRATION_H
