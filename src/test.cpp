#include <test.h>

#include <sstream>
#include <algorithm>
#include <sys/wait.h>
#include <message.h>
#include <util.h>

using namespace dtest;

///////////////////////////////////////////////////////////////////////////////

void Test::_run() {

    if (_isDriver) {
        if (_distributed()) {
            if (_numWorkers == 0) _numWorkers = _defaultNumWorkers;

            auto extraWorkers = DriverContext::instance->_allocateWorkers(_numWorkers);

            DriverContext::instance->_run(this);
            _driverRun();

            DriverContext::instance->_join(this);
            DriverContext::instance->_deallocateWorkers(extraWorkers);
        }
        else {
            _driverRun();
        }
    }
    else {
        _workerRun();
    }
}

std::string Test::__statusString[] = {
    "PENDING",
    "PASS",
    "PASS (with memory leak)",
    "FAIL",
    "TIMEOUT",
};

static std::string __statusStringPastTense[] = {
    "PENDING",
    "PASSED",
    "PASSED (with memory leak)",
    "FAILED",
    "TIMED OUT",
};

std::unordered_map<std::string, std::list<Test *>> Test::__tests;

std::list<std::string> Test::__err;

bool Test::_logStatsToStderr = false;

bool Test::_isDriver = false;

uint16_t Test::_defaultNumWorkers = 4;

thread_local bool Test::_trackMemory;

void Test::_enter() {
    _trackMemory = MemoryWatch::isTracking();
    MemoryWatch::track(false);
}

void Test::_exit() {
    MemoryWatch::track(_trackMemory);
}

std::string Test::_collectErrorMessages() {
    std::stringstream s;

    if (__err.size() > 0) {
        for (const auto &e : __err) s << e << std::endl;

        __err.clear();
    }

    return s.str();
}

bool Test::runAll(std::ostream &out) {

    _isDriver = true;
    Context::_currentCtx = DriverContext::instance.ptr();
    DriverContext::instance->_start();

    bool success = true;

    std::unordered_map<Status, uint32_t> result;

    std::list<Test *> ready;
    std::unordered_map<std::string, std::list<Test *>> blocked;
    std::unordered_map<std::string, std::unordered_set<Test *>> remaining;

    for (const auto &moduleTests : __tests) {
        for (auto t : moduleTests.second) {
            auto tt = t->copy();

            if (t->_dependencies.empty()) {
                ready.push_back(tt);
            }
            else {
                for (const auto &dep: t->_dependencies) {
                    blocked[dep].push_back(tt);
                }
            }

            remaining[t->_module].insert(tt);
        }
    }

    if (_logStatsToStderr) std::cerr << std::endl;

    uint32_t count = 0;
    while (! ready.empty()) {

        auto test = ready.front();
        ready.pop_front();

        auto testnum = std::to_string(count + 1);
        testnum.resize(5, ' ');

        auto testname = test->_module + "::" + test->_name;
        auto shortTestName = testname;
        if (testname.size() > 33) {
            shortTestName = 
                testname.substr(0, 14)
                + " ... "
                + testname.substr(testname.size() - 14);
        }
        else {
            shortTestName.resize(33, ' ');
        }

        out << "RUNNING TEST #" << testnum << "  " << testname << "   ";
        out.flush();
        if (_logStatsToStderr) {
            std::cerr << "RUNNING TEST #" << testnum << "  " << shortTestName  << "   ";
        }

        test->_run();

        auto statusStr = statusString(test->_status);
        out << statusStr << "\n";
        for (size_t i = 0; i < test->_childStatus.size(); ++i) {
            out << "  Child #" << i + 1 << "   " << statusString(test->_childStatus[i]) << "\n";
            out << indent(test->_childDetailedReport[i], 4) << "\n";
        }
        out << indent(test->_detailedReport, 2) << "\n";
        out.flush();
        if (_logStatsToStderr) std::cerr << statusStr << "\n";

        if (test->_status == Status::PASS) {
            auto it = remaining.find(test->_module);
            it->second.erase(test);     // remove from it module's remaining list of tests
            if (it->second.empty()) {   // if entire module test completed
                // find tests blocked on the (now) completed module
                auto it = blocked.find(test->_module);
                if (it != blocked.end()) {
                    for (auto tt : it->second) {
                        // and remove that module from their set of dependencies
                        tt->_dependencies.erase(test->_module);
                        // if no more dependencies are needed, then push to ready queue
                        if (tt->_dependencies.empty()) ready.push_back(tt);
                    }
                }
            }
        }
        else {
            success = false;
        }

        ++result[test->_status];
        ++count;
        delete test;
    }

    // print a dashed line
    std::string line = "";
    line.resize(80, '-');
    out << line << "\n";
    if (_logStatsToStderr) std::cerr << "\n" << line << "\n";

    // print summary
    std::stringstream resultSummary;
    for (const auto &r : result) {
        resultSummary
            << r.second << "/" << count
            << " TESTS " << __statusStringPastTense[(uint32_t) r.first]
            << "\n";
    }
    out << resultSummary.str();
    if (_logStatsToStderr) std::cerr << resultSummary.str();

    return success;
}

