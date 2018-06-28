/**
 * @file maintest.cpp
 * @brief test main source
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

/*

launch specific test with, e.g.

./testd.bin --gtest_filter=AsyncEventLoop.init

*/

#include <csignal>

#include "testutil.h"

#include "trz/engine/platform.h"

using namespace std;

//----- Dump Stack Backtrace ---------------------------------------------------

static void DumpStackTrace(void)
{
#ifndef NDEBUG
    cout << "DEBUG dumping backtrace!" << endl;

    const vector<string> bt = tredzone::debugBacktrace(32);

    for (const auto s : bt)
    {
        cout << "  " << s << endl;
    }

#else

    cout << "RELEASE has no backtrace!" << endl;

#endif
}

//---- Abort Handler, also handles assert() -----------------------------------

extern "C" void my_abort_handler(int) { ::kill(0, SIGTRAP); }

int main(int argc, char **argv)
{
#ifndef NDEBUG
    signal(SIGABRT, &my_abort_handler);
#endif

    assert(argc > 0);

    ::testing::InitGoogleTest(&argc, argv);

    int res = 0;

    DumpStackTrace();

    try
    {
        res = RUN_ALL_TESTS();
    }
    catch (exception &e)
    {
        cout << "exception ERROR (" << e.what() << ")" << endl;
        exit(-1);
    }
    catch (...)
    {
        cout << "exception ERROR (?)" << endl;
        exit(-1);
    }
    
    cout << "Press enter to exit...";
    cin.get();

    return res;
}

