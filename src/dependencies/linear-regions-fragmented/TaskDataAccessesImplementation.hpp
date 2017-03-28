#ifndef TASK_DATA_ACCESSES_IMPLEMENTATION_HPP
#define TASK_DATA_ACCESSES_IMPLEMENTATION_HPP

#include <boost/intrusive/parent_from_member.hpp>

#include <InstrumentDependenciesByAccessLinks.hpp>

#include "BottomMapEntry.hpp"
#include "DataAccess.hpp"
#include "TaskDataAccesses.hpp"
#include "tasks/Task.hpp"


inline TaskDataAccesses::~TaskDataAccesses()
{
	assert(!hasBeenDeleted());
	
#ifndef NDEBUG
	Task *task = boost::intrusive::get_parent_from_member<Task>(this, &Task::_dataAccesses);
	assert(task != nullptr);
	assert(&task->getDataAccesses() == this);
#endif
	
	// We take the lock since the task may be marked for deletion while the lock is held
	std::lock_guard<spinlock_t> guard(_lock);
	_accesses.deleteAll(
		[&](DataAccess *access) {
			Instrument::removedDataAccess(access->_instrumentationId);
			delete access;
		}
	);
	
	_subaccessBottomMap.deleteAll(
		[&](BottomMapEntry *bottomMapEntry) {
			delete bottomMapEntry;
		}
	);
	
	_accessFragments.deleteAll(
		[&](DataAccess *fragment) {
			Instrument::removedDataAccess(fragment->_instrumentationId);
			delete fragment;
		}
	);
	
#ifndef NDEBUG
	hasBeenDeleted() = true;
#endif
	
}


// inline void TaskDataAccesses::addRemovableChild(Task *child)
// {
// 	assert(child != nullptr);
// 	
// 	Task *lastRemovableChild = _removableChildren;
// 	do {
// 		child->_dataAccesses._nextRemovableSibling = lastRemovableChild;
// 	} while (!_removableChildren.compare_exchange_strong(lastRemovableChild, child));
// }


#endif // TASK_DATA_ACCESSES_IMPLEMENTATION_HPP
