#include "NaiveScheduler.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "executors/threads/ThreadManager.hpp"
#include "executors/threads/CPUManager.hpp"
#include "hardware/places/CPUPlace.hpp"
#include "tasks/Task.hpp"

#include <algorithm>
#include <cassert>
#include <mutex>

NaiveScheduler::NaiveScheduler()
{
}

NaiveScheduler::~NaiveScheduler()
{
}


Task *NaiveScheduler::getReplacementTask(__attribute__((unused)) CPU *computePlace)
{
	if (!_unblockedTasks.empty()) {
		Task *replacementTask = _unblockedTasks.front();
		_unblockedTasks.pop_front();
		
		assert(replacementTask != nullptr);
		
		return replacementTask;
	} else {
		return nullptr;
	}
}


ComputePlace * NaiveScheduler::addReadyTask(Task *task, __attribute__((unused)) ComputePlace *computePlace, __attribute__((unused)) ReadyTaskHint hint, bool doGetIdle)
{
	std::lock_guard<SpinLock> guard(_globalLock);
	_readyTasks.push_front(task);
	
	if (doGetIdle) {
		return CPUManager::getIdleCPU();
	} else {
		return nullptr;
	}
}


void NaiveScheduler::taskGetsUnblocked(Task *unblockedTask, __attribute__((unused)) ComputePlace *computePlace)
{
	std::lock_guard<SpinLock> guard(_globalLock);
	_unblockedTasks.push_front(unblockedTask);
}


Task *NaiveScheduler::getReadyTask(__attribute__((unused)) ComputePlace *computePlace, __attribute__((unused)) Task *currentTask, bool canMarkAsIdle)
{
	Task *task = nullptr;
	
	std::lock_guard<SpinLock> guard(_globalLock);
	
	// 1. Get an unblocked task
	task = getReplacementTask((CPU *) computePlace);
	if (task != nullptr) {
		return task;
	}
	
	// 2. Or get a ready task
	if (!_readyTasks.empty()) {
		task = _readyTasks.front();
		_readyTasks.pop_front();
		
		assert(task != nullptr);
		
		return task;
	}
	
	// 3. Or mark the CPU as idle
	if (canMarkAsIdle) {
		CPUManager::cpuBecomesIdle((CPU *) computePlace);
	}
	
	return nullptr;
}


ComputePlace *NaiveScheduler::getIdleComputePlace(bool force)
{
	std::lock_guard<SpinLock> guard(_globalLock);
	if (force || !_readyTasks.empty() || !_unblockedTasks.empty()) {
		return CPUManager::getIdleCPU();
	} else {
		return nullptr;
	}
}


std::string NaiveScheduler::getName() const
{
	return "naive";
}
