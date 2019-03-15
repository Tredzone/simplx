/**
 * @file datastrem.h
 * @brief binary stream i/o
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cassert>
#include <cstdlib>
#include <string>
#include <limits>
#include <istream>
#include <ostream>
#include <string.h>         // (for strlen)

#include "trz/engine/internal/endianness.h"

namespace tredzone
{
// import into namespace
using std::string;

const size_t	MAX_DATA_STRING_LEN = 0x200000ul;

//---- Data INPUT Stream -------------------------------------------------------

template <bool _SWAP_F = false>
class DataInputStream
{
public:

	DataInputStream(std::istream &is)
		: m_IS(is)
{	
}

//---- Read 8 ------------------------------------------------------------------

char	Read8(void)
{
	char    c;
    
    m_IS.get(c/*&*/);
    
    return c;
}

//---- Read Bool ---------------------------------------------------------------

bool	ReadBool(void)
{
    return (bool) Read8();
}

//---- Read 16 -----------------------------------------------------------------

uint16_t	Read16(void)
{
	uint16_t	tmp;
	
	m_IS.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
	
	return _SWAP_F ? netswap16(tmp) : tmp;
}

//---- Read 32 -----------------------------------------------------------------

uint32_t	Read32(void)
{
	uint32_t	tmp;
	
	m_IS.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
	
    return _SWAP_F ? netswap32(tmp) : tmp;
}

//---- Read 64 -----------------------------------------------------------------

uint64_t	Read64(void)
{
	uint64_t	tmp;
	
	m_IS.read((char*)&tmp, sizeof(tmp));
	
	return _SWAP_F ? netswap64(tmp) : tmp;
}

//---- Read String -------------------------------------------------------------

string	ReadString(void)
{
	// read string length (swap is implicitly handled)
	const size_t	sz = static_cast<size_t>(Read32());
	if (0 == sz)	return "";	// empty string
	
	assert(sz < MAX_DATA_STRING_LEN);
	
    // reserve space
	string  res(sz, 0/*clear val*/);
    
    // read string data to buffer
	m_IS.read(&res[0], sz);
	
	return res;
}

//---- Read RAW Buffer ---------------------------------------------------------

size_t	ReadRawBuffer(uint8_t *p, size_t sz)
{
	m_IS.read(reinterpret_cast<char*>(p), sz);
    
    return sz;
}

//---- pipe operators ----------------------------------------------------------

DataInputStream&	operator>>(bool &b)         {b = ReadBool(); return *this;}
DataInputStream&	operator>>(uint8_t &ui)     {ui = (uint8_t)Read8(); return *this;}
DataInputStream&	operator>>(uint16_t &ui)    {ui = Read16(); return *this;}
DataInputStream&	operator>>(uint32_t &ui)    {ui = Read32(); return *this;}
DataInputStream&	operator>>(int8_t &i)       {i = (int8_t)Read8(); return *this;}
DataInputStream&	operator>>(int16_t &i)      {i = (int16_t)Read16(); return *this;}
DataInputStream&	operator>>(int32_t &i)      {i = (int32_t)Read32(); return *this;}
DataInputStream&	operator>>(uint64_t &ui)    {ui = Read64(); return *this;}
// DataInputStream&	operator>>(size_t &sz)      {sz = (size_t)Read64(); return *this;}          // can't overload?
DataInputStream&	operator>>(string &s)       {s = ReadString(); return *this;}

private:

    std::istream    &m_IS;
};

//---- DataOutputStream --------------------------------------------------------

template <bool _SWAP_F = false>
class DataOutputStream
{
public:
	DataOutputStream(std::ostream &os)
        : m_OS(os)
    {
    }
    
	virtual ~DataOutputStream() = default;
	
	DataOutputStream&	Write8(const uint8_t &v)
    {
        m_OS.put(v);
        return *this;
    }
    
	DataOutputStream&	Write16(const uint16_t &v)
    {
        const uint16_t	tmp = _SWAP_F ? netswap16(v) : v;
	
        m_OS.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        return *this;
    }
    
	DataOutputStream&	Write32(const uint32_t &v)
	{
        const uint32_t	tmp = netswap32(v);
	
        m_OS.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        return *this;
    }
    
    DataOutputStream&	Write64(const uint64_t &v)
	{
        const uint64_t	tmp = netswap64(v);
	
        m_OS.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        return *this;
    }
    
//---- Write ASCII String ------------------------------------------------------

DataOutputStream&    WriteASCII(const char *ascii_s)
{
	assert(ascii_s);
	
	const size_t	sz = ::strlen(ascii_s);
	
	assert(sz < MAX_DATA_STRING_LEN);
	
	// write length
	Write32(sz);

	if (sz == 0)	return *this;		// MSVC complains otherwise

	// then write string data
	m_OS.write(ascii_s, sz);
    return *this;
}

//---- Write String ------------------------------------------------------------

DataOutputStream&	WriteString(const string &s)
{
	const size_t	sz = s.length();
	// if (sz == 0)	{} 	// (legit, writes the 4-byte length (0), then nothing)      // was old version? [PL]
	
	assert(sz < MAX_DATA_STRING_LEN);
	
	// write string length
	Write32(sz);

	if (sz == 0)	return *this;		// MSVC complains otherwise
	
	const char	*data = (const char*) s.c_str();
	
	// then write string data
	m_OS.write(data, sz);
    return *this;
}

//---- Write RAW Buffer --------------------------------------------------------

DataOutputStream&	WriteRawBuffer(const uint8_t *data, const size_t sz)
{
	// (no header)
	m_OS.write(reinterpret_cast<const char*>(data), sz);
    return *this;
}
    
//---- pipes -------------------------------------------------------------------

DataOutputStream&	operator<<(const bool &b)       {return Write8(b);}
DataOutputStream&	operator<<(const uint8_t &ui)   {return Write8(ui);}
DataOutputStream&	operator<<(const int8_t &i)     {return Write8(i);}
DataOutputStream&	operator<<(const uint16_t &ui)  {return Write16(ui);}
DataOutputStream&	operator<<(const int16_t &i)    {return Write16(i);}
DataOutputStream&	operator<<(const uint32_t &ui)  {return Write32(ui);}
DataOutputStream&	operator<<(const int32_t &i)    {return Write32(i);}
DataOutputStream&	operator<<(const uint64_t &ui)  {return Write64(ui);}
DataOutputStream&	operator<<(const int64_t &i)    {return Write64(i);}
DataOutputStream&	operator<<(const string &s)     {return WriteString(s);}
DataOutputStream&	operator<<(const char *s)       {return WriteASCII(s);}
    
private:

	std::ostream     &m_OS;
};

} // namespace tredzone


