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

#ifndef PROGRESSBAR_H_
#define PROGRESSBAR_H_

#include <esc/common.h>
#include <esc/gui/common.h>
#include <esc/gui/control.h>
#include <esc/gui/color.h>
#include <string>

namespace esc {
	namespace gui {
		class ProgressBar : public Control {
		private:
			static Color BGCOLOR;
			static Color BARCOLOR;
			static Color FGCOLOR;
			static Color BORDER_COLOR;

		public:
			ProgressBar(tCoord x,tCoord y,tSize width,tSize height)
				: Control(x,y,width,height), _position(0), _text("") {
			};
			ProgressBar(const esc::string &text,tCoord x,tCoord y,tSize width,tSize height)
				: Control(x,y,width,height), _position(0), _text(text) {
			};
			ProgressBar(const ProgressBar &b)
				: Control(b), _position(b._position), _text(b._text) {
			};
			virtual ~ProgressBar() {

			};
			ProgressBar &operator=(const ProgressBar &b);

			inline u32 getPosition() const {
				return _position;
			};
			inline void setPosition(u32 pos) {
				_position = MIN(100,pos);
				repaint();
			};
			inline esc::string getText() const {
				return _text;
			};
			inline void setText(const esc::string &text) {
				_text = text;
				repaint();
			};

			virtual void paint(Graphics &g);

		private:
			u32 _position;
			esc::string _text;
		};
	}
}

#endif /* PROGRESSBAR_H_ */