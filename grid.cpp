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

#include "grid.h"

static const GridProps gridProps[] =
{
    {0,1, 1,1},

    {0,1, 2,2},
    {0,1, 1,2},
    {1,1, 2,2},

    {0,0, 2,1},
    {0,0, 1,1},
    {1,0, 2,1},

    {0,0, 2,2},
    {0,0, 1,2},
    {1,0, 2,2},
};

void
GridScreen::slotToRect (CompWindow *w,
	    XRectangle *slot,
	    XRectangle *rect)
{
    rect->x = slot->x + w->input ().left;
    rect->y = slot->y + w->input ().top;
    rect->width = slot->width - (w->input ().left + w->input ().right);
    rect->height = slot->height - (w->input ().top + w->input ().bottom);
}

void
GridScreen::constrainSize (CompWindow *w,
			   XRectangle *slot,
			   XRectangle *rect)
{
    XRectangle workarea;
    XRectangle r;
    int        cw, ch;

    screen->getWorkareaForOutput (w->outputDevice (), &workarea);
    slotToRect (w, slot, &r);

    if (w->constrainNewWindowSize (r.width, r.height, &cw, &ch))
    {
	/* constrained size may put window offscreen, adjust for that case */
	int dx = r.x + cw - workarea.width - workarea.x + w->input ().right;
	int dy = r.y + ch - workarea.height - workarea.y + w->input ().bottom;

	if ( dx > 0 )
	    r.x -= dx;
	if ( dy > 0 )
	    r.y -= dy;

	r.width = cw;
	r.height = ch;
    }

    *rect = r;
}

bool
GridScreen::initiateCommon (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector &option,
			    GridType           where)
{
    Window     xid;
    CompWindow *cw = 0;

    xid = CompOption::getIntOptionNamed (option, "window");
    cw  = screen->findWindow (xid);
    if (cw)
    {
	XRectangle     workarea;
	XRectangle     desiredSlot;
	XRectangle     desiredRect;
	XRectangle     currentRect;
	GridProps      props = gridProps[where];
	XWindowChanges xwc;

	/* get current available area */
	screen->getWorkareaForOutput (cw->outputDevice (), &workarea);

	/* Convention:
	 * xxxSlot include decorations (it's the screen area occupied)
	 * xxxRect are undecorated (it's the constrained position
	                            of the contents)
	 */

	/* slice and dice to get desired slot - including decorations */
	desiredSlot.y =  workarea.y + props.gravityDown *
	                 (workarea.height / props.numCellsY);
	desiredSlot.height = workarea.height / props.numCellsY;
	desiredSlot.x =  workarea.x + props.gravityRight *
	                 (workarea.width / props.numCellsX);
	desiredSlot.width = workarea.width / props.numCellsX;

	/* Adjust for constraints and decorations */
	constrainSize (cw, &desiredSlot, &desiredRect);

	/* Get current rect not including decorations */
	currentRect.x = cw->serverX ();
	currentRect.y = cw->serverY ();
	currentRect.width  = cw->serverWidth ();
	currentRect.height = cw->serverHeight ();

	if (desiredRect.y      == currentRect.y &&
	    desiredRect.height == currentRect.height)
	{
	    int slotWidth33  = workarea.width / 3;
	    int slotWidth66  = workarea.width - slotWidth33;

	    if (props.numCellsX == 2) /* keys (1, 4, 7, 3, 6, 9) */
	    {
		if (currentRect.width == desiredRect.width &&
		    currentRect.x == desiredRect.x)
		{
		    desiredSlot.width = slotWidth66;
		    desiredSlot.x = workarea.x +
			            props.gravityRight * slotWidth33;
		}
		else
		{
		    /* tricky, have to allow for window constraints when
		     * computing what the 33% and 66% offsets would be
		     */
		    XRectangle rect33, rect66, slot33, slot66;

		    slot33 = desiredSlot;
		    slot33.x = workarea.x + props.gravityRight * slotWidth66;
		    slot33.width = slotWidth33;
		    constrainSize (cw, &slot33, &rect33);

		    slot66 = desiredSlot;
		    slot66.x = workarea.x + props.gravityRight * slotWidth33;
		    slot66.width = slotWidth66;
		    constrainSize (cw, &slot66, &rect66);

		    if (currentRect.width == rect66.width &&
			currentRect.x == rect66.x)
		    {
			desiredSlot.width = slotWidth33;
			desiredSlot.x = workarea.x +
			                props.gravityRight * slotWidth66;
		    }
		}
	    }
	    else /* keys (2, 5, 8) */
	    {
		if (currentRect.width == desiredRect.width &&
		    currentRect.x == desiredRect.x)
		{
		    desiredSlot.width = slotWidth33;
		    desiredSlot.x = workarea.x + slotWidth33;
		}
	    }
	    constrainSize (cw, &desiredSlot, &desiredRect);
	}

	xwc.x = desiredRect.x;
	xwc.y = desiredRect.y;
	xwc.width  = desiredRect.width;
	xwc.height = desiredRect.height;

	if (cw->mapNum ())
	    cw->sendSyncRequest ();

	if (cw->state () & MAXIMIZE_STATE)
	{
	    /* maximized state interferes with us, clear it */
	    cw->maximize (0);
	}

	/* TODO: animate move+resize */
	cw->configureXWindow (CWX | CWY | CWWidth | CWHeight, &xwc);
    }

    return true;
}

#define GRIDSET(opt,where)			      \
    optionSet##opt##Initiate (boost::bind (&GridScreen::initiateCommon,this, _1, _2, _3, where))

GridScreen::GridScreen (CompScreen *screen) :
    PrivateHandler<GridScreen, CompScreen> (screen),
    GridOptions (gridVTable->getMetadata ())
{
    GRIDSET ( PutCenterKey, GridCenter);
    GRIDSET ( PutLeftKey, GridLeft);
    GRIDSET ( PutRightKey, GridRight);
    GRIDSET ( PutTopKey, GridTop);
    GRIDSET ( PutBottomKey, GridBottom);
    GRIDSET ( PutTopleftKey, GridTopLeft);
    GRIDSET ( PutToprightKey, GridTopRight);
    GRIDSET ( PutBottomleftKey, GridBottomLeft);
    GRIDSET ( PutBottomrightKey, GridBottomRight);

}
#undef GRIDSET
/* Initial plugin init function called. Checks to see if we are ABI
 * compatible with core, otherwise unload */

bool
GridPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
        return false;

    return true;
}





