/*
 * Stephen Kennedy <suasol@gmail.com>
 * 
 * compiz-fusion plugin to act like winsplit revolution (http://www.winsplit-revolution.com/)
 * use <Control><Alt>NUMPAD_KEY to move and tile your windows.
 * 
 * Press the tiling keys several times to cycle through some tiling options.
 */

#include <compiz-core.h>
#include <string.h>
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

static void
getWindowDecorationSize( CompWindow* cw, CompWindowExtents* border )
{
	memset(border, 0, sizeof(CompWindowExtents));
	CompWindow* w;
	for( w = cw->screen->windows; w; w = w->next )
	{
		if (w->input.left > border->left)
			border->left = w->input.left;
		if (w->input.right > border->right)
			border->right = w->input.right;
		if (w->input.top > border->top)
			border->top = w->input.top;
		if (w->input.bottom > border->bottom)
			border->bottom = w->input.bottom;
	}
}

static inline int roundDown( int x, int r )
{
	return x - (x%r);
}

static Bool
gridCommon(CompDisplay     *compdisplay,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int             nOption,
		 GridType		 where)
{
	Window     xid;
	CompWindow *cw;

	xid = getIntOptionNamed (option, nOption, "window", 0);
	cw   = findWindowAtDisplay (compdisplay, xid);
	if (cw)
	{
		CompWindowExtents border;
		getWindowDecorationSize(cw, &border);

		// get current rect including decorations
		XRectangle current;
		current.x = cw->attrib.x - border.left;
		current.y = cw->attrib.y - border.top;
		current.width = cw->attrib.width + border.left + border.right;
		current.height = cw->attrib.height + border.top + border.bottom;

		// slice and dice to get desired rect
		GridProps props = gridProps[where];
		XRectangle workarea;
		getWorkareaForOutput(cw->screen, outputDeviceForWindow(cw), &workarea);
		XRectangle desired;
		desired.y =  workarea.y + props.gravityDown * (workarea.height / props.numCellsY);
		desired.height = workarea.height / props.numCellsY;
		desired.x =  workarea.x + props.gravityRight * (workarea.width / props.numCellsX);
		desired.width = workarea.width / props.numCellsX;

		// TODO:
		// * handle size increment hints correctly
		// * read and use minimum size hints
		// * second '8' '5' and '2' keypresses fit window into the middle third.

		int winc = 1;
		int hinc = 1;
		if( cw->sizeHints.flags & PResizeInc )
		{
			winc = cw->sizeHints.width_inc;
			hinc = cw->sizeHints.height_inc;
		}
		desired.height = roundDown(desired.height, hinc);
		desired.width  = roundDown(desired.width, winc);

		//printf("\nattr(%i %i %i %i)\n", cw->attrib.x, cw->attrib.y, cw->attrib.width, cw->attrib.height);
		//printf("border(%i %i %i %i)\n", border.left, border.top, border.right, border.bottom);
		//printf("work(%i %i %i %i)\n", workarea.x, workarea.y, workarea.width, workarea.height);
		//printf("comp(%i %i %i %i)\n", desired.x, desired.y, desired.width, desired.height);

		if( (desired.y == current.y)
			&& (desired.height == current.height) )
		{
			int width66 = roundDown( (2*workarea.width) / 3, winc );
			int width33 = roundDown( workarea.width - width66, winc );

			if( props.numCellsX == 2 )
			{
				if( (current.width ==  desired.width)
					&& (current.x == desired.x) )
				{
					desired.width = width66;
					desired.x = workarea.x + props.gravityRight * width33;
				}
				else if( (current.width == width66)
					&& (current.x == workarea.x + props.gravityRight * width33) )
				{
					desired.width = width33;
					desired.x = workarea.x + props.gravityRight * width66;
				}
			}
			else // props.numCellsX == 1
			{
				if( (current.width ==  desired.width)
					&& (current.x == desired.x) )
				{
					desired.width = width33;
					desired.x = workarea.x + width33;
				}
			}
		}

		XWindowChanges xwc;
		xwc.x = desired.x + border.left;
		xwc.y = desired.y + border.top;
		xwc.width = desired.width - (border.left+border.right);
		xwc.height = desired.height - (border.top+border.bottom);
		unsigned mask = CWWidth | CWHeight | CWX | CWY;
		if (cw->mapNum && (mask & (CWWidth | CWHeight)))
		{
			sendSyncRequest (cw);
		}
		if( cw->state & MAXIMIZE_STATE )
		{
			maximizeWindow(cw,0); // max state interferes with us, clear it
		}
		//TODO: animate move+resize
		configureXWindow (cw, mask, &xwc);
	}

	return TRUE;
}

#define HANDLER(WHERE) \
	static Bool grid##WHERE(CompDisplay* d, CompAction* action, CompActionState state, CompOption* option, int nOption) \
	{ \
		return gridCommon(d, action, state, option, nOption, Grid##WHERE); \
	}
HANDLER(BottomLeft)
HANDLER(Bottom)
HANDLER(BottomRight)
HANDLER(Left)
HANDLER(Center)
HANDLER(Right)
HANDLER(TopLeft)
HANDLER(Top)
HANDLER(TopRight)

/* Configuration, initialization, boring stuff. --------------------- */
static Bool
gridInitDisplay (CompPlugin  *p,
			  CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    gridSetPutCenterKeyInitiate (d, gridCenter);
    gridSetPutLeftKeyInitiate (d, gridLeft);
    gridSetPutRightKeyInitiate (d, gridRight);
    gridSetPutTopKeyInitiate (d, gridTop);
    gridSetPutBottomKeyInitiate (d, gridBottom);
    gridSetPutTopleftKeyInitiate (d, gridTopLeft);
    gridSetPutToprightKeyInitiate (d, gridTopRight);
    gridSetPutBottomleftKeyInitiate (d, gridBottomLeft);
    gridSetPutBottomrightKeyInitiate (d, gridBottomRight);

    return TRUE;
}

static CompBool
gridInitObject (CompPlugin *p,
			 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) gridInitDisplay,
	0, 
	0 
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
gridFiniObject (CompPlugin *p,
			 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	0, 
	0, 
	0 
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable gridVTable = {
    "grid",
    0,
    0,
    0,
    gridInitObject,
    gridFiniObject,
    0,
    0
};

CompPluginVTable*
getCompPluginInfo (void)
{
    return &gridVTable;
}