void Test::runWorker(uint32_t id) {

    _isDriver = false;
    Context::_currentCtx = WorkerContext::instance.ptr();
    WorkerContext::instance->_start(id);

    while (true) {
        WorkerContext::instance->_waitForEvent();
    }
}

// Context /////////////////////////////////////////////////////////////////////

Context * Context::_currentCtx = nullptr;

// DriverContext::WorkerHandle /////////////////////////////////////////////////


void DriverContext::WorkerHandle::run(const Test *test) {
    Message m;
    m << OpCode::RUN_TEST << test->_module << test->_name;
    m.send(_socket);
}

void DriverContext::WorkerHandle::notify() {
    Message m;
    m << OpCode::NOTIFY;
    m.send(_socket);
}

void DriverContext::WorkerHandle::terminate() {
    Message m;
    m << OpCode::TERMINATE;
    m.send(_socket);

    if (_pid != 0) {
        waitpid(_pid, NULL, 0);
    }
}

// DriverContext ///////////////////////////////////////////////////////////////

DriverContext::WorkerHandle DriverContext::_spawnWorker() {
    uint32_t id = _workers.size();
    pid_t pid = fork();

    if (pid == 0) {
        try {
            Test::runWorker(id);
            exit(0);
        }
        catch (...) {
            exit(1);
        }
    }
    else {
        WorkerHandle w(id, pid);
        _workers[id] = w;
        return w;
    }
}

void DriverContext::_start() {
    _socket = Socket(0, 128);
}

uint32_t DriverContext::_waitForEvent() {
    auto &conn = _socket.pollOrAccept();

    Message m;
    m.recv(conn);
    OpCode op;
    uint32_t id;
    m >> op >> id;

    switch (op) {
    case OpCode::WORKER_STARTED: {
        auto it = _workers.find(id);
        if (it == _workers.end()) return -1;
        m >> it->second._addr;
        it->second._running = true;
    }
    break;

    case OpCode::FINISHED_TEST: {
        auto it = _allocatedWorkers.find(id);
        if (it == _allocatedWorkers.end()) return -1;
        it->second._done = true;
        m >> it->second._status >> it->second._detailedReport;
    }
    break;

    case OpCode::NOTIFY: {
        auto it = _allocatedWorkers.find(id);
        if (it == _allocatedWorkers.end()) return -1;
        ++it->second._notifyCount;
    }
    break;

    default: break;
    }

    return id;
}

