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
} GridType;

typedef struct _GridProps
{
    int gravityRight;
    int gravityDown;
    int numCellsX;
    int numCellsY;
} GridProps;

class GridScreen :
    public PluginClassHandler <GridScreen, CompScreen>,
    public GridOptions
{
    public:

	GridScreen (CompScreen *);
	bool
	initiateCommon (CompAction         *action,
			CompAction::State  state,
			CompOption::Vector &option,
			GridType           where);

    private:

	CompRect
	slotToRect (CompWindow      *w,
		    const CompRect& slot);
	CompRect
	constrainSize (CompWindow *w,
		       const CompRect& slot);
};

class GridPluginVTable :
    public CompPlugin::VTableForScreen <GridScreen>
{
    public:

	bool init ();

	PLUGIN_OPTION_HELPER (GridScreen);
};

COMPIZ_PLUGIN_20081216 (grid, GridPluginVTable);

