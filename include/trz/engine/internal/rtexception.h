/**
 * @file rtexception.h
 * @brief internal exception-related classes
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <sstream>
#include <stdexcept>

namespace tredzone
{

/**
 * @brief Specialization of std::runtime_error exception.
 */
class RunTimeException : public std::runtime_error
{
  public:
    /**
     * @brief Provides a formatted string for a source code file, name and line location, as well as an optional
     * message.
     *
     * Generally source file, name and line, are provided using the __FILE__ and __LINE__ standard ANSI macros.
     * <br>The source file name will be extracted if provided with a path. The extraction is OS neutral.
     * @param[in]	sourceFileName	Name of the source file. Can be a path.
     * @param[in]	sourceFileLine	Line number in the source file.
     * @param[in]	message			Optional pointer to a description string, ignored if 0.
     * @return An instance of the formatted string.
     */
    inline static std::string formatSourceFileNameLine(const char *sourceFileName, int sourceFileLine,
                                                       const std::string *message = 0);

    /**
     * A constructor that uses formatSourceFileNameLine() to create the message passed to the parent exception.
     */
    RunTimeException(const char *sourceFileName, int sourceFileLine, const std::string &message)
        : std::runtime_error(formatSourceFileNameLine(sourceFileName, sourceFileLine, &message))
    {
    }

    /**
     * @overload
     */
    RunTimeException(const char *sourceFileName, int sourceFileLine)
        : std::runtime_error(formatSourceFileNameLine(sourceFileName, sourceFileLine))
    {
    }
};

std::string RunTimeException::formatSourceFileNameLine(const char *sourceFileName, int sourceFileLine,
                                                       const std::string *message)
{
    const char *fileName = sourceFileName;
    for (; *sourceFileName != '\0'; ++sourceFileName)
    {
        if (*sourceFileName == '\\' || *sourceFileName == '/' || *sourceFileName == ':')
        {
            fileName = sourceFileName + 1;
        }
    }
    std::stringstream s;
    s << fileName << ':' << sourceFileLine;
    if (message != 0)
    {
        s << " (" << *message << ")";
    }
    s << std::ends;
    return s.str();
}

} // namespace