/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <esc/common.h>
#include <esc/driver.h>
#include <esc/messages.h>
#include <esc/sllist.h>
#include <esc/thread.h>
#include <esc/rect.h>
#include <tui/tuidev.h>
#include <stdlib.h>

#define DIFF(a,b)				((a) > (b) ? ((a) - (b)) : ((b) - (a)))

#define MAX_CLIENTS				16
#define MAX_REQC				30
#define MAX_RECTS				16

typedef struct {
	gpos_t x;
	gpos_t y;
	int cursor;
	int client;
} sNewCursor;

static sScreenMode *modes;
static size_t modeCount;
static sTUIClient clients[MAX_CLIENTS];
static fUpdateScreen updateScreen;
static fSetCursor setCursor;

static size_t mergeTolerance[] = {
	/* 0 = unused */	0,
	/* 1 = tui */		10,
	/* 2 = gui */		40
};
static sRectangle rects[MAX_RECTS];
static size_t rectCount;
static sNewCursor newCursor;

static void tui_addUpdate(int cli,gpos_t x,gpos_t y,gsize_t width,gsize_t height);
static void tui_addCursor(int cli,gpos_t x,gpos_t y,int cursor);
static void tui_update(void);
static size_t tui_find_client(inode_t cid);

void tui_driverLoop(const char *name,sScreenMode *modelist,size_t mcount,fSetMode setMode,
					fSetCursor setCur,fUpdateScreen updScreen) {
	modes = modelist;
	modeCount = mcount;
	updateScreen = updScreen;
	setCursor = setCur;

	int id = createdev(name,DEV_TYPE_BLOCK,DEV_OPEN | DEV_CLOSE);
	if(id < 0)
		error("Unable to register device '%s'",name);

	size_t reqc = 0;
	newCursor.client = -1;

	/* wait for messages */
	while(1) {
		sMsg msg;
		msgid_t mid;

		int fd = getwork(id,&mid,&msg,sizeof(msg),GW_NOBLOCK);
		if(fd < 0 || reqc >= MAX_REQC) {
			if(fd != -ENOCLIENT)
				printe("Unable to get work");
			reqc = 0;
			tui_update();
		}
		if(fd == -ENOCLIENT)
			wait(EV_CLIENT,id);
		else {
			/* see what we have to do */
			switch(mid) {
				case MSG_DEV_OPEN: {
					size_t i = tui_find_client(0);
					if(i == MAX_CLIENTS)
						msg.args.arg1 = -ENOMEM;
					else {
						clients[i].id = fd;
						clients[i].shm = NULL;
						clients[i].mode = NULL;
						clients[i].type = VID_MODE_TYPE_TUI;
						clients[i].data = NULL;
						msg.args.arg1 = 0;
					}
					send(fd,MSG_DEV_OPEN_RESP,&msg,sizeof(msg.args));
				}
				break;

				case MSG_DEV_CLOSE: {
					size_t i = tui_find_client(fd);
					if(i != MAX_CLIENTS) {
						setMode(clients + i,"",-1,VID_MODE_TYPE_TUI,false);
						clients[i].id = 0;
						close(fd);
					}
				}
				break;

				case MSG_SCR_UPDATE: {
					gpos_t x = (gpos_t)msg.args.arg1;
					gpos_t y = (gpos_t)msg.args.arg2;
					gsize_t width = (gsize_t)msg.args.arg3;
					gsize_t height = (gsize_t)msg.args.arg4;
					size_t i = tui_find_client(fd);
					if(i != MAX_CLIENTS)
						tui_addUpdate(i,x,y,width,height);
				}
				break;

				case MSG_SCR_GETMODES: {
					if(msg.args.arg1 == 0) {
						msg.args.arg1 = modeCount;
						send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.args));
					}
					else
						send(fd,MSG_DEF_RESPONSE,modes,modeCount * sizeof(sScreenMode));
				}
				break;

				case MSG_SCR_GETMODE: {
					size_t i = tui_find_client(fd);
					msg.data.arg1 = -EINVAL;
					if(clients[i].mode) {
						msg.data.arg1 = 0;
						memcpy(msg.data.d,clients[i].mode,sizeof(*clients[i].mode));
						clients[i].mode->type = clients[i].type;
					}
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.data));
				}
				break;

				case MSG_SCR_SETMODE: {
					size_t i;
					int modeno = msg.str.arg1;
					int type = msg.str.arg2;
					bool switchMode = msg.str.arg3;
					msg.args.arg1 = -EINVAL;
					for(i = 0; i < modeCount ; i++) {
						if(modeno == modes[i].id) {
							size_t j = tui_find_client(fd);
							if(j != MAX_CLIENTS)
								msg.args.arg1 = setMode(clients + j,msg.str.s1,i,type,switchMode);
							break;
						}
					}
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.args));
				}
				break;

				case MSG_SCR_SETCURSOR: {
					size_t i = tui_find_client(fd);
					if(i != MAX_CLIENTS && clients[i].mode) {
						gpos_t x = (gpos_t)msg.args.arg1;
						gpos_t y = (gpos_t)msg.args.arg2;
						int cursor = (int)msg.args.arg3;
						tui_addCursor(i,x,y,cursor);
					}
				}
				break;

				default:
					msg.args.arg1 = -ENOTSUP;
					send(fd,MSG_DEF_RESPONSE,&msg,sizeof(msg.args));
					break;
			}
		}
	}
}

static void tui_addUpdate(int cli,gpos_t x,gpos_t y,gsize_t width,gsize_t height) {
	/* first check if we can merge this rectangle with another one.
	 * maybe we have even got exactly this rectangle... */
	bool present = false;
	size_t tolerance = mergeTolerance[clients[cli].type];
	for(size_t i = 0; i < rectCount; ++i) {
		sRectangle *r = rects + i;
		if(r->window == cli &&
			(size_t)DIFF(r->x,x) < tolerance && (size_t)DIFF(r->y,y) < tolerance &&
			DIFF(r->width,width) < tolerance && DIFF(r->height,height) < tolerance) {
			/* mergable, so do it */
			rectAddTo(r,x,y,width,height);
			present = true;
			break;
		}
	}
	/* if not present yet, add it */
	if(!present) {
		if(rectCount == MAX_RECTS)
			tui_update();
		sRectangle *add = rects + rectCount++;
		add->x = x;
		add->y = y;
		add->width = width;
		add->height = height;
		add->window = cli;
	}
}

static void tui_addCursor(int cli,gpos_t x,gpos_t y,int cursor) {
	if(newCursor.client == cli) {
		newCursor.x = x;
		newCursor.y = y;
		newCursor.cursor = cursor;
	}
	else
		setCursor(clients + cli,x,y,cursor);
}

static void tui_update(void) {
	for(size_t i = 0; i < rectCount; ++i) {
		sRectangle *r = rects + i;
		updateScreen(clients + r->window,r->x,r->y,r->width,r->height);
	}
	rectCount = 0;

	if(newCursor.client != -1) {
		setCursor(clients + newCursor.client,newCursor.x,newCursor.y,newCursor.cursor);
		newCursor.client = -1;
	}
}

static size_t tui_find_client(inode_t cid) {
	size_t i;
	for(i = 0; i < MAX_CLIENTS; ++i) {
		if(clients[i].id == cid)
			return i;
	}
	return MAX_CLIENTS;
}