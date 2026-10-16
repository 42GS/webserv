#include "core/EventLoop.hpp"
#include "unit/EventLoopDoubles.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

TEST_CASE(loopDeliversReadyEvents) {
  ReadyPipe ready;
  RecordingHandler handler(ready.readEnd());
  Clock clock(test_paths::kClockFile);
  EventLoop loop(clock);
  loop.watch(&handler);
  StopAfterRounds observer(2);
  loop.run(observer);
  CHECK_EQ(2, handler.events);
  CHECK_EQ(0, handler.failures);
}

TEST_CASE(unwatchDuringDispatchSkipsTheRestOfTheRound) {
  ReadyPipe first;
  ReadyPipe second;
  RecordingHandler closer(first.readEnd());
  RecordingHandler closed(second.readEnd());
  Clock clock(test_paths::kClockFile);
  EventLoop loop(clock);
  loop.watch(&closer);
  loop.watch(&closed);
  closer.registry = &loop;
  closer.toUnwatch = &closed;
  StopAfterRounds observer(2);
  loop.run(observer);
  CHECK_EQ(2, closer.events);
  CHECK_EQ(0, closed.events);
}

TEST_CASE(exceptionFromHandlerCallsOnFailureOnly) {
  ReadyPipe first;
  ReadyPipe second;
  RecordingHandler failing(first.readEnd());
  RecordingHandler healthy(second.readEnd());
  failing.throwOnEvent = true;
  Clock clock(test_paths::kClockFile);
  EventLoop loop(clock);
  loop.watch(&failing);
  loop.watch(&healthy);
  StopAfterRounds observer(1);
  loop.run(observer);
  CHECK_EQ(1, failing.failures);
  CHECK_EQ(1, healthy.events);
  CHECK_EQ(0, healthy.failures);
}
