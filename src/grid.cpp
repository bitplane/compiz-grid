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

    {0,0, 1,1},
};

CompRect
GridScreen::slotToRect (CompWindow      *w,
			const CompRect& slot)
{
    return CompRect (slot.x () + w->input ().left,
		     slot.y () + w->input ().top,
		     slot.width () - (w->input ().left + w->input ().right),
		     slot.height () - (w->input ().top + w->input ().bottom));
}

CompRect
GridScreen::constrainSize (CompWindow      *w,
			   const CompRect& slot)
{
    CompRect result;
    int      cw, ch;

    result   = slotToRect (w, slot);

    if (w->constrainNewWindowSize (result.width (), result.height (), &cw, &ch))
    {
	/* constrained size may put window offscreen, adjust for that case */
	int dx = result.x () + cw - workarea.right () + w->input ().right;
	int dy = result.y () + ch - workarea.bottom () + w->input ().bottom;

	if (dx > 0)
	    result.setX (result.x () - dx);
	if (dy > 0)
	    result.setY (result.y () - dy);

	result.setWidth (cw);
	result.setHeight (ch);
    }

    return result;
}

void
GridScreen::getPaintRectangle (CompRect &cRect)
{
    if (edge != NoEdge && optionGetDrawIndicator ())
	cRect = desiredSlot;
    else
	cRect.setGeometry (0, 0, 0, 0);
}

bool
GridScreen::initiateCommon (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector &option,
			    GridType           where,
			    bool               resize)
{
    Window     xid;
    CompWindow *cw = 0;

    xid = CompOption::getIntOptionNamed (option, "window");
    cw  = screen->findWindow (xid);

    if (where == GridUnknown || screen->otherGrabExist ("move", NULL))
	return false;

    if (cw)
    {
	XWindowChanges xwc;

	/* get current available area */
	if (GridWindow::get (cw)->grabIsMove)
	    workarea = screen->getWorkareaForOutput (screen->outputDeviceForPoint (pointerX, pointerY));
	else
	    workarea = screen->getWorkareaForOutput (cw->outputDevice ());

	if ((cw->state () & MAXIMIZE_STATE) && (resize || optionGetSnapoffMaximized ()))
	{
	    /* maximized state interferes with us, clear it */
	    cw->maximize (0);
	}

	if (where == GridMaximize && resize)
	{
	    /* move the window to the correct output */
	    if (GridWindow::get (cw)->grabIsMove)
	    {
		xwc.x = workarea.x ();
		xwc.y = workarea.y ();
		xwc.width = workarea.width ();
		xwc.height = workarea.height ();
		cw->configureXWindow (CWX | CWY, &xwc);
	    }
	    cw->maximize (MAXIMIZE_STATE);
	    return true;
	}

	props = gridProps[where];

	/* Convention:
	 * xxxSlot include decorations (it's the screen area occupied)
	 * xxxRect are undecorated (it's the constrained position
	                            of the contents)
	 */

	/* slice and dice to get desired slot - including decorations */
	desiredSlot.setY (workarea.y () + props.gravityDown *
			  (workarea.height () / props.numCellsY));
	desiredSlot.setHeight (workarea.height () / props.numCellsY);
	desiredSlot.setX (workarea.x () + props.gravityRight *
			  (workarea.width () / props.numCellsX));
	desiredSlot.setWidth (workarea.width () / props.numCellsX);

	/* Adjust for constraints and decorations */
	desiredRect = constrainSize (cw, desiredSlot);
	/* Get current rect not including decorations */
	currentRect.setGeometry (cw->serverX (), cw->serverY (),
				 cw->serverWidth (),
				 cw->serverHeight ());

	if (desiredRect == currentRect)
	{
	    int slotWidth33  = workarea.width () / 3;
	    int slotWidth66  = workarea.width () - slotWidth33;

	    if (props.numCellsX == 2) /* keys (1, 4, 7, 3, 6, 9) */
	    {
		if (currentRect.width () == desiredRect.width () &&
		    currentRect.x () == desiredRect.x ())
		{
		    desiredSlot.setWidth (slotWidth66);
		    desiredSlot.setX (workarea.x () +
				      props.gravityRight * slotWidth33);
		}
		else
		{
		    /* tricky, have to allow for window constraints when
		     * computing what the 33% and 66% offsets would be
		     */
		    CompRect rect33, rect66, slot33, slot66;

		    slot33 = desiredSlot;
		    slot33.setX (workarea.x () +
				 props.gravityRight * slotWidth66);
		    slot33.setWidth (slotWidth33);
		    rect33 = constrainSize (cw, slot33);

		    slot66 = desiredSlot;
		    slot66.setX (workarea.x () +
				 props.gravityRight * slotWidth33);
		    slot66.setWidth (slotWidth66);
		    rect66 = constrainSize (cw, slot66);

		    if (currentRect.width () == rect66.width () &&
			currentRect.x () == rect66.x ())
		    {
			desiredSlot.setWidth (slotWidth33);
			desiredSlot.setX (workarea.x () +
					  props.gravityRight * slotWidth66);
		    }
		}
	    }
	    else /* keys (2, 5, 8) */
	    {
		if (currentRect.width () == desiredRect.width () &&
		    currentRect.x () == desiredRect.x ())
		{
		    desiredSlot.setWidth (slotWidth33);
		    desiredSlot.setX (workarea.x () + slotWidth33);
		}
	    }
	    desiredRect = constrainSize (cw, desiredSlot);
	}

	xwc.x = desiredRect.x ();
	xwc.y = desiredRect.y ();
	xwc.width  = desiredRect.width ();
	xwc.height = desiredRect.height ();

	if (cw->mapNum ())
	    cw->sendSyncRequest ();

	/* TODO: animate move+resize */
	if (resize)
	cw->configureXWindow (CWX | CWY | CWWidth | CWHeight, &xwc);
    }

    return true;
}

