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
    return CompRect (slot.x () + w->border ().left,
		     slot.y () + w->border ().top,
		     slot.width () - (w->border ().left + w->border ().right),
		     slot.height () - (w->border ().top + w->border ().bottom));
}

CompRect
GridScreen::constrainSize (CompWindow      *w,
			   const CompRect& slot)
{
    CompRect result;
    int      cw, ch;

    result = slotToRect (w, slot);

    if (w->constrainNewWindowSize (result.width (), result.height (), &cw, &ch))
    {
	/* constrained size may put window offscreen, adjust for that case */
	int dx = result.x () + cw - workarea.right () + w->border ().right;
	int dy = result.y () + ch - workarea.bottom () + w->border ().bottom;

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
    if (edgeToGridType () != GridUnknown && optionGetDrawIndicator ())
	cRect = desiredSlot;
    else
	cRect.setGeometry (0, 0, 0, 0);
}

int
applyProgress (int a, int b, float progress)
{
	return a < b ?
	 b - (ABS (a - b) * progress) :
	 b + (ABS (a - b) * progress);
}

void
GridScreen::setCurrentRect (Animation &anim)
{
	anim.currentRect.setLeft (applyProgress (anim.targetRect.x1 (),
													anim.fromRect.x1 (),
													anim.progress));
	anim.currentRect.setRight (applyProgress (anim.targetRect.x2 (),
													anim.fromRect.x2 (),
													anim.progress));
	anim.currentRect.setTop (applyProgress (anim.targetRect.y1 (),
													anim.fromRect.y1 (),
													anim.progress));
	anim.currentRect.setBottom (applyProgress (anim.targetRect.y2 (),
													anim.fromRect.y2 (),
													anim.progress));
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

	GRID_WINDOW (cw);

	if (gw->lastTarget != where)
	    gw->resizeCount = 0;

	props = gridProps[where];

	/* get current available area */
	if (cw == mGrabWindow)
	    workarea = screen->getWorkareaForOutput
			    (screen->outputDeviceForPoint (pointerX, pointerY));
	else
	{
	    workarea = screen->getWorkareaForOutput (cw->outputDevice ());

	    if (props.numCellsX == 1)
		centerCheck = true;

	    if (!gw->isGridResized)
		/* Store size not including borders when using a keybinding */
		gw->originalSize = slotToRect(cw, cw->serverBorderRect ());
	}

	if ((cw->state () & MAXIMIZE_STATE) &&
	    (resize || optionGetSnapoffMaximized ()))
	{
	    /* maximized state interferes with us, clear it */
	    cw->maximize (0);
	}

	if (where == GridMaximize && resize)
	{
	    /* move the window to the correct output */
	    if (cw == mGrabWindow)
	    {
		xwc.x = workarea.x () + 50;
		xwc.y = workarea.y () + 50;
		xwc.width = workarea.width ();
		xwc.height = workarea.height ();
		cw->configureXWindow (CWX | CWY, &xwc);
	    }
	    cw->maximize (MAXIMIZE_STATE);
	    gw->isGridResized = true;
	    gw->isGridMaximized = true;
		for (unsigned int i = 0; i < animations.size (); i++)
			animations.at (i).fadingOut = true;
	    return true;
	}

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

	if (desiredRect.y () == currentRect.y () &&
	    desiredRect.height () == currentRect.height () &&
	    where != GridMaximize && gw->lastTarget == where)
	{
	    int slotWidth25  = workarea.width () / 4;
	    int slotWidth33  = (workarea.width () / 3) + cw->border ().left;
	    int slotWidth17  = slotWidth33 - slotWidth25;
	    int slotWidth66  = workarea.width () - slotWidth33;
	    int slotWidth75  = workarea.width () - slotWidth25;

	    if (props.numCellsX == 2) /* keys (1, 4, 7, 3, 6, 9) */
	    {
		if ((currentRect.width () == desiredRect.width () &&
		    currentRect.x () == desiredRect.x ()) ||
		    (gw->resizeCount < 1) || (gw->resizeCount > 5))
		    gw->resizeCount = 3;

		/* tricky, have to allow for window constraints when
		 * computing what the 33% and 66% offsets would be
		 */
		switch (gw->resizeCount)
		{
		    case 1:
			desiredSlot.setWidth (slotWidth66);
			desiredSlot.setX (workarea.x () +
					  props.gravityRight * slotWidth33);
			gw->resizeCount++;
			break;
		    case 2:
			gw->resizeCount++;
			break;
		    case 3:
			desiredSlot.setWidth (slotWidth33);
			desiredSlot.setX (workarea.x () +
					  props.gravityRight * slotWidth66);
			gw->resizeCount++;
			break;
		    case 4:
			desiredSlot.setWidth (slotWidth25);
			desiredSlot.setX (workarea.x () +
					  props.gravityRight * slotWidth75);
			gw->resizeCount++;
			break;
		    case 5:
			desiredSlot.setWidth (slotWidth75);
			desiredSlot.setX (workarea.x () +
					  props.gravityRight * slotWidth25);
			gw->resizeCount++;
			break;
		    default:
			break;
		}
	    }
	    else /* keys (2, 5, 8) */
	    {

		if ((currentRect.width () == desiredRect.width () &&
		    currentRect.x () == desiredRect.x ()) ||
		    (gw->resizeCount < 1) || (gw->resizeCount > 5))
		    gw->resizeCount = 1;
	    
		switch (gw->resizeCount)
		{
		    case 1:
			desiredSlot.setWidth (workarea.width () -
					     (slotWidth17 * 2));
			desiredSlot.setX (workarea.x () + slotWidth17);
			gw->resizeCount++;
			break;
		    case 2:
			desiredSlot.setWidth ((slotWidth25 * 2) +
					      (slotWidth17 * 2));
			desiredSlot.setX (workarea.x () +
					 (slotWidth25 - slotWidth17));
			gw->resizeCount++;
			break;
		    case 3:
			desiredSlot.setWidth ((slotWidth25 * 2));
			desiredSlot.setX (workarea.x () + slotWidth25);
			gw->resizeCount++;
			break;
		    case 4:
			desiredSlot.setWidth (slotWidth33 -
			    (cw->border ().left + cw->border ().right));
			desiredSlot.setX (workarea.x () + slotWidth33);
			gw->resizeCount++;
			break;
		    case 5:
			gw->resizeCount++;
			break;
		    default:
			break;
		}
	    }

	    if (gw->resizeCount == 6)
		gw->resizeCount = 1;

	    desiredRect = constrainSize (cw, desiredSlot);
	}

	xwc.x = desiredRect.x ();
	xwc.y = desiredRect.y ();
	xwc.width  = desiredRect.width ();
	xwc.height = desiredRect.height ();

	/* Store a copy of xwc since configureXWindow changes it's values */
	XWindowChanges wc = xwc;

	if (cw->mapNum ())
	    cw->sendSyncRequest ();

	/* TODO: animate move+resize */
	if (resize)
	{
	    cw->configureXWindow (CWX | CWY | CWWidth | CWHeight, &xwc);
	    gw->isGridResized = true;
	    gw->isGridMaximized = false;
		for (unsigned int i = 0; i < animations.size (); i++)
			animations.at (i).fadingOut = true;
	}

	/* This centers a window if it could not be resized to the desired
	 * width. Without this, it can look buggy when desired width is
	 * beyond the minimum or maximum width of the window.
	 */
	if (centerCheck)
	{
	    if ((cw->serverBorderRect ().width () >
		 desiredSlot.width ()) ||
		 cw->serverBorderRect ().width () <
		 desiredSlot.width ())
	    {
		wc.x = (workarea.width () >> 1) -
		      ((cw->serverBorderRect ().width () >> 1) -
			cw->border ().left);
		cw->configureXWindow (CWX, &wc);
	    }

	    centerCheck = false;
	}

	gw->lastTarget = where;
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
	std::vector<Animation>::iterator iter;

    getPaintRectangle (rect);

	for (unsigned int i = 0; i < animations.size (); i++)
		setCurrentRect (animations.at (i));

    glPushMatrix ();

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    glLoadMatrixf (sTransform.getMatrix ());

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnable (GL_BLEND);

	for (iter = animations.begin (); iter != animations.end () && animating; iter++)
	{
		Animation& anim = *iter;
		float alpha = ((float) optionGetFillColorAlpha () / 65535.0f) * anim.opacity;

		/* fill rectangle */
		glColor4f (((float) optionGetFillColorRed () / 65535.0f) * alpha,
			   ((float) optionGetFillColorGreen () / 65535.0f) * alpha,
			   ((float) optionGetFillColorBlue () / 65535.0f) * alpha,
			   alpha);

		/* fill rectangle */
		glRecti (anim.currentRect.x1 (), anim.currentRect.y2 (),
				 anim.currentRect.x2 (), anim.currentRect.y1 ());

		/* Set outline rect smaller to avoid damage issues */
		anim.currentRect.setGeometry (anim.currentRect.x () + 1,
					      anim.currentRect.y () + 1,
					      anim.currentRect.width () - 2,
					      anim.currentRect.height () - 2);

		alpha = (float) (optionGetOutlineColorAlpha () / 65535.0f) * anim.opacity;

		/* draw outline */
		glColor4f (((float) optionGetOutlineColorRed () / 65535.0f) * alpha,
			   ((float) optionGetOutlineColorGreen () / 65535.0f) * alpha,
			   ((float) optionGetOutlineColorBlue () / 65535.0f) * alpha,
			   alpha);

		glLineWidth (2.0);

		glBegin (GL_LINE_LOOP);
		glVertex2i (anim.currentRect.x1 (),	anim.currentRect.y1 ());
		glVertex2i (anim.currentRect.x2 (),	anim.currentRect.y1 ());
		glVertex2i (anim.currentRect.x2 (),	anim.currentRect.y2 ());
		glVertex2i (anim.currentRect.x1 (),	anim.currentRect.y2 ());
		glEnd ();
	}

	if (!animating)
	{
		/* fill rectangle */
		float alpha = (float) optionGetFillColorAlpha () / 65535.0f;

		/* fill rectangle */
		glColor4f (((float) optionGetFillColorRed () / 65535.0f) * alpha,
			   ((float) optionGetFillColorGreen () / 65535.0f) * alpha,
			   ((float) optionGetFillColorBlue () / 65535.0f) * alpha,
			   alpha);
		glRecti (rect.x1 (), rect.y2 (), rect.x2 (), rect.y1 ());

		/* Set outline rect smaller to avoid damage issues */
		rect.setGeometry (rect.x () + 1, rect.y () + 1,
				  rect.width () - 2, rect.height () - 2);

		/* draw outline */
		alpha = (float) optionGetOutlineColorAlpha () / 65535.0f;

		/* draw outline */
		glColor4f (((float) optionGetOutlineColorRed () / 65535.0f) * alpha,
			   ((float) optionGetOutlineColorGreen () / 65535.0f) * alpha,
			   ((float) optionGetOutlineColorBlue () / 65535.0f) * alpha,
			   alpha);

		glLineWidth (2.0);
		glBegin (GL_LINE_LOOP);
		glVertex2i (rect.x1 (), rect.y1 ());
		glVertex2i (rect.x2 (), rect.y1 ());
		glVertex2i (rect.x2 (), rect.y2 ());
		glVertex2i (rect.x1 (), rect.y2 ());
		glEnd ();
	}

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
    CompOutput out;
    CompWindow *w;

    screen->handleEvent (event);

    if (event->type != MotionNotify || !mGrabWindow)
	return;

    out = screen->outputDevs ().at (
                   screen->outputDeviceForPoint (CompPoint (pointerX, pointerY)));

    /* Detect corners first */
    /* Bottom Left */
    if (pointerY > (out.y () + out.height () - optionGetBottomEdgeThreshold()) &&
        pointerX < out.x () + optionGetLeftEdgeThreshold())
	edge = BottomLeft;
    /* Bottom Right */
    else if (pointerY > (out.y () + out.height () - optionGetBottomEdgeThreshold()) &&
             pointerX > (out.x () + out.width () - optionGetRightEdgeThreshold()))
	edge = BottomRight;
    /* Top Left */
    else if (pointerY < optionGetTopEdgeThreshold() &&
	    pointerX < optionGetLeftEdgeThreshold())
	edge = TopLeft;
    /* Top Right */
    else if (pointerY < out.y () + optionGetTopEdgeThreshold() &&
             pointerX > (out.x () + out.width () - optionGetRightEdgeThreshold()))
	edge = TopRight;
    /* Left */
    else if (pointerX < out.x () + optionGetLeftEdgeThreshold())
	edge = Left;
    /* Right */
    else if (pointerX > (out.x () + out.width () - optionGetRightEdgeThreshold()))
	edge = Right;
    /* Top */
    else if (pointerY < out.y () + optionGetTopEdgeThreshold())
	edge = Top;
    /* Bottom */
    else if (pointerY > (out.y () + out.height () - optionGetBottomEdgeThreshold()))
	edge = Bottom;
    /* No Edge */
    else
	edge = NoEdge;

    /* Detect when cursor enters another output */

    currentWorkarea = screen->getWorkareaForOutput
			    (screen->outputDeviceForPoint (pointerX, pointerY));
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
		lastSlot = desiredSlot;

		if (edge == NoEdge)
			desiredSlot.setGeometry (0, 0, 0, 0);

		if (cScreen)
			cScreen->damageRegion (desiredSlot);

		initiateCommon (0, 0, o, edgeToGridType (), false);

		if (cScreen)
			cScreen->damageRegion (desiredSlot);

		if (lastSlot != desiredSlot)
		{
			if (animations.size ())
				/* Begin fading previous animation instance */
				animations.at (animations.size () - 1).fadingOut = true;

			if (edge != NoEdge)
			{
				CompWindow *cw = screen->findWindow (screen->activeWindow ());
				animations.push_back (Animation ());
				int current = animations.size () - 1;
				animations.at (current).fromRect	= cw->serverBorderRect ();
				animations.at (current).currentRect	= cw->serverBorderRect ();
				animations.at (current).timer = animations.at (current).duration;
				animations.at (current).targetRect = desiredSlot;

				if (lastEdge == NoEdge || !animating)
				{
					/* Cursor has entered edge region from non-edge region */
					animating = true;
					glScreen->glPaintOutputSetEnabled (this, true);
					cScreen->preparePaintSetEnabled (this, true);
					cScreen->donePaintSetEnabled (this, true);
				}
			}
		}

		lastEdge = edge;
    }

    w = screen->findWindow (CompOption::getIntOptionNamed (o, "window"));

    if (w)
    {
	GRID_WINDOW (screen->findWindow
				(CompOption::getIntOptionNamed (o, "window")));

	if ((gw->pointerBufDx > SNAPOFF_THRESHOLD ||
	     gw->pointerBufDy > SNAPOFF_THRESHOLD ||
	     gw->pointerBufDx < -SNAPOFF_THRESHOLD ||
	     gw->pointerBufDy < -SNAPOFF_THRESHOLD) &&
	     gw->isGridResized &&
	     optionGetSnapbackWindows ())
		restoreWindow (0, 0, o);
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
	gScreen->mGrabWindow = window;
	pointerBufDx = pointerBufDy = 0;

	if (!isGridResized && gScreen->optionGetSnapbackWindows ())
	    /* Store size not including borders when grabbing with cursor */
	    originalSize = gScreen->slotToRect(window,
						    window->serverBorderRect ());
    }

    if (screen->grabExist ("resize"))
    {
	isGridResized = false;
	resizeCount = 0;
    }

    window->grabNotify (x, y, state, mask);
}

