/*
 * Compiz Fusion Grid plugin
 *
 * Copyright (c) 2008 Stephen Kennedy <suasol@gmail.com>
 * Copyright (c) 2010 Scott Moreau <oreaus@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Description:
 *
 * Plugin to act like winsplit revolution (http://www.winsplit-revolution.com/)
 * use <Control><Alt>NUMPAD_KEY to move and tile your windows.
 *
 * Press the tiling keys several times to cycle through some tiling options.
 */

#include <core/core.h>
#include <core/atoms.h>
#include <core/pluginclasshandler.h>
#include <composite/composite.h>
#include <opengl/opengl.h>

#include "grid_options.h"

#define SNAPOFF_THRESHOLD 50

typedef enum
{
    GridUnknown = 0,
    GridBottomLeft = 1,
    GridBottom = 2,
    GridBottomRight = 3,
    GridLeft = 4,
    GridCenter = 5,
    GridRight = 6,
    GridTopLeft = 7,
    GridTop = 8,
    GridTopRight = 9,
    GridMaximize = 10
} GridType;

typedef struct _GridProps
{
    int gravityRight;
    int gravityDown;
    int numCellsX;
    int numCellsY;
} GridProps;

enum Edges
{
    NoEdge = 0,
    BottomLeft,
    Bottom,
    BottomRight,
    Left,
    Right,
    TopLeft,
    Top,
    TopRight
};

class GridScreen :
    public ScreenInterface,
    public CompositeScreenInterface,
    public GLScreenInterface,
    public PluginClassHandler <GridScreen, CompScreen>,
    public GridOptions
{
    public:

	GridScreen (CompScreen *);
	CompositeScreen *cScreen;
	GLScreen        *glScreen;

	CompRect workarea, currentRect, desiredSlot,
		 desiredRect, lastWorkarea, currentWorkarea;
	GridProps props;
	Edges edge, lastEdge;
	CompOption::Vector o;
	bool centerCheck;
	CompWindow *mGrabWindow;

	void getPaintRectangle (CompRect&);

	bool initiateCommon (CompAction*, CompAction::State,
			     CompOption::Vector&, GridType, bool);

	void glPaintRectangle (const GLScreenPaintAttrib&,
			       const GLMatrix&, CompOutput *);

	bool glPaintOutput (const GLScreenPaintAttrib &,
			    const GLMatrix &, const CompRegion &,
			    CompOutput *, unsigned int);

	GridType edgeToGridType ();

	void handleEvent (XEvent *event);

	bool restoreWindow (CompAction*,
			    CompAction::State,
			    CompOption::Vector&);

	void
	snapbackOptionChanged (CompOption *option,
				Options    num);

	CompRect
	slotToRect (CompWindow      *w,
		    const CompRect& slot);
	CompRect
	constrainSize (CompWindow *w,
		       const CompRect& slot);
};

class GridWindow :
    public WindowInterface,
    public PluginClassHandler <GridWindow, CompWindow>
{
    public:

	GridWindow (CompWindow *);
	CompWindow *window;
	GridScreen *gScreen;

	bool isGridResized;
	bool isGridMaximized;
	int pointerBufDx;
	int pointerBufDy;
	int resizeCount;
	CompRect originalSize;
	GridType lastTarget;

	void grabNotify (int, int, unsigned int, unsigned int);

	void ungrabNotify ();

	void moveNotify (int, int, bool);
};

#define GRID_WINDOW(w) \
    GridWindow *gw = GridWindow::get (w)

class GridPluginVTable :
    public CompPlugin::VTableForScreenAndWindow <GridScreen, GridWindow>
{
    public:

	bool init ();
};

COMPIZ_PLUGIN_20090315 (grid, GridPluginVTable);

