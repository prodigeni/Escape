/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
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
#include <esc/keycodes.h>
#include <esc/signals.h>
#include <esc/io.h>
#include <esc/service.h>
#include <esc/fileio.h>
#include <esc/heap.h>
#include <string.h>
#include "vtctrl.h"
#include "vtin.h"
#include "vtout.h"
#include "keymap.h"
#include "keymap.us.h"
#include "keymap.ger.h"

#define RLBUF_INCR			20

typedef sKeymapEntry *(*fKeymapGet)(u8 keyCode);

/**
 * @return the current position in the readline-buffer
 */
static u32 vterm_rlGetBufPos(sVTerm *vt);

/**
 * Handles the given keycode for readline
 *
 * @return true if handled
 */
static bool vterm_rlHandleKeycode(sVTerm *vt,u8 keycode);

/* our keymaps */
static fKeymapGet keymaps[] = {
	keymap_us_get,
	keymap_ger_get
};

/* key-states */
static bool shiftDown = false;
static bool altDown = false;
static bool ctrlDown = false;

bool vterm_translateKeycode(sVTerm *vt,bool isBreak,u32 keycode,u8 *modifier,char *c) {
	sKeymapEntry *e;
	if(vt == NULL)
		return false;

	/* handle shift, alt and ctrl */
	switch(keycode) {
		case VK_LSHIFT:
		case VK_RSHIFT:
			shiftDown = !isBreak;
			break;
		case VK_LALT:
		case VK_RALT:
			altDown = !isBreak;
			break;
		case VK_LCTRL:
		case VK_RCTRL:
			ctrlDown = !isBreak;
			break;
	}

	/* we don't need breakcodes anymore */
	if(isBreak)
		return false;

	e = keymaps[vt->keymap](keycode);
	if(e != NULL) {
		*modifier = (altDown ? STATE_ALT : 0) | (ctrlDown ? STATE_CTRL : 0) |
				(shiftDown ? STATE_SHIFT : 0);
		if(shiftDown)
			*c = e->shift;
		else if(altDown)
			*c = e->alt;
		else
			*c = e->def;
		return true;
	}
	return false;
}

void vterm_handleKey(sVTerm *vt,u32 keycode,u8 modifier,char c) {
	if((modifier & STATE_SHIFT) && vt->navigation) {
		switch(keycode) {
			case VK_PGUP:
				vterm_scroll(vt,vt->rows);
				return;
			case VK_PGDOWN:
				vterm_scroll(vt,-vt->rows);
				return;
			case VK_UP:
				vterm_scroll(vt,1);
				return;
			case VK_DOWN:
				vterm_scroll(vt,-1);
				return;
		}
	}

	if(c == NPRINT || (modifier & (STATE_CTRL | STATE_ALT))) {
		if(vt->handlerShortcut) {
			if(!vt->handlerShortcut(vt,keycode,modifier,c))
				return;
		}

		/* in reading mode? */
		if(vt->readLine) {
			if(vt->echo)
				vterm_rlHandleKeycode(vt,keycode);
		}
	}
	else if(vt->readLine)
		vterm_rlPutchar(vt,c);

	/* send escape-code when we're not in readline-mode */
	if(!vt->readLine) {
		bool empty = rb_length(vt->inbuf) == 0;
		char escape[SSTRLEN("\033[kc;123;123;7]") + 1];
		sprintf(escape,"\033[kc;%d;%d;%d]",c,keycode,modifier);
		rb_writen(vt->inbuf,escape,strlen(escape));
		if(empty)
			setDataReadable(vt->sid,true);
	}
	if(vt->echo && vt->setCursor)
		vt->setCursor(vt);
}

void vterm_rlFlushBuf(sVTerm *vt) {
	u32 i,len,bufPos = vterm_rlGetBufPos(vt);
	if(vt->echo)
		bufPos++;

	len = rb_length(vt->inbuf);

	i = 0;
	while(bufPos > 0) {
		rb_write(vt->inbuf,vt->rlBuffer + i);
		bufPos--;
		i++;
	}
	vt->rlBufPos = 0;

	if(len == 0)
		setDataReadable(vt->sid,true);
}

