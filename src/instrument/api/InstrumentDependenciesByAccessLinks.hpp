#ifndef INSTRUMENT_DEPENDENCIES_BY_ACCESS_LINK_HPP
#define INSTRUMENT_DEPENDENCIES_BY_ACCESS_LINK_HPP


#include <InstrumentDataAccessId.hpp>
#include <InstrumentTaskId.hpp>

#include <DataAccessRange.hpp>

#include "dependencies/DataAccessType.hpp"


namespace Instrument {
	//! \file
	//! \name Dependency instrumentation by access links
	//! @{
	//! This group of functions is useful for instrumenting task dependencies. It passes the actual information that the
	//! runtime manages so that the instrumentation can closely represent its behaviour.
	//! 
	//! The main concepts are the sequence of data accesses and the data access.
	//!
	//! A data access is an access of a task to a storage that can produce dependencies. A sequence of those is a list
	//! of data accesses to the same storage ordered by creation time.
	//! 
	//! As tasks are executed, they make the data accesses of other tasks satisfied, that is, they liberate a dependency
	//! between the task that finishes and the task that originates the satisfied data access. Therefore, when using this
	//! interface, dependencies are determined at the time that they are satisfied.
	//! 
	//! Since this interface depends on the information that the runtime stores, it cannot detect reliably situations in
	//! which there are not enough data accesses in a given sequence.
	//! 
	//! In addition the interface for instrumenting taskwaits can help to differentiate between cases in which a sequence
	//! is depleted due to a taskwait from the cases in which they deplete due to the conditions of the execution
	//! environment (for instance the number of threads).
	//! 
	//! Sequences of data accesses are identified by their superaccess. For the outermost accesses, the identifier of
	//! their superaccess is the default value returned by calling the constructor if the type without parameters.
	
	//! \brief Called when a new DataAccess is created
	//! 
	//! \param superAccessId the identifier of the superaccess that contains the new DataAccess as returned by Instrument::createdDataAccess or data_access_id_t() if there is no superaccess
	//! \param accessType the type of access of the new DataAccess
	//! \param weak true if the access is weak
	//! \param range the range of data that the new access covers
	//! \param readSatisfied whether the access is ready to perform a potential read operation
	//! \param writeSatisfied whether the access is ready to perform a potential write operation
	//! \param globallySatisfied whether the access does not preclude the task from running immediately
	//! \param originatorTaskId the identifier of the task that will perform the access as returned in the call to Instrument::enterAddTask
	//! 
	//! \returns an identifier for the new data access
	data_access_id_t createdDataAccess(
		data_access_id_t superAccessId,
		DataAccessType accessType, bool weak, DataAccessRange range,
		bool readSatisfied, bool writeSatisfied, bool globallySatisfied,
		task_id_t originatorTaskId
	);
	