void
GridWindow::ungrabNotify ()
{
    if (window == gScreen->mGrabWindow)
    {
	gScreen->initiateCommon
			(0, 0, gScreen->o, gScreen->edgeToGridType (), true);

	screen->handleEventSetEnabled (gScreen, false);
	gScreen->mGrabWindow = NULL;
	gScreen->o[0].value ().set (0);
	gScreen->cScreen->damageRegion (gScreen->desiredSlot);
    }

    gScreen->edge = NoEdge;

    window->ungrabNotify ();
}

void
GridWindow::moveNotify (int dx, int dy, bool immediate)
{
    window->moveNotify (dx, dy, immediate);

    pointerBufDx += dx;
    pointerBufDy += dy;
}

bool
GridScreen::restoreWindow (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector &option)
{
    XWindowChanges xwc;
    CompWindow *cw = screen->findWindow (screen->activeWindow ());

    if (!cw)
	return false;

    GRID_WINDOW (cw);

    if (!gw->isGridResized)
	return false;

    if (gw->isGridMaximized & !(cw->state () & MAXIMIZE_STATE))
	    gw->isGridMaximized = false;
    else
    {
        if (cw == mGrabWindow)
	{
	    xwc.x = pointerX - (gw->originalSize.width () >> 1);
	    xwc.y = pointerY + (cw->border ().top >> 1);
	}
	else
	{
	    xwc.x = gw->originalSize.x ();
	    xwc.y = gw->originalSize.y ();
	}
	xwc.width  = gw->originalSize.width ();
	xwc.height = gw->originalSize.height ();
	cw->maximize (0);
	cw->configureXWindow (CWX | CWY | CWWidth | CWHeight, &xwc);
	gw->pointerBufDx = 0;
	gw->pointerBufDy = 0;
    }
    gw->isGridResized = false;
    gw->resizeCount = 0;

    return true;
}

