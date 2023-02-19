/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */
#include "heaptimer.h"

void HeapTimer::siftup(size_t i)
{
    assert(i >= 0 && i < mHeap.size());
    size_t j = (i - 1) / 2;
    while (j >= 0)
    {
        if (mHeap[j] < mHeap[i])
        {
            break;
        }
        SwapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode(size_t i, size_t j)
{
    assert(i >= 0 && i < mHeap.size());
    assert(j >= 0 && j < mHeap.size());
    std::swap(mHeap[i], mHeap[j]);
    mRef[mHeap[i].id] = i;
    mRef[mHeap[j].id] = j;
}

bool HeapTimer::siftdown(size_t index, size_t n)
{
    assert(index >= 0 && index < mHeap.size());
    assert(n >= 0 && n <= mHeap.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n)
    {
        if (j + 1 < n && mHeap[j + 1] < mHeap[j])
            j++;
        if (mHeap[i] < mHeap[j])
            break;
        SwapNode(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack &cb)
{
    assert(id >= 0);
    size_t i;
    if (mRef.count(id) == 0)
    {
        /* 新节点：堆尾插入，调整堆 */
        i = mHeap.size();
        mRef[id] = i;
        mHeap.push_back({id, Clock::now() + MS(timeout), cb});
        siftup(i);
    }
    else
    {
        /* 已有结点：调整堆 */
        i = mRef[id];
        mHeap[i].expires = Clock::now() + MS(timeout);
        mHeap[i].cb = cb;
        if (!siftdown(i, mHeap.size()))
        {
            siftup(i);
        }
    }
}

void HeapTimer::doWork(int id)
{
    /* 删除指定id结点，并触发回调函数 */
    if (mHeap.empty() || mRef.count(id) == 0)
    {
        return;
    }
    size_t i = mRef[id];
    TimerNode node = mHeap[i];
    node.cb();
    remove(i);
}

void HeapTimer::remove(size_t index)
{
    /* 删除指定位置的结点 */
    assert(!mHeap.empty() && index >= 0 && index < mHeap.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = mHeap.size() - 1;
    assert(i <= n);
    if (i < n)
    {
        SwapNode(i, n);
        if (!siftdown(i, n))
        {
            siftup(i);
        }
    }
    /* 队尾元素删除 */
    mRef.erase(mHeap.back().id);
    mHeap.pop_back();
}

void HeapTimer::adjust(int id, int timeout)
{
    /* 调整指定id的结点 */
    assert(!mHeap.empty() && mRef.count(id) > 0);
    mHeap[mRef[id]].expires = Clock::now() + MS(timeout);
    ;
    siftdown(mRef[id], mHeap.size());
}

void HeapTimer::tick()
{
    /* 清除超时结点 */
    if (mHeap.empty())
    {
        return;
    }
    while (!mHeap.empty())
    {
        TimerNode node = mHeap.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop()
{
    assert(!mHeap.empty());
    remove(0);
}

void HeapTimer::clear()
{
    mRef.clear();
    mHeap.clear();
}

int HeapTimer::GetNextTick()
{
    tick();
    size_t res = -1;
    if (!mHeap.empty())
    {
        res = std::chrono::duration_cast<MS>(mHeap.front().expires - Clock::now()).count();
        if (res < 0)
        {
            res = 0;
        }
    }
    return res;
}