	//! \brief Called when a DataAccess has its type of access upgraded
	//! 
	//! Note that this function may be called with previousAccessType == newAccessType in case of a repeated access
	//! 
	//! \param dataAccessId the identifier of the DataAccess returned in the previous call to Instrument::createdDataAccess
	//! \param previousAccessType the type of access that will be upgraded
	//! \param previousWeakness true if the access to be upgraded is weak
	//! \param newAccessType the type of access to which it will be upgraded
	//! \param newWeakness true if the resulting access is weak
	//! \param becomesUnsatisfied indicates if the DataAccess was satisfied and has become unsatisfied as a result of the upgrade
	//! \param triggererTaskId the identifier of the task that triggers the change
	//! 
	void upgradedDataAccess(
		data_access_id_t dataAccessId,
		DataAccessType previousAccessType, bool previousWeakness,
		DataAccessType newAccessType, bool newWeakness,
		bool becomesUnsatisfied,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess becomes satisfied
	//! 
	//! \param dataAccessId the identifier of the DataAccess that becomes satisfied as returned in the previous call to Instrument::createdDataAccess
	//! \param readSatisfied whether the access becomes read satisfied
	//! \param writeSatisfied whether the access begomes write satisfied
	//! \param globallySatisfied whether the access becomes globally satisfied
	//! \param triggererTaskId the identifier of the task that triggers the change
	//! \param targetTaskId the identifier of the task that will perform the now satisfied DataAccess
	void dataAccessBecomesSatisfied(
		data_access_id_t dataAccessId,
		bool readSatisfied, bool writeSatisfied, bool globallySatisfied,
		task_id_t triggererTaskId,
		task_id_t targetTaskId
	);
	
	//! \brief Called when a DataAccess has its range modified
	//! 
	//! \param dataAccessId the identifier of the affected DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param newRange the range of data that the access covers now
	//! \param triggererTaskId the identifier of the task that triggers the change
	void modifiedDataAccessRange(
		data_access_id_t dataAccessId,
		DataAccessRange newRange,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess gets fragmented
	//! 
	//! \param dataAccessId the identifier of the affected DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param newRange the range of data of the new fragment
	//! \param triggererTaskId the identifier of the task that triggers the change
	//! 
	//! The original data access and any newly created fragments will have the modifiedDataAccessRange method called
	//! 
	//! \returns an identifier for the new data access
	data_access_id_t fragmentedDataAccess(
		data_access_id_t dataAccessId,
		DataAccessRange newRange,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess has its subaccess fragment created
	//! 
	//! \param dataAccessId the identifier of the affected DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param triggererTaskId the identifier of the task that triggers the change
	//! 
	//! \returns an identifier for the subaccess fragment
	data_access_id_t createdDataSubaccessFragment(
		data_access_id_t dataAccessId,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess has has been completed
	//! 
	//! \param dataAccessId the identifier of the affected DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param triggererTaskId the identifier of the task that triggers the change
	void completedDataAccess(
		data_access_id_t dataAccessId,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess becomes removable
	//! 
	//! \param dataAccessId the identifier of the DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param triggererTaskId the identifier of the task that triggers the change
	void dataAccessBecomesRemovable(
		data_access_id_t dataAccessId,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess has been removed
	//! 
	//! \param dataAccessId the identifier of the DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param triggererTaskId the identifier of the task that triggers the change
	void removedDataAccess(
		data_access_id_t dataAccessId,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when two DataAccess objects are linked
	//! 
	//! \param sourceAccessId the identifier of the source DataAccess
	//! \param sinkTaskId the identifier of the sink Task
	//! \param range the range of data covered by the link
	//! \param direct true if it is a direct link, false if it is an indirect effective previous relation
	//! \param bidirectional tru if the link is bidirectional
	//! \param triggererTaskId the identifier of the task that triggers the change
	void linkedDataAccesses(
		data_access_id_t sourceAccessId, task_id_t sinkTaskId,
		DataAccessRange range,
		bool direct, bool bidirectional,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when two DataAccess objects are unlinked
	//! 
	//! \param sourceAccessId the identifier of the source DataAccess
	//! \param sinkTaskId the identifier of the sink Task
	//! \param direct true if it is a direct link, false if it is an indirect effective previous relation
	//! \param triggererTaskId the identifier of the task that triggers the change
	void unlinkedDataAccesses(
		data_access_id_t sourceAccessId, task_id_t sinkTaskId, bool direct,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess has is moved from one superaccess to another
	//! 
	//! \param oldSuperAccessId the identifier of the superaccess from which the DataAccess is removed
	//! \param newSuperAccessId the identifier of the superaccess to which the DataAccess is inserted
	//! \param dataAccessId the identifier of the DataAccess that is moved
	//! \param triggererTaskId the identifier of the task that triggers the change
	//! 
	void reparentedDataAccess(
		data_access_id_t oldSuperAccessId, data_access_id_t newSuperAccessId,
		data_access_id_t dataAccessId,
		task_id_t triggererTaskId
	);
	
	//! \brief Called when a DataAccess has a new property
	//! 
	//! \param dataAccessId the identifier of the DataAccess as returned in the previous call to Instrument::createdDataAccess
	//! \param shortPropertyName a short name to give to the property (1 to 3 characters)
	//! \param longPropertyName a name for the property with unconstrained length
	//! \param triggererTaskId the identifier of the task that triggers the change
	void newDataAccessProperty(
		data_access_id_t dataAccessId,
		char const *shortPropertyName,
		char const *longPropertyName,
		task_id_t triggererTaskId
	);
	//! @}
}


#endif // INSTRUMENT_DEPENDENCIES_BY_ACCESS_LINK_HPP