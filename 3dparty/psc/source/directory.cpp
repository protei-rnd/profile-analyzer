#if defined (_WIN32)
	#include <winsock2.h>
#endif // _WIN32

#include <vector>
#include <string>
#include <algorithm>
#include <psc/directory.h>
#include <string.h>
#include <stdio.h>

#if defined (_LINUX)
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <dirent.h>
	#include <stdlib.h>
#endif //_LINUX
#include <errno.h>

//#include <iostream>
//#include <psc/thread.h>
//#include <trace.h>

//std::ostream & operator << (std::ostream & strm, std::vector<std::string> const & strings);

namespace psc
{
	namespace filesystem
	{
#if defined (_WIN32)
			directory::directory (void)
				: _handle(INVALID_HANDLE_VALUE)
			{ }
			directory::~directory (void)
			{
				close();
			}
			void directory::close (void)
			{
				if (INVALID_HANDLE_VALUE != _handle)
					FindClose(_handle), _handle = INVALID_HANDLE_VALUE;
			}
			bool directory::start_file_enum (std::string const & from)
			{
				close();
				std::string what = from;
				if ( 0 < what.size() ) {
					char const last = from[from.length()-1];
					if (last != '/' && last != '\\')
						what += '/';
				}
				what += '*';
				_handle = FindFirstFile(what.c_str(), &_file_info);
				return INVALID_HANDLE_VALUE != _handle;
			}
			bool directory::next_file (void)
			{
				return TRUE == FindNextFile(_handle, &_file_info);
			}
			char const * const directory::file_name (void) const
			{
				return _file_info.cFileName;
			}
			bool directory::is_directory (void) const
			{
				return 0 != (_file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			}
			bool directory::is_file (void) const
			{
				return 0 == (_file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			}

			bool directory::current (std::string & str)
			{
				char buffer[MAX_PATH+1];
				unsigned len = GetCurrentDirectory(MAX_PATH, buffer);
				if (len)
					str.assign(buffer, buffer + len);
				return 0 != len;
			}

			bool directory::create (std::string const & str)
			{
				return 0 == mkdir(str.c_str());
			}

			int get_unique_file_id (std::string const & file_name, std::string & id)
			{
				int rc = 0;
				char buffer[2048];
				DWORD rez = GetFullPathName(file_name.c_str(), 2047, buffer, NULL);
				if (rez != 0) {
					if (rez > 2047) {
						char * dbuf = (char *)malloc(rez + 1);
						rez = GetFullPathName(file_name.c_str(), rez, dbuf, NULL);
						free(dbuf);
						dbuf[rez] = 0;
						id = dbuf;
					} else {
						id = buffer;
					}
				} else {
					rc = -1;
				}

				return rc;
			}
#endif // _WIN32

#if defined (_LINUX)
			directory::directory (void)
				: _handle(0)
				, _file_info(0)
			{ }
			directory::~directory(void)
			{
				close();
			}
			void directory::close (void)
			{
				if (_handle)
					::closedir(_handle), _handle = 0;
			}
			bool directory::start_file_enum (std::string const & from)
			{
				close();
				_from = from;
				char const last = from[from.length()-1];
				if (last != '/')
					_from += '/';
				_handle = opendir(from.c_str());
				if ( 0 != _handle )
					return next_file();
				return false;
			}
			bool directory::next_file (void)
			{
				return 0 != (_file_info = readdir(_handle));
			}
			bool directory::is_directory (void) const
			{
				std::string fname = _from;
				if (file_name())
					fname += file_name();
				struct stat path_stat;
				if (::stat(fname.c_str(), &path_stat) == 0)
					return S_ISDIR(path_stat.st_mode);
				return false;
			}
			bool directory::is_file (void) const
			{
				std::string fname = _from;
				if (file_name())
					fname += file_name();
				struct stat path_stat;
				if (::stat(fname.c_str(), &path_stat) == 0)
					return S_ISREG(path_stat.st_mode);
				return false;
			}
			char const * directory::file_name (void) const
			{
				if (_file_info)
					return _file_info->d_name;
				return 0;
			}

			bool directory::current (std::string & str)
			{
				long path_max = pathconf( ".", _PC_PATH_MAX );
				char * buffer = (char *)malloc(path_max);
				char const * const rbuf = getcwd(buffer, path_max);
				bool rez = rbuf == buffer;
				if (rez)
					str = buffer;
				free(buffer);
				return rez;
			}
			bool directory::create (std::string const & str)
			{
				//return 0 == mkdir(str.c_str(), S_IRWXU | S_IROTH | S_IRGRP);
				return 0 == mkdir(str.c_str(), 0777);
			}
			int get_unique_file_id (std::string const & file_name, std::string & id)
			{
				struct stat fst;
				int rc = stat(file_name.c_str(), &fst);
				if (0 == rc) {
					char buffer[32] = {0};
					int len = snprintf(buffer, 31, "%016lx", fst.st_ino);
					buffer[len] = 0;
					id = buffer;
				} else {
					rc = -1;
				}
				return rc;
			}

#endif // _LINUX

		bool directory::change (std::string const & name)
		{
			return 0 == chdir(name.c_str());
		}

		int convert_path_2_unix_style (std::string const & path, std::string & converted)
		{
			char const * const mask = "\\/";
			char const * const mfirst = mask;
			char const * const mlast = mask + strlen(mask);
			char const * first = path.c_str();
			char const *  last = path.c_str() + path.length();
			char const * begin = std::find_first_of(first, last, mfirst, mlast);
			for (; begin != last; ) {
				converted.append(first, begin);
				converted += '/';
				for (; '\\' == *begin || '/' == *begin; ++begin)
					;
				first = begin;
				begin = std::find_first_of(first, last, mfirst, mlast);
			}
			converted.append(first, begin);
			return 0;
		}

		int convert_path_2_unix_style (std::string & path)
		{
			std::string tmp;
			convert_path_2_unix_style (path, tmp);
			path = tmp;
			return 0;
		}

		int get_unix_path_as_vector (std::string const & path, std::vector<std::string> & vect)
		{
			int rc = 0;
			std::string p = path;
			convert_path_2_unix_style(p);

			char const * first = p.c_str();
			char const *  last = p.c_str() + p.length();
			char const * begin = std::find(first, last, '/');
			for (; begin != last; ) {
				if (begin != first) {
					vect.push_back(std::string());
					vect.back().append(first, begin);
				}
				for (; '/' == *begin; ++begin)
					;
				first = begin;
				begin = std::find(first, last, '/');
			}
			if (begin != first) {
				vect.push_back(std::string());
				vect.back().append(first, begin);
			}

			return rc;
		}


		bool is_win_absolute_path (std::string const & path)
		{
			return (path.size() >= 2 && 0 != isalpha(path[0]) && ':' == path[1] );
		}

		bool is_unix_absolute_path (std::string const & path)
		{
			return (path.size() > 0 && '/' == path[0]);
		}

		bool is_absolute_path (std::string const & path)
		{
			return is_unix_absolute_path(path) || is_win_absolute_path(path);
		}

		void convert_strings_2_unix_style (strings_t & strings)
		{
			strings_t::iterator first = strings.begin();
			strings_t::iterator last = strings.end();
			for (; first != last; ++first) {
				convert_path_2_unix_style(*first);
			}
		}

		void get_filename (std::string & fname, std::string const & fullname)
		{
			char const * first = fullname.c_str();
			char const *  last = fullname.c_str() + fullname.length();
			char const * begin = std::find(first, last, '/');
			for (; begin != last; ) {
				first = ++begin;
				begin = std::find(first, last, '/');
			}
			fname.assign(first, last);
		}


		void get_filename_without_extensition (std::string & name, std::string const & filename)
		{
			char const * first = filename.c_str();
			char const *  last = filename.c_str() + filename.length();
			char const * begin = std::find(first, last, '.');
			name.assign(first, begin);
		}

		void get_path (std::string & fname, std::string const & fullname)
		{
			char const * first = fullname.c_str();
			char const *  last = fullname.c_str() + fullname.length();
			char const * begin = std::find(first, last, '/');
			for (; begin != last; ) {
				first = ++begin;
				begin = std::find(first, last, '/');
			}
			fname.assign(fullname.c_str(), first);
		}

  //      int make_path (std::string const & path)
		//{
		//	std::string pwd;
		//	psc::fs::directory::current(pwd);
		//	strings_t vpath;
		//	if ( is_absolute_path(path) ) {
		//		psc::fs::directory::change("/");
		//	}
		//	int rc = 0;
		//	get_unix_path_as_vector(path, vpath);
		//	strings_t::iterator first = vpath.begin(), last = vpath.end();
		//	for (; first != last; ++first) {
		//		bool rl = psc::fs::directory::change(*first);
		//		if ( !rl ) {
		//			rl = psc::fs::directory::create(*first);
		//			if ( rl ) 
		//				rl = psc::fs::directory::change(*first);
		//			
		//			if ( !rl) {
		//				rc = -1;
		//				break;
		//			} 
		//		}
		//	}

		//	psc::fs::directory::change(pwd);
		//	return rc;
		//}

        int make_path (std::string const & path)
		{
			int rc = 0;
			strings_t vpath;
			get_unix_path_as_vector(path, vpath);
			strings_t::iterator first = vpath.begin(), last = vpath.end();
			std::string mpath;
			if (is_unix_absolute_path(path)) {
				mpath = "/";
			}
			for ( ; 0 == rc && first != last; ++first) {
				mpath += *first;
				mpath += '/';
#if defined(_LINUX)
				rc = mkdir(mpath.c_str(), 0777);
#elif defined(_WIN32)
				rc = mkdir(mpath.c_str());
#endif // _WIN32
				if ( -1 == rc && EEXIST == errno ) {
					rc = 0;
				}
				//if ( 0 == rc ) {
				//	mpath += '/';
				//}
			}

			return rc;
		}

		void append_prefix_2_nonabs_path (std::string const & prefix, strings_t & strings)
		{
			std::string tmp;
			strings_t::iterator first = strings.begin(), last = strings.end();
			for (; first != last; ++first) {
				if ( !is_absolute_path(*first) ) {
					tmp = prefix;
					if ( *prefix.rbegin() != '/' && *first->begin() != '/' ) {
						tmp += '/';
					}
					tmp += *first;
					*first = tmp;
				}
			}
		}

		void remove_relative_offset (std::string & str)
		{
			strings_t path;
			path.clear();

			get_unix_path_as_vector(str, path);
			size_t idx = 0;
			for ( ; idx < path.size(); ) {
				if ( path[idx] == "." ) {
					path.erase(path.begin() + idx);
				} else  if ( path[idx] == ".." ) {
					if ( idx > 0 ) {
						--idx;
						path.erase(path.begin() + idx);
						path.erase(path.begin() + idx);
					}
				} else {
					++idx;
				}
			}
			std::string tmp;
			if ( is_unix_absolute_path(str) ) {
				tmp = '/';
			}
			idx = 0;
			for (; idx < path.size() - 1; ++idx) {
				tmp += path[idx];
				tmp += '/';
			}
			tmp += path[idx];
			str = tmp;
		}

		void remove_relative_offset (strings_t & strings)
		{
			strings_t path;
			strings_t::iterator first = strings.begin(), last = strings.end();
			for (; first != last; ++first) {
				path.clear();
				get_unix_path_as_vector(*first, path);
				size_t idx = 0;
				for ( ; idx < path.size(); ) {
					if ( path[idx] == "." ) {
						path.erase(path.begin() + idx);
					} else  if ( path[idx] == ".." ) {
						if ( idx > 0 ) {
							--idx;
							path.erase(path.begin() + idx);
							path.erase(path.begin() + idx);
						} else {
							++idx;
						}
					} else {
						++idx;
					}
				}
				std::string tmp;
				if ( is_unix_absolute_path(*first) ) {
					tmp = '/';
				}
				idx = 0;
				for (; idx < path.size() - 1; ++idx) {
					tmp += path[idx];
					tmp += '/';
				}
				tmp += path[idx];
				*first = tmp;
			}
		}

		bool directory::remove (std::string const & name, bool recursive)
		{
			bool rc;
			if ( recursive ) {
				{
					directory d;
					rc = d.start_file_enum(name);
					if ( rc ) {
						std::string full_name;
						do {
							full_name  = name;
							full_name += "/";
							full_name += d.file_name();
							if ( d.is_file() ) {
								rc = 0 == ::remove(full_name.c_str());
							} else {
								if ( 0 != strcmp(".", d.file_name() ) && 0 != strcmp("..", d.file_name() ) ) {
									rc = remove(full_name, recursive);
								}
							}
						} while ( rc && d.next_file() );
					}
				}
				if ( rc ) {
					rc = 0 == ::rmdir(name.c_str());
				}
			} else {
				rc = 0 == ::rmdir(name.c_str());
			}
			return rc;
		}

		int enumerate_files_recursive ( std::string const & prefix, std::string const & ext, std::vector<std::string> & vect )
		{
			int rc = 0;
			directory folder;
			if ( folder.start_file_enum(prefix) ) {
				bool repeat = true;
				for ( ; repeat; ) {
					char const * szfile = folder.file_name();
					if ( folder.is_directory() && 0 != strcmp(szfile, ".") && 0 != strcmp(szfile, "..") ) {
						enumerate_files_recursive(prefix + "/" + szfile, ext, vect);
					} else {
						size_t fnlen = strlen(szfile);
						if ( fnlen > ext.length() && 0 == strncmp(szfile + fnlen - ext.length(), ext.c_str(), ext.length()) ) {
							vect.push_back( prefix + "/" + szfile );
						}
					}
					repeat = folder.next_file();
				}
			} else {
				rc = -1;
			}

			return rc;
		}

	}
}
