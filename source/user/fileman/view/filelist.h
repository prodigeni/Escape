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

#pragma once

#include <gui/layout/iconlayout.h>
#include <gui/layout/flowlayout.h>
#include <gui/panel.h>
#include <gui/imagebutton.h>
#include <gui/scrollpane.h>
#include <gui/label.h>
#include <file.h>
#include <list>

#include "pathbar.h"

class FileList : public gui::Panel {
	class FileObject : public gui::Panel {
	public:
		explicit FileObject(FileList &list,const std::file &file)
			: Panel(gui::make_layout<gui::FlowLayout>(
					gui::FlowLayout::CENTER,true,gui::FlowLayout::VERTICAL,4)), _list(list) {
			using namespace std;
			using namespace gui;
			string path = file.is_dir() ? "/etc/folder.bmp" : "/etc/file.bmp";
			shared_ptr<ImageButton> img = make_control<ImageButton>(Image::loadImage(path),false);
			shared_ptr<Label> lbl = make_control<Label>(file.name());
			if(file.is_dir())
				img->clicked().subscribe(bind1_mem_recv(file,this,&FileObject::onClick));
			add(img);
			add(lbl);
		}

		void onClick(const std::file &file,UIElement&) {
			gui::Application::getInstance()->executeLater(
					std::make_bind1_memfun(file,this,&FileObject::loadDir));
		}

	private:
		void loadDir(const std::file &file) {
			_list.loadDir(file.path());
		}

		FileList &_list;
	};

public:
	explicit FileList(std::shared_ptr<PathBar> pathbar)
		: Panel(gui::make_layout<gui::IconLayout>(gui::IconLayout::HORIZONTAL,4)),
		  _pathbar(pathbar), _files() {
	}

	void loadDir(const std::string &path) {
		using namespace std;
		list<file> files;
		try {
			vector<sDirEntry> entries = file(path).list_files(false);
			for(auto it = entries.begin(); it != entries.end(); ++it)
				files.push_back(file(path,it->name));
			files.sort([] (const file &a,const file &b) {
				if(a.is_dir() == b.is_dir())
					return a.name() < b.name();
				return a.is_dir();
			});
			setList(files);
			_pathbar->setPath(this,path);
		}
		catch(const io_exception& e) {
			cerr << e.what() << endl;
		}
	}

	void setList(const std::list<std::file> &files) {
		removeAll();
		_files = files;
		for(auto it = _files.begin(); it != _files.end(); ++it)
			add(gui::make_control<FileObject>(*this,*it));
		layout();
		static_cast<gui::ScrollPane*>(getParent())->scrollToTop(false);
		repaint();
	}

private:
	std::shared_ptr<PathBar> _pathbar;
	std::list<std::file> _files;
};
