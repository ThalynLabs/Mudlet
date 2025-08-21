#ifndef MUDLET_GLWIDGET_INTEGRATION_H
#define MUDLET_GLWIDGET_INTEGRATION_H

/***************************************************************************
 *   Copyright (C) 2024 by Mudlet Development Team                        *
 *                                                                         *
 *   Integration header for modernized GLWidget                           *
 *   This header allows for a gradual transition to the modern OpenGL     *
 *   implementation while maintaining compatibility with existing code.    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

// To enable the modern GLWidget implementation, define this macro:
// #define USE_MODERN_GLWIDGET

#ifdef USE_MODERN_GLWIDGET
    #include "modern_glwidget.h"
    using GLWidget = ModernGLWidget;
#else
    #include "glwidget.h"
    // Keep using the original GLWidget
#endif

#endif // MUDLET_GLWIDGET_INTEGRATION_H