void
GridScreen::snapbackOptionChanged (CompOption *option,
				    Options    num)
{
    GRID_WINDOW (screen->findWindow
		    (CompOption::getIntOptionNamed (o, "window")));
    gw->isGridResized = false;
    gw->isGridMaximized = false;
    gw->resizeCount = 0;
}

void
GridScreen::preparePaint (int msSinceLastPaint)
{
	std::vector<Animation>::iterator iter;

	for (iter = animations.begin (); iter != animations.end (); iter++)
	{
		Animation& anim = *iter;
		anim.timer -= msSinceLastPaint;

		if (anim.timer < 0)
			anim.timer = 0;

		if (anim.fadingOut)
			anim.opacity -= msSinceLastPaint * 0.002;
		else
			if (anim.opacity < 1.0f)
				anim.opacity = anim.progress * anim.progress;
			else
				anim.opacity = 1.0f;

		if (anim.opacity < 0)
		{
			anim.opacity = 0.0f;
			anim.fadingOut = false;
			anim.complete = true;
		}

		anim.progress =	(anim.duration - anim.timer) / anim.duration;
	}

    cScreen->preparePaint (msSinceLastPaint);
}

void
GridScreen::donePaint ()
{
	std::vector<Animation>::iterator iter;

	for (iter = animations.begin (); iter != animations.end (); )
	{
		Animation& anim = *iter;
		if (anim.complete)
			iter = animations.erase(iter);
		else
			iter++;
	}

	if (animations.empty ())
	{
		cScreen->preparePaintSetEnabled (this, false);
		cScreen->donePaintSetEnabled (this, false);
		if (edge == NoEdge)
			glScreen->glPaintOutputSetEnabled (this, false);
		animations.clear ();
		animating = false;
	}

	cScreen->damageScreen ();

    cScreen->donePaint ();
}