std::list<uint32_t> DriverContext::_allocateWorkers(uint16_t n) {
    std::list<uint32_t> spawned;

    while (_workers.size() < n) {
        auto w = _spawnWorker();
        spawned.push_back(w._id);
        _workers[w._id] = w;
    }

    for (auto &w : _workers) {
        if (n-- == 0) break;

        while (! w.second._running) {
            _waitForEvent();
        }

        w.second._notifyCount = 0;
        w.second._done = false;

        _allocatedWorkers[w.second._id] = w.second;
    }

    return spawned;
}

void DriverContext::_deallocateWorkers(std::list<uint32_t> &spawnedWorkers) {
    for (auto &w : spawnedWorkers) {
        _allocatedWorkers[w].terminate();
        _workers.erase(w);
    }

    _allocatedWorkers.clear();
}

void DriverContext::_run(const Test *test) {
    for (auto &w : _allocatedWorkers) {
        w.second.run(test);
    }
}

void DriverContext::_join(Test *test) {
    for (auto &w : _allocatedWorkers) {
        while (! w.second._done) {
            _waitForEvent();
        }

        test->_childStatus.push_back(w.second._status);
        test->_childDetailedReport.push_back(w.second._detailedReport);
    }
}

void DriverContext::notify() {
    Test::_enter();

    for (auto &w : _allocatedWorkers) {
        w.second.notify();
    }

    Test::_exit();
}

void DriverContext::wait(uint32_t n) {
    Test::_enter();

    if (n == -1u) n = _allocatedWorkers.size();

    std::unordered_set<uint32_t> pulled;

    for (auto &w : _allocatedWorkers) {
        if (pulled.size() == n) {
            Test::_exit();
            return;
        }

        if (w.second._notifyCount > 0) {
            --w.second._notifyCount;
            pulled.insert(w.second._id);
        }
    }

    while (pulled.size() != n) {
        auto id = _waitForEvent();
        if (id == -1u) continue;

        auto it = _allocatedWorkers.find(id);
        if (
            it->second._notifyCount > 0
            && pulled.count(it->second._id) == 0
        ) {
            --it->second._notifyCount;
            pulled.insert(it->second._id);
        }
    }

    Test::_exit();
}

Lazy<DriverContext> DriverContext::instance([] { return new DriverContext(); });

// WorkerContext ///////////////////////////////////////////////////////////////

void WorkerContext::_start(uint32_t id) {
    _id = id;

    _socket = Socket(0, 128);

    _driverSocket = Socket(DriverContext::instance->_socket.address());

    Message m;
    m << OpCode::WORKER_STARTED << _id << _socket.address();
    m.send(_driverSocket);
}

void WorkerContext::_waitForEvent() {

    auto &conn = _socket.pollOrAccept();

    Message m;
    m.recv(conn);
    OpCode op;
    m >> op;

    switch (op) {
    case OpCode::RUN_TEST: {
        if (_inTest) break;
        _inTest = true;

        std::string module;
        std::string name;

        m >> module >> name;

        auto it = std::find_if(
            Test::__tests[module].begin(),
            Test::__tests[module].end(),
            [&name] (Test *t) { return t->name() == name; }
        );

        if (it != Test::__tests[module].end()) {
            Test *t = (*it)->copy();
            t->_run();

            Message m;
            m << OpCode::FINISHED_TEST << _id << t->_status << t->_detailedReport;
            m.send(_driverSocket);
        }

        _inTest = false;
    }
    break;

    case OpCode::NOTIFY: {
        ++_notifyCount;
    }
    break;

    case OpCode::TERMINATE: {
        exit(0);
    }
    break;

    default: break;
    }
}

void WorkerContext::notify() {
    Test::_enter();

    Message m;
    m << OpCode::NOTIFY << _id;
    m.send(_driverSocket);

    Test::_exit();
}

void WorkerContext::wait(uint32_t n) {
    Test::_enter();

    if (n == -1u) n = 1;

    while (_notifyCount < n) {
        _waitForEvent();
    }

    _notifyCount -= n;

    Test::_exit();
}

Lazy<WorkerContext> WorkerContext::instance([] { return new WorkerContext(); });
