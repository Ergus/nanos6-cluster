#include "NaiveScheduler.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "executors/threads/TaskFinalization.hpp"
#include "executors/threads/ThreadManager.hpp"
#include "executors/threads/CPUManager.hpp"
#include "hardware/places/CPUPlace.hpp"
#include "tasks/Task.hpp"
#include "tasks/TaskloopManagerImplementation.hpp"

#include <algorithm>
#include <cassert>
#include <mutex>

NaiveScheduler::NaiveScheduler(__attribute__((unused)) int numaNodeIndex)
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
	bool workAssigned = false;
	std::vector<Taskloop *> completeTaskloops;
	
	{
		std::lock_guard<SpinLock> guard(_globalLock);
		
		// Try to get an unblocked task
		task = getReplacementTask((CPU *) computePlace);
		if (task != nullptr) {
			return task;
		}
		
		while (!_readyTasks.empty() && !workAssigned) {
			// Get the first ready task
			task = _readyTasks.front();
			assert(task != nullptr);
			
			if (!task->isTaskloop()) {
				_readyTasks.pop_front();
				workAssigned = true;
				break;
			}
			
			Taskloop *taskloop = (Taskloop *)task;
			workAssigned = taskloop->needMoreExecutors();
			if (!workAssigned) {
				_readyTasks.pop_front();
				completeTaskloops.push_back(taskloop);
			}
		}
	}
	
	bool shouldRecheckUnblockedTasks = false;
	for (Taskloop *completeTaskloop : completeTaskloops) {
		// Check if the taskloop can disposed
		bool disposable = completeTaskloop->markAsFinished();
		if (disposable) {
			TaskFinalization::disposeOrUnblockTask(completeTaskloop, computePlace);
			shouldRecheckUnblockedTasks = true;
		}
	}
	
	if (workAssigned) {
		assert(task != nullptr);
		
		if (task->isTaskloop()) {
			return TaskloopManager::createRunnableTaskloop((Taskloop *)task);
		}
		
		return task;
	}
	
	if (shouldRecheckUnblockedTasks) {
		return Scheduler::getReadyTask(computePlace, currentTask);
	}
	
	if (canMarkAsIdle) {
		CPUManager::cpuBecomesIdle((CPU *)computePlace);
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
