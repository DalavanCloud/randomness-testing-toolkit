#ifndef RTT_BATTERIES_TESTRUNNER_H
#define RTT_BATTERIES_TESTRUNNER_H

#include <poll.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>

#include "rtt/batteries/itest-batt.h"

namespace rtt {
namespace batteries {

/* This is static class responsible for execution of given set of tests.
 * Multiple tests can be executed at once. */
class TestRunner {
public:

    /* At all times, two threads are running.
     * Main thread -    Handles incoming signals about
     *                  finished processes, announces finished PIDs.
     * Thread manager - Handles starting and joining of process threads.
     * Process thread - Starts process and waits for its PID to be announced.
     *                  If PID isn't announced withit TIMEOUT, kills process.
     *                  Also starts reader thread and joins it at the end.
     * Reader thread  - Reads process pipes. Usually is blocked by read.
     *                  It is important that this thread is running, otherwise
     *                  pipes could be filled up, blocking associated process.*/
    static const int MAX_TESTS_EXEC             = 8;
    static const int PROCESS_TIMEOUT_SECONDS    = 600;

    static const int THREAD_STATE_PENDING       = 0;
    static const int THREAD_STATE_READY         = 1;
    static const int THREAD_STATE_DONE          = 2;

    /* Called from battery's runTests code. This is main thread,
     * creates one threadManager and receives and hands out IDs of
     * finished child processes of RTT. */
    static void executeTests(std::vector<std::unique_ptr<ITest> > &tests);

    /* Called from test code in method execute. Thread is not created
     * directly from this method, but from test's execute. */
    static std::string executeBinary(const std::string & binaryPath,
                                     const std::string & arguments,
                                     const std::string & input = "");
private:
    /* Manages creation of maximum number of threads. After creation
     * wait for the threads to end and joins them. */
    static void threadManager(std::vector<std::unique_ptr<ITest>> & tests);

    static void readOutput(std::string & output, std::string & stderr,
                           int * stdout_pipe, int * stderr_pipe);

    static char ** buildArgv(const std::string & arguments , int * argc);

    static void destroyArgv(int argc , char ** argv);
};

} // namespace batteries
} // namespace rtt

#endif // RTT_BATTERIES_TESTRUNNER_H