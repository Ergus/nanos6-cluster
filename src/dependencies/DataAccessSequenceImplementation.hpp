#ifndef DATA_ACCESS_SEQUENCE_IMPLEMENTATION_HPP
#define DATA_ACCESS_SEQUENCE_IMPLEMENTATION_HPP

#include <cassert>
#include <mutex>

#include "DataAccessSequence.hpp"
#include "tasks/Task.hpp"

#include <InstrumentDependenciesByAccessSequences.hpp>
#include <InstrumentDependenciesByGroup.hpp>


DataAccessSequence::DataAccessSequence()
	: _accessRange(),
	_lock(), _accessSequence(), _superAccess(0),
	_instrumentationId(Instrument::registerAccessSequence(Instrument::data_access_id_t(), Instrument::task_id_t()))
{
}


DataAccessSequence::DataAccessSequence(DataAccessRange accessRange, DataAccess *superAccess)
	: _accessRange(accessRange),
	_lock(), _accessSequence(), _superAccess(superAccess),
	_instrumentationId(Instrument::registerAccessSequence(Instrument::data_access_id_t(), Instrument::task_id_t()))
{
}


bool DataAccessSequence::reevaluateSatisfactibility(DataAccessSequence::access_sequence_t::iterator position)
{
	DataAccess &dataAccess = *position;
	
	if (dataAccess._satisfied) {
		// Already satisfied
		return false;
	}
	
	if (position == _accessSequence.begin()) {
		// The first position is satisfied, otherwise the parent task code is incorrect
		dataAccess._satisfied = true;
		return true;
	}
	
	if (dataAccess._type == WRITE_ACCESS_TYPE) {
		// A write access with accesses before it
		return false;
	}
	
	--position;
	DataAccess const &previousAccess = *position;
	if (!previousAccess._satisfied) {
		// If the preceeding access is not satisfied, this cannot be either
		return false;
	}
	
	assert(dataAccess._type == READ_ACCESS_TYPE);
	assert(previousAccess._satisfied);
	if (previousAccess._type == READ_ACCESS_TYPE) {
		// Consecutive reads are satisfied together
		dataAccess._satisfied = true;
		return true;
	} else {
		assert(previousAccess._type == WRITE_ACCESS_TYPE);
		// Read after Write
		return false;
	}
}


bool DataAccessSequence::addTaskAccess(Task *task, DataAccessType accessType, DataAccess *&dataAccess)
{
	assert(task != 0);
	std::lock_guard<SpinLock> guard(_lock);
	
	auto it = _accessSequence.rbegin();
	
	// If there are no previous accesses, then the new access can be satisfied
	bool satisfied = (it == _accessSequence.rend());
	
	if (it != _accessSequence.rend()) {
		// There is a "last" access
		DataAccess &lastAccess = *it;
		
		if (lastAccess._originator == task) {
			// The task "accesses" twice to the same location
			
			dataAccess = 0;
			if (lastAccess._type == accessType) {
				// An identical access
				return true; // Do not count this one
			} else if ((accessType == WRITE_ACCESS_TYPE) && (lastAccess._type == READWRITE_ACCESS_TYPE)) {
				return true; // The previous access subsumes this
			} else if ((accessType == READWRITE_ACCESS_TYPE) && (lastAccess._type == WRITE_ACCESS_TYPE)) {
				// An almost identical access
				Instrument::upgradedDataAccessInSequence(_instrumentationId, lastAccess._instrumentationId, lastAccess._type, accessType, false, task->getInstrumentationTaskId());
				lastAccess._type = accessType;
				
				return true; // Do not count this one
			} else if (lastAccess._type == READ_ACCESS_TYPE) {
				// Upgrade a read into a write or readwrite
				assert((accessType == WRITE_ACCESS_TYPE) || (accessType == READWRITE_ACCESS_TYPE));
				
				Instrument::removeTaskFromAccessGroup(this, task->getInstrumentationTaskId());
				Instrument::beginAccessGroup(task->getParent()->getInstrumentationTaskId(), this, false);
				Instrument::addTaskToAccessGroup(this, task->getInstrumentationTaskId());
				
				if (lastAccess._satisfied) {
					// Calculate if the upgraded access is satisfied
					--it;
					satisfied = (it == _accessSequence.rend());
					lastAccess._satisfied = satisfied;
					
					Instrument::upgradedDataAccessInSequence(_instrumentationId, lastAccess._instrumentationId, lastAccess._type, accessType, !satisfied, task->getInstrumentationTaskId());
					
					// Upgrade the access type
					lastAccess._type = accessType;
					
					return satisfied; // A new chance for the access to not be satisfied
				} else {
					Instrument::upgradedDataAccessInSequence(_instrumentationId, lastAccess._instrumentationId, lastAccess._type, accessType, false, task->getInstrumentationTaskId());
					
					// Upgrade the access type
					lastAccess._type = accessType;
					
					return true; // The predecessor has already been counted
				}
			} else {
				assert((lastAccess._type == WRITE_ACCESS_TYPE) || (lastAccess._type == READWRITE_ACCESS_TYPE));
				
				// The previous access was as restrictive as possible
				return true; // Satisfactibility has already been accounted for
			}
		} else {
			if ((lastAccess._type == WRITE_ACCESS_TYPE) || (lastAccess._type == READWRITE_ACCESS_TYPE)) {
				satisfied = false;
			} else {
				satisfied = (lastAccess._type == accessType) && lastAccess._satisfied;
			}
		}
	} else {
		// We no longer have (or never had) information about any previous access to this storage
		Instrument::beginAccessGroup(task->getParent()->getInstrumentationTaskId(), this, true);
	}
	
	if (_accessSequence.empty()) {
		_instrumentationId = Instrument::registerAccessSequence((_superAccess != 0 ? _superAccess->_instrumentationId : Instrument::data_access_id_t()), task->getInstrumentationTaskId());
		if (_superAccess != 0) {
			// The access of the parent will start having subaccesses
			
			// 1. The parent is adding this task, so it cannot have finished (>=1)
			// 2. The sequence is empty, so it has not been counted yet (<2)
			assert(_superAccess->_completionCountdown.load() == 1);
			
			_superAccess->_completionCountdown++;
		}
	}
	
	Instrument::data_access_id_t dataAccessInstrumentationId =
		Instrument::addedDataAccessInSequence(_instrumentationId, accessType, satisfied, task->getInstrumentationTaskId());
	Instrument::addTaskToAccessGroup(this, task->getInstrumentationTaskId());
	
	dataAccess = new DataAccess(this, accessType, satisfied, task, _accessRange, dataAccessInstrumentationId);
	_accessSequence.push_back(*dataAccess); // NOTE: It actually does get the pointer
	
	return satisfied;
}


#endif // DATA_ACCESS_SEQUENCE_IMPLEMENTATION_HPP