Animation::Animation ()
{
	progress = 0.0f;
	fromRect = CompRect (0, 0, 0, 0);
	targetRect = CompRect (0, 0, 0, 0);
	currentRect = CompRect (0, 0, 0, 0);
	opacity = 0.0f;
	timer = 0.0f;
	duration = 250;
	complete = false;
	fadingOut = false;
}


GridScreen::GridScreen (CompScreen *screen) :
    PluginClassHandler<GridScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    glScreen (GLScreen::get (screen)),
    centerCheck (false),
    mGrabWindow (NULL),
    animating (false)
{

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (glScreen, false);

    edge = lastEdge = NoEdge;
    currentWorkarea = lastWorkarea = screen->getWorkareaForOutput
			    (screen->outputDeviceForPoint (pointerX, pointerY));

	animations.clear ();

#define GRIDSET(opt,where,resize)					       \
    optionSet##opt##Initiate (boost::bind (&GridScreen::initiateCommon, this,  \
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

    optionSetSnapbackWindowsNotify (boost::bind (&GridScreen::
				    snapbackOptionChanged, this, _1, _2));

    optionSetPutRestoreKeyInitiate (boost::bind (&GridScreen::
					    restoreWindow, this, _1, _2, _3));

}

GridWindow::GridWindow (CompWindow *window) :
    PluginClassHandler <GridWindow, CompWindow> (window),
    window (window),
    gScreen (GridScreen::get (screen)),
    isGridResized (false),
    isGridMaximized (false),
    pointerBufDx (0),
    pointerBufDy (0),
    resizeCount (0),
    lastTarget (GridUnknown)
{
    WindowInterface::setHandler (window);
}

GridWindow::~GridWindow ()
{
    if (gScreen->mGrabWindow == window)
	gScreen->mGrabWindow = NULL;

    gScreen->o[0].value ().set (0);
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