void vterm_rlPutchar(sVTerm *vt,char c) {
	switch(c) {
		case '\b': {
			u32 bufPos = vterm_rlGetBufPos(vt);
			if(bufPos > 0) {
				if(vt->echo) {
					u32 i = (vt->currLine * vt->cols * 2) + (vt->row * vt->cols * 2) + (vt->col * 2);
					/* move the characters back in the buffer */
					memmove(vt->buffer + i - 2,vt->buffer + i,(vt->cols - vt->col) * 2);
					vt->col--;
				}

				/* move chars back */
				memmove(vt->rlBuffer + bufPos - 1,vt->rlBuffer + bufPos,vt->rlBufPos - bufPos);
				/* remove last */
				vt->rlBuffer[vt->rlBufPos - 1] = '\0';
				vt->rlBufPos--;

				/* overwrite line */
				vterm_markDirty(vt,vt->row * vt->cols * 2 + vt->col * 2,vt->cols * 2);
			}
		}
		break;

		case '\r':
		case '\a':
		case '\t':
			/* ignore */
			break;

		default: {
			bool flushed = false;
			bool moved = false;
			u32 bufPos = vterm_rlGetBufPos(vt);

			/* increase buffer size? */
			if(vt->rlBuffer && bufPos >= vt->rlBufSize) {
				vt->rlBufSize += RLBUF_INCR;
				vt->rlBuffer = (char*)realloc(vt->rlBuffer,vt->rlBufSize);
			}
			if(vt->rlBuffer == NULL)
				return;

			/* move chars forward */
			if(bufPos < vt->rlBufPos) {
				memmove(vt->rlBuffer + bufPos + 1,vt->rlBuffer + bufPos,vt->rlBufPos - bufPos);
				moved = true;
			}

			/** add char */
			vt->rlBuffer[bufPos] = c;
			vt->rlBufPos++;

			/* TODO later we should allow "multiline-editing" */
			if(c == '\n' || vt->rlStartCol + vt->rlBufPos >= vt->cols) {
				flushed = true;
				vterm_rlFlushBuf(vt);
			}

			/* echo character, if required */
			if(vt->echo) {
				if(moved && !flushed) {
					u32 count = vt->rlBufPos - bufPos + 1;
					char *copy = (char*)malloc(count * sizeof(char));
					if(copy != NULL) {
						/* print the end of the buffer again */
						strncpy(copy,vt->rlBuffer + bufPos,count - 1);
						copy[count - 1] = '\0';
						vterm_puts(vt,copy,count - 1,false);
						free(copy);

						/* reset cursor */
						vt->col = vt->rlStartCol + bufPos + 1;
					}
				}
				else if(c != IO_EOF)
					vterm_putchar(vt,c);
			}
			if(flushed)
				vt->rlStartCol = vt->col;
		}
		break;
	}
}

static u32 vterm_rlGetBufPos(sVTerm *vt) {
	if(vt->echo)
		return vt->col - vt->rlStartCol;
	else
		return vt->rlBufPos;
}

static bool vterm_rlHandleKeycode(sVTerm *vt,u8 keycode) {
	bool res = false;
	switch(keycode) {
		case VK_LEFT:
			if(vt->col > vt->rlStartCol)
				vt->col--;
			res = true;
			break;
		case VK_RIGHT:
			if(vt->col < vt->rlStartCol + vt->rlBufPos)
				vt->col++;
			res = true;
			break;
		case VK_HOME:
			if(vt->col != vt->rlStartCol)
				vt->col = vt->rlStartCol;
			res = true;
			break;
		case VK_END:
			if(vt->col != vt->rlStartCol + vt->rlBufPos)
				vt->col = vt->rlStartCol + vt->rlBufPos;
			res = true;
			break;
	}
	return res;
}