void
GridScreen::glPaintRectangle (const GLScreenPaintAttrib &sAttrib,
			      const GLMatrix            &transform,
			      CompOutput                *output)
{
    CompRect rect;
    GLMatrix sTransform (transform);

    getPaintRectangle (rect);

    glPushMatrix ();

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    glLoadMatrixf (sTransform.getMatrix ());

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnable (GL_BLEND);

    /* fill rectangle */
    glColor4usv (optionGetFillColor ());
    glRecti (rect.x1 (), rect.y2 (), rect.x2 (), rect.y1 ());

    /* Set outline rect smaller to avoid damage issues */
    rect.setGeometry (rect.x () + 1, rect.y () + 1,
		      rect.width () - 2, rect.height () - 2);

    /* draw outline */
    glColor4usv (optionGetOutlineColor ());
    glLineWidth (2.0);
    glBegin (GL_LINE_LOOP);
    glVertex2i (rect.x1 (), rect.y1 ());
    glVertex2i (rect.x2 (), rect.y1 ());
    glVertex2i (rect.x2 (), rect.y2 ());
    glVertex2i (rect.x1 (), rect.y2 ());
    glEnd ();

    /* clean up */
    glColor4usv (defaultColor);
    glDisable (GL_BLEND);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glPopMatrix ();
}

bool
GridScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			   const GLMatrix            &matrix,
			   const CompRegion          &region,
			   CompOutput                *output,
			   unsigned int              mask)
{
    bool status;

    status = glScreen->glPaintOutput (attrib, matrix, region, output, mask);

    glPaintRectangle (attrib, matrix, output);

    return status;
}

GridType
GridScreen::edgeToGridType ()
{
    GridType ret;

    switch (edge) {
    case Left:
	ret = (GridType) optionGetLeftEdgeAction ();
	break;
    case Right:
	ret = (GridType) optionGetRightEdgeAction ();
	break;
    case Top:
	ret = (GridType) optionGetTopEdgeAction ();
	break;
    case Bottom:
	ret = (GridType) optionGetBottomEdgeAction ();
	break;
    case TopLeft:
	ret = (GridType) optionGetTopLeftCornerAction ();
	break;
    case TopRight:
	ret = (GridType) optionGetTopRightCornerAction ();
	break;
    case BottomLeft:
	ret = (GridType) optionGetBottomLeftCornerAction ();
	break;
    case BottomRight:
	ret = (GridType) optionGetBottomRightCornerAction ();
	break;
    case NoEdge:
    default:
	ret = GridUnknown;
	break;
    }

    return ret;
}

