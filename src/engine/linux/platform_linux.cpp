/**
 * @file platform_linux.cpp
 * @brief Linux-specific OS wrapper
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iomanip>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sstream>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "trz/engine/internal/linux/platform_linux.h"

using namespace std;

namespace tredzone
{
    
//---- Soft (non-spurious) String to Int conversion ---------------------------

int	Soft_stoi(const string &s, const int def)
{
	try
	{	
		if (s.empty())	return def;
		
		const int	v = ::stoi(s);
		return v;
	
	}
	catch (std::runtime_error &e)
	{
		const char	*what_s = e.what();	// (don't allocate)
		(void)what_s;
	}
	
	return def;
}

//----Get Hostname IP address --------------------------------------------------

string  GetHostnameIP(const string &name)
{
    addrinfo    hints, *res_p;
    
    ::memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    const int err = ::getaddrinfo(nullptr/*node*/, name.c_str(), &hints, &res_p);
    if (err)    return "";          // error
    
    const string  res((const char*)res_p->ai_addr);
    
    ::freeaddrinfo(res_p);

    return res;
}

//---- Exec (linux) command ----------------------------------------------------

vector<string> exec_command(const string cmd_s, const bool strip_whitespace_f)
{
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd_s.c_str(), "r"), pclose);
    if (!pipe)  return {"error: popen() failed"};
    
    array<char, 512>    ascii_buffer;
    vector<string>      res;
    
    while (::fgets(ascii_buffer.data(), ascii_buffer.size(), pipe.get()) != nullptr)
    {
        string  s(ascii_buffer.data());
        
        if (s.empty())  continue;
        
        if (strip_whitespace_f)
        {   // chop any trailing eol
            if (s.back() == '\n')   s = string(s, 0, s.length() - 1);
            
            // strip tabs
            size_t  pos = 0;
            
            while ((pos = s.find('\t', pos)) != string::npos)
            {
                s.replace(pos, 1/*len*/, " ");
            }
        }
            
        res.push_back(s);
    }
    
    return res;
}

// cf: http://linux.die.net/man/3/strerror_r

string systemErrorToString(int err)
{
    assert(err != 0);
    char buffer[4096];

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE
    // POSIX strerror_r() returns int
    if (strerror_r(err, buffer, sizeof(buffer)))
    {
        return "<internal strerror_r() error>";
    }
    
    const size_t sz = ::strlen(buffer);
    if (sz > 0 && buffer[sz - 1] == '\n')
    {
        buffer[sz - 1] = '\0';
    }
    
    return buffer;

#else
    // GNU strerror_r() returns const char*
    return strerror_r(err, buffer, sizeof(buffer));
#endif
}

// put_time is missing in gcc 4.9
// https://stackoverflow.com/questions/14136833/stdput-time-implementation-status-in-gcc#14137287

#if __GNUC__ >= 5

//---- Current ISO date/time UTC -----------------------------------------------

string currentISOTimeUTC(const string &fmt_s)
{
    auto now = chrono::system_clock::now();
    auto itt = chrono::system_clock::to_time_t(now);
   
    ostringstream   ss;
    
    ss << put_time(gmtime(&itt), fmt_s.c_str());
    
    return ss.str();
}

//---- Current ISO date/time local ---------------------------------------------

string currentISOTimeLocal(const string &fmt_s)
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    tm  res;
    
    localtime_r(&in_time_t, &res);
    
    ostringstream    ss;
    
    ss << put_time(&res, fmt_s.c_str());
    
    return ss.str();
}

#endif // __GNUC__

} // namespace tredzone

// nada mas
