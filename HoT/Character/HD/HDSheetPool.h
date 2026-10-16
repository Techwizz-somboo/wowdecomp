#ifndef HOT_CHARACTER_HD_HDSHEETPOOL_H
#define HOT_CHARACTER_HD_HDSHEETPOOL_H

#include <atomic>
#include <functional>

// Below normal priority helper threads for the HD sheet compositor, created on first use and never stopped.
namespace HoT::HD::Pool {

struct Group {
    std::atomic<int> m_remaining { 0 };
};

void Submit(std::function<void()> task, Group* group, bool front);

// Runs queued tasks of the group on the calling thread while it waits.
void Wait(Group& group);

int ThreadCount();

}

#endif
