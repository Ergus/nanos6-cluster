#include "SchedulerInterface.hpp"

#include <cassert>

#define _unused(x) ((void)(x))

void SchedulerInterface::disableComputePlace(__attribute__((unused)) ComputePlace *hardwarePlace)
{
}


void SchedulerInterface::enableComputePlace(__attribute__((unused)) ComputePlace *hardwarePlace)
{
}


bool SchedulerInterface::requestPolling(ComputePlace *hardwarePlace, polling_slot_t *pollingSlot)
{
	assert(pollingSlot != nullptr);
	assert(pollingSlot->_task == nullptr);
	
	// Default implementation: attempt to get a ready task and fail if not possible
	Task *task = getReadyTask(hardwarePlace);
	
	if (task != nullptr) {
		Task *expected = nullptr;
		
		pollingSlot->_task.compare_exchange_strong(expected, task);
		assert(expected == nullptr);
		
		return true;
	} else {
		return false;
	}
}


bool SchedulerInterface::releasePolling(__attribute__((unused)) ComputePlace *hardwarePlace, __attribute__((unused)) polling_slot_t *pollingSlot)
{
	// The default implementation should never be called if there is a default implementation of requestPolling
	// otherwise there should be an implementation of this method that matches requestPolling
	assert(false);
	return true;
}

void SchedulerInterface::createReadyQueues(std::size_t nodes)
{
    _unused(nodes);
}
