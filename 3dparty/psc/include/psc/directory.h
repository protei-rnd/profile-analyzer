#ifndef __PSC__DIRECTORY_H
#define __PSC__DIRECTORY_H

#include <string>
#include <vector>

#if defined (_WIN32)
	#include <winsock2.h>
	#include <windows.h>
	#include <direct.h>
#endif // _WIN32

#if defined (_LINUX)
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <dirent.h>
	#include <stdlib.h>
#endif //_LINUX

namespace psc
{
	namespace filesystem
	{
#if defined (_WIN32)
		struct directory
		{
			typedef HANDLE handle_type;
			typedef WIN32_FIND_DATA file_info_type;

			static bool current (std::string & str);
			static bool create (std::string const & str);
			static bool change (std::string const & name);
			static bool remove (std::string const & name, bool recursive = false);

			directory (void);
			~directory (void);

			void close (void);
			bool start_file_enum (std::string const & from);
			bool next_file (void);
			char const * const file_name (void) const;
			bool is_directory (void) const;
			bool is_file (void) const;
		protected:
			handle_type _handle;
			file_info_type _file_info;
		};

#elif defined (_LINUX)

		struct directory
		{
			typedef DIR * handle_type;
			typedef dirent * file_info_type;

			static bool current (std::string & str);
			static bool create (std::string const & str);
			static bool change (std::string const & name);
			static bool remove (std::string const & name, bool recursive = false);

			directory (void);
			~directory(void);
			void close (void);
			bool start_file_enum (std::string const & from);
			bool next_file (void);
			bool is_directory (void) const;
			bool is_file (void) const;
			char const * file_name (void) const;

		protected:
			std::string _from;
			handle_type _handle;
			file_info_type _file_info;
		};
#endif // _LINUX

		typedef std::vector<std::string> strings_t;

		int get_unique_file_id (std::string const & file_name, std::string & id);
		int convert_path_2_unix_style (std::string const & path, std::string & converted);
		int convert_path_2_unix_style (std::string & path);

		int get_unix_path_as_vector (std::string const & path, strings_t & vect);

		bool is_absolute_path (std::string const & path);

		void convert_strings_2_unix_style (strings_t & strings);

		void get_filename (std::string & fname, std::string const & fullname);
		void get_filename_without_extensition (std::string & name, std::string const & filename);
		void get_path (std::string & fname, std::string const & fullname);

		int make_path (std::string const & path);

		void append_prefix_2_nonabs_path (std::string const & prefix, strings_t & strings);

		void remove_relative_offset (strings_t & strings);
		void remove_relative_offset (std::string & str);

		int enumerate_files_recursive ( std::string const & prefix, std::string const & ext, std::vector<std::string> & vect);
	}

	namespace fs = filesystem;
}

#endif // __PSC__DIRECTORY_H
