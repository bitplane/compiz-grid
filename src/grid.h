/*
 * Compiz Fusion Grid plugin
 *
 * Copyright (c) 2008 Stephen Kennedy <suasol@gmail.com>
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

#include "grid_options.h"

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
    public PluginClassHandler <GridScreen, CompScreen>,
    public GridOptions
{
    public:

	void handleEvent (XEvent *event);

	GridScreen (CompScreen *);

	bool
	initiateCommon (CompAction*, CompAction::State,	CompOption::Vector&, GridType);

	Edges edge;

	GridType
	dropLocation ();

    private:

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

	GridScreen      *gScreen;

	bool grabIsMove;

	void
	grabNotify (int, int, unsigned int, unsigned int);

	void
	ungrabNotify ();

	void
	sendMaximizationRequest ();
};

#define GRID_SCREEN(s) \
    GridScreen *gs = GridScreen::get (s)

#define GRID_WINDOW(w) \
    GridWindow *gw = GridWindow::get (w)

class GridPluginVTable :
    public CompPlugin::VTableForScreenAndWindow <GridScreen, GridWindow>
{
    public:

	bool init ();
};

COMPIZ_PLUGIN_20090315 (grid, GridPluginVTable);

