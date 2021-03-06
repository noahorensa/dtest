/*
 * Copyright (c) 2021 Noah Orensa.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#pragma once

#include <dtest_core/test.h>

using Status = dtest::Test::Status;

#define __dtest_concat(a,b) __dtest_concat2(a,b)    // force expand
#define __dtest_concat2(a,b) a ## b                 // actually concatenate

#define __dtest_instance(t) static auto __dtest_concat(__unit_test__uid_, __COUNTER__) = (*(t))

#define __test_1__(testType, name) __dtest_instance(new testType(name))
#define __test_2__(testType, module,name) __dtest_instance(new testType(module, name))
#define __test___(_1, _2, NAME, ...) NAME
#define __test__(testType, ...) __test___(__VA_ARGS__, __test_2__, __test_1__, UNUSED)(testType, __VA_ARGS__)

////

#define module(name) static auto __dtest_concat(__module_controller__uid_, __COUNTER__) = (*(new dtest::ModuleController(name)))

#define dtest_num_workers() dtest::Context::instance()->numWorkers()

#define dtest_worker_id() dtest::Context::instance()->workerId()

#define dtest_is_driver() dtest::Test::isDriver()

////

#include <dtest_core/unit_test.h>

#ifdef DTEST_DISABLE_ALL
#define unit(...) __test__(dtest::UnitTest, __VA_ARGS__).disable()
#else
#define unit(...) __test__(dtest::UnitTest, __VA_ARGS__)
#endif

////

#include <dtest_core/performance_test.h>

#ifdef DTEST_DISABLE_ALL
#define perf(...) __test__(dtest::PerformanceTest, __VA_ARGS__).disable()
#else
#define perf(...) __test__(dtest::PerformanceTest, __VA_ARGS__)
#endif

////

#include <dtest_core/distributed_unit_test.h>

#ifdef DTEST_DISABLE_ALL
#define dunit(...) __test__(dtest::DistributedUnitTest, __VA_ARGS__).disable()
#else
#define dunit(...) __test__(dtest::DistributedUnitTest, __VA_ARGS__)
#endif

#define dtest_notify() dtest::Context::instance()->notify()
#define dtest_wait(n) dtest::Context::instance()->wait(n)

#define dtest_send_msg(m) dtest::Context::instance()->sendUserMessage(dtest::Context::instance()->createUserMessage() << m)
#define dtest_recv_msg(m) dtest::Context::instance()->getUserMessage() >> m

////

#include <dtest_core/random.h>

#define dtest_random() dtest::frand()

////

#include <dtest_core/time_of.h>

#define dtest_timeOf(code) dtest::timeOf(code)
