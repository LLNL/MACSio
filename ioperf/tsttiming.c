#include <unistd.h>
#include <time.h>

#include <timing.h>

void func2();

void func4()
{
    int nanoSecsOfSleeps = 200000;
    struct timespec ts = {0, nanoSecsOfSleeps};
    IOPTimerId_t tid = StartTimer();
    nanosleep(&ts, 0);
    StopTimer(tid, TEST_TIMERS, "func4");
}

void func3()
{
    IOPTimerId_t tid = StartTimer();
    func4();
    func2();
    StopTimer(tid, TEST_TIMERS, "func3");
}

void func2()
{
    IOPTimerId_t tid = StartTimer();
    sleep(2);
    StopTimer(tid, TEST_TIMERS, "func2");
}

void func1()
{
    IOPTimerId_t tid = StartTimer();
    IOPTimerId_t tid2;

    func2();
    sleep(1);
    tid2 = StartTimer();
    func3();
    StopTimer(tid, TEST_TIMERS, "call to func3 from func1");
    func2();

    StopTimer(tid, TEST_TIMERS, "func1");
}

int main()
{
    IOPTimerId_t a, b;

    a = StartTimer();

    func1();

    func4();

    b = StartTimer();
    func3();
    StopTimer(b, TEST_TIMERS, "call to func3 from main");

    StopTimer(a, TEST_TIMERS, "main");

    DumpTimings(0);
    
    return 0;
}
