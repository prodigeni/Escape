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

#ifndef FILE_H_
#define FILE_H_

#include <esc/io.h>
#include <esc/date.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace std {
	class file {
	public:
		typedef size_t size_type;
		typedef tTime time_type;
		typedef u16 user_type;
		typedef u16 group_type;
		typedef tInodeNo inode_type;
		typedef tDevNo dev_type;
		typedef u16 mode_type;

	public:
		/**
		 * Builds a file-object for given path
		 *
		 * @param path the path (has not to be absolute)
		 * @throws io_exception if stat fails
		 */
		file(const string& path);
		/**
		 * Builds a file-object for <name> in <parent>
		 *
		 * @param parent the parent-path (has not to be absolute)
		 * @param name the filename
		 * @throws io_exception if stat fails
		 */
		file(const string& parent,const string& name);
		/**
		 * Copy-constructor
		 */
		file(const file& f);
		/**
		 * Assignment-op
		 */
		file& operator =(const file& f);
		/**
		 * Destructor
		 */
		virtual ~file();

		/**
		 * Builds a vector with all entries in the directory denoted by this file-object.
		 *
		 * @param pattern a pattern the files have to match
		 * @param showHidden wether to include hidden files/folders
		 * @return the vector
		 */
		vector<sDirEntry> list_files(bool showHidden,const string& pattern = string()) const;

		/**
		 * @return the mode of the file
		 */
		mode_type mode() const {
			return _info.mode;
		}

		/**
		 * @return wether its a file
		 */
		bool is_file() const {
			return MODE_IS_FILE(_info.mode);
		}
		/**
		 * @return wether its a directory
		 */
		bool is_dir() const {
			return MODE_IS_DIR(_info.mode);
		}
		/**
		 * @return the size of the file in bytes
		 */
		size_type size() const {
			return _info.size;
		}

		/**
		 * @return the inode-number
		 */
		inode_type inode() const {
			return _info.inodeNo;
		}
		/**
		 * @return the device-number
		 */
		dev_type device() const {
			return _info.device;
		}

		/**
		 * @return the user-id
		 */
		user_type uid() const {
			return _info.uid;
		}
		/**
		 * @return the group-id
		 */
		group_type gid() const {
			return _info.gid;
		}
		/**
		 * @return the number of hardlinks to it
		 */
		size_type links() const {
			return _info.linkCount;
		}

		/**
		 * @return timestamp of last modification
		 */
		time_type modified() const {
			return _info.modifytime;
		}
		/**
		 * @return timestamp of last access
		 */
		time_type accessed() const {
			return _info.accesstime;
		}
		/**
		 * @return timestamp of creation
		 */
		time_type created() const {
			return _info.createtime;
		}

		/**
		 * @return the filename
		 */
		const string &name() const {
			return _name;
		}
		/**
		 * @return the parent-path
		 */
		const string &parent() const {
			return _parent;
		}
		/**
		 * @return the absolute path
		 */
		string path() const {
			return _parent + "/" + _name;
		}

	private:
		/**
		 * Inits _info and _path
		 *
		 * @param parent the parent-path
		 * @param name the filename
		 * @throws io_exception if stat fails
		 */
		void init(const string& parent,const string& name);

	private:
		sFileInfo _info;
		string _parent;
		string _name;
	};
}

#endif /* FILE_H_ */