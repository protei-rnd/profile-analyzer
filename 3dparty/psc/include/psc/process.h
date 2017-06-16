#ifndef __PSC__MT_H
#define __PSC__MT_H

#include <string>

namespace psc {
	int create_process (std::string const & cmd, std::string const & in, std::string & out);
}

#endif // __PSC__MT_H