void
GridScreen::handleEvent (XEvent *event)
{
    bool damage = false;

    screen->handleEvent (event);

    if (event->type != MotionNotify)
	return;

    /* Detect corners first */
    /* Bottom Left */
    if (pointerY > (screen->height() - optionGetBottomEdgeThreshold()) &&
	pointerX < optionGetLeftEdgeThreshold())
	edge = BottomLeft;
    /* Bottom Right */
    else if (pointerY > (screen->height() - optionGetBottomEdgeThreshold()) &&
	     pointerX > (screen->width() - optionGetRightEdgeThreshold()))
	edge = BottomRight;
    /* Top Left */
    else if (pointerY < optionGetTopEdgeThreshold() &&
	    pointerX < optionGetLeftEdgeThreshold())
	edge = TopLeft;
    /* Top Right */
    else if (pointerY < optionGetTopEdgeThreshold() &&
	     pointerX > (screen->width() - optionGetRightEdgeThreshold()))
	edge = TopRight;
    /* Left */
    else if (pointerX < optionGetLeftEdgeThreshold())
	edge = Left;
    /* Right */
    else if (pointerX > (screen->width() - optionGetRightEdgeThreshold()))
	edge = Right;
    /* Top */
    else if (pointerY < optionGetTopEdgeThreshold())
	edge = Top;
    /* Bottom */
    else if (pointerY > (screen->height() - optionGetBottomEdgeThreshold()))
	edge = Bottom;
    /* No Edge */
    else
	edge = NoEdge;

    /* Detect when cursor enters another output */
    currentWorkarea = screen->getWorkareaForOutput (screen->outputDeviceForPoint (pointerX, pointerY));
    if (lastWorkarea != currentWorkarea)
    {
	lastWorkarea = currentWorkarea;

	if (cScreen)
	    cScreen->damageRegion (desiredSlot);

	initiateCommon (0, 0, o, edgeToGridType (), false);

	if (cScreen)
	    cScreen->damageRegion (desiredSlot);
    }

    /* Detect edge region change */
    if (lastEdge != edge)
    {
	lastEdge = edge;

	if (cScreen)
	    cScreen->damageRegion (desiredSlot);

	initiateCommon (0, 0, o, edgeToGridType (), false);

	if (cScreen)
	    cScreen->damageRegion (desiredSlot);
    }
}

void
GridWindow::grabNotify (int          x,
			int          y,
			unsigned int state,
			unsigned int mask)
{
    if (screen->grabExist ("move"))
    {
	gScreen->o.push_back (CompOption ("window", CompOption::TypeInt));
	gScreen->o[0].value ().set ((int) window->id ());

	screen->handleEventSetEnabled (gScreen, true);
	gScreen->glScreen->glPaintOutputSetEnabled (gScreen, true);
	grabIsMove = true;
    }
}

void
GridWindow::ungrabNotify ()
{
    if (grabIsMove)
    {
	gScreen->initiateCommon (0, 0, gScreen->o, gScreen->edgeToGridType (), true);

	screen->handleEventSetEnabled (gScreen, false);
	gScreen->glScreen->glPaintOutputSetEnabled (gScreen, false);
	grabIsMove = false;
	gScreen->cScreen->damageRegion (gScreen->desiredSlot);
    }

    gScreen->edge = NoEdge;
}


GridScreen::GridScreen (CompScreen *screen) :
    PluginClassHandler<GridScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    glScreen (GLScreen::get (screen)),
    GridOptions ()
{

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (glScreen, false);

    edge = lastEdge = NoEdge;
    currentWorkarea = lastWorkarea = screen->getWorkareaForOutput (screen->outputDeviceForPoint (pointerX, pointerY));

#define GRIDSET(opt,where,resize)			                             \
    optionSet##opt##Initiate (boost::bind (&GridScreen::initiateCommon,this, \
					   _1, _2, _3, where, resize))

    GRIDSET (PutCenterKey, GridCenter, true);
    GRIDSET (PutLeftKey, GridLeft, true);
    GRIDSET (PutRightKey, GridRight, true);
    GRIDSET (PutTopKey, GridTop, true);
    GRIDSET (PutBottomKey, GridBottom, true);
    GRIDSET (PutTopleftKey, GridTopLeft, true);
    GRIDSET (PutToprightKey, GridTopRight, true);
    GRIDSET (PutBottomleftKey, GridBottomLeft, true);
    GRIDSET (PutBottomrightKey, GridBottomRight, true);
    GRIDSET (PutMaximizeKey, GridMaximize, true);

#undef GRIDSET

}

GridWindow::GridWindow (CompWindow *window) :
    PluginClassHandler <GridWindow, CompWindow> (window),
    gScreen (GridScreen::get (screen)),
    window (window),
    grabIsMove (false)
{
    WindowInterface::setHandler (window);
}

/* Initial plugin init function called. Checks to see if we are ABI
 * compatible with core, otherwise unload */

bool
GridPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
        return false;

    return true;
}
