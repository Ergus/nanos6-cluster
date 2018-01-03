/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
	
	Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
*/

#include <algorithm>
#include <cassert>
#include <set>
#include <vector>

#include <math.h>

#include <Atomic.hpp>
#include <Functors.hpp>
#include "TestAnyProtocolProducer.hpp"

#include <nanos6/debug.h>


using namespace Functors;


#define SUSTAIN_MICROSECONDS 200000L

//#define FINE_SELF_CHECK


TestAnyProtocolProducer tap;

static int nextTaskId = 0;

#if FINE_SELF_CHECK
static int numTests = 0;
#else
static int numTests = 1;
#endif

static unsigned int ncpus = 0;
static double delayMultiplier = 1.0;


struct TaskVerifier;
std::vector<TaskVerifier *> verifiers;


void shutdownTests()
{
}


struct TaskVerifier {
	typedef enum {
		READ,
		WRITE,
		CONCURRENT,
		REDUCTION,
		REDUCTION_OTHER
	} type_t;
	
	typedef enum {
		NOT_STARTED,
		STARTED,
		FINISHED
	} status_t;
	
	int _id;
	std::set<int> _runsAfter;
	std::set<int> _runsBefore;
	std::set<int> _runsConcurrentlyWith;
	status_t _status;
	type_t _type;
	int *_variable;
	Atomic<int> *_numConcurrentTasks;
	
private:
	TaskVerifier();
	
public:
	TaskVerifier(type_t type, int *variable, Atomic<int> *numConcurrentTasks = 0)
		: _id(nextTaskId++), _runsAfter(), _runsBefore(), _runsConcurrentlyWith(), _status(NOT_STARTED)
		, _type(type), _variable(variable), _numConcurrentTasks(numConcurrentTasks)
	{
	}
	
	
	char const *type2String() const
	{
		switch (_type) {
			case READ:
				return "READ";
			case WRITE:
				return "WRITE";
			case CONCURRENT:
				return "CONCURRENT";
			case REDUCTION:
				return "REDUCTION";
			case REDUCTION_OTHER:
				return "REDUCTION OTHER";
		}
		
		return "UNKNOWN";
	}
	
	void submit();
	
	void verify()
	{
		assert(_status == NOT_STARTED);
		tap.emitDiagnostic("Task ", _id, " (", type2String(), ") starts");
		_status = STARTED;
		
		for (std::set<int>::const_iterator it = _runsAfter.begin(); it != _runsAfter.end(); it++) {
			int predecessor = *it;
			
			TaskVerifier *predecessorTask = verifiers[predecessor];
			assert(predecessorTask != 0);
			{
				std::ostringstream oss;
				oss << "Task " << _id << " must run after task " << predecessorTask->_id;
				tap.evaluate(predecessorTask->_status == FINISHED, oss.str());
			}
		}
		
		if (!_runsConcurrentlyWith.empty()) {
			// FIXME can be extended to a full wait when taskyield is implemented
			int nwait = (ncpus < _runsConcurrentlyWith.size() + 1) ? ncpus : _runsConcurrentlyWith.size() + 1;
			
			assert(_numConcurrentTasks != 0);
			int var = ++(*_numConcurrentTasks);
			tap.emitDiagnostic("Task ", var, "/", nwait, ", running concurrently within its group, enters synchronization");
			
			std::ostringstream oss;
			oss << "Task " << _id << " can run concurrently with other tasks filling up the number of available CPUs";
			tap.timedEvaluate(
				GreaterOrEqual<Atomic<int>, int>(*_numConcurrentTasks, nwait),
				SUSTAIN_MICROSECONDS * delayMultiplier,
				oss.str()
			);
		}
		
		struct timespec delay = { 0, 1000000};
		nanosleep(&delay, &delay);
		
		for (std::set<int>::const_iterator it = _runsBefore.begin(); it != _runsBefore.end(); it++) {
			int successor = *it;
			
			TaskVerifier *successorTask = verifiers[successor];
			assert(successorTask != 0);
			{
				std::ostringstream oss;
				oss << "Task " << _id << " must run before task " << successorTask->_id;
				tap.evaluate(successorTask->_status == NOT_STARTED, oss.str());
			}
		}
		
		_status = FINISHED;
		tap.emitDiagnostic("Task ", _id, " (", type2String(), ") finishes");
	}
};


#pragma oss task in(*variable) label(R)
void verifyRead(int *variable, TaskVerifier *verifier)
{
	assert(verifier != 0);
	verifier->verify();
}


#pragma oss task out(*variable) label(W)
void verifyWrite(int *variable, TaskVerifier *verifier)
{
	assert(verifier != 0);
	verifier->verify();
}


#pragma oss task concurrent(*variable1) label(C)
void verifyConcurrent(int *variable1, TaskVerifier *verifier)
{
	assert(verifier != 0);
	verifier->verify();
}


void verifyReduction(int *variable1, TaskVerifier *verifier)
{
	assert(verifier != 0);
	verifier->verify();
}


void TaskVerifier::submit()
{
	switch (_type) {
		case READ:
			verifyRead(_variable, this);
			break;
		case WRITE:
			verifyWrite(_variable, this);
			break;
		case CONCURRENT:
			verifyConcurrent(_variable, this);
			break;
		case REDUCTION: {
			int& red_variable = *_variable;
			#pragma oss task reduction(+: red_variable) label(RED)
			verifyReduction(_variable, this);
			break;
		}
		case REDUCTION_OTHER: {
			int& red_variable = *_variable;
			#pragma oss task reduction(*: red_variable) label(RED_OTHER)
			verifyReduction(_variable, this);
			break;
		}
	}
}


struct VerifierConstraintCalculator {
	typedef enum {
		READERS,
		WRITER,
		CONCURRENT,
		REDUCTION
	} access_type_t;
	
	access_type_t _lastAccessType;
	
	std::set<int> _lastWriters;
	std::set<int> _lastReaders;
	std::set<int> _newWriters;
	
	VerifierConstraintCalculator()
		: _lastAccessType(READERS), _lastWriters(), _lastReaders(), _newWriters()
	{}
	
	// Fills out the _runsBefore and _runsConcurrentlyWith members of the verifier that is about to exit the current view of the status
	void flush()
	{
		if (_lastAccessType == READERS) {
			// There can only be writers before last access, unless it's the first access
			for (std::set<int>::const_iterator it = _lastWriters.begin(); it != _lastWriters.end(); it++) {
				int writer = *it;
				
				TaskVerifier *writerVerifier = verifiers[writer];
				assert(writerVerifier != 0);
				
				writerVerifier->_runsBefore = _lastReaders;
				// Increment number of tests, corresponding to tests run by selfcheck and verify
				numTests += _lastReaders.size();
#if FINE_SELF_CHECK
				numTests += _lastReaders.size();
#endif
				
				for (std::set<int>::const_iterator it2 = _lastWriters.begin(); it2 != _lastWriters.end(); it2++) {
					int other = *it2;
					
					if (other != writer)
						writerVerifier->_runsConcurrentlyWith.insert(other);
				}
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += writerVerifier->_runsConcurrentlyWith.size();
#endif
				numTests +=  writerVerifier->_runsConcurrentlyWith.empty() ? 0 : 1;
			}
			_lastWriters.clear();
		} else {
			assert(_lastAccessType == WRITER || _lastAccessType == CONCURRENT || _lastAccessType == REDUCTION);
			
			// Readers before last access
			for (std::set<int>::const_iterator it = _lastReaders.begin(); it != _lastReaders.end(); it++) {
				int reader = *it;
				
				TaskVerifier *readerVerifier = verifiers[reader];
				assert(readerVerifier != 0);
				
				readerVerifier->_runsBefore = _newWriters;
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += _newWriters.size();
#endif
				numTests += _newWriters.size();
				for (std::set<int>::const_iterator it2 = _lastReaders.begin(); it2 != _lastReaders.end(); it2++) {
					int other = *it2;
					
					if (other != reader)
						readerVerifier->_runsConcurrentlyWith.insert(other);
				}
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += readerVerifier->_runsConcurrentlyWith.size();
#endif
				numTests += readerVerifier->_runsConcurrentlyWith.empty() ? 0 : 1;
			}
			_lastReaders.clear();
			
			// Writer(s) before last access (either this or previous set will
			// be non-empty, but not both unless it's the first access)
			for (std::set<int>::const_iterator it = _lastWriters.begin(); it != _lastWriters.end(); it++) {
				int writer = *it;
				
				TaskVerifier *writerVerifier = verifiers[writer];
				assert(writerVerifier != 0);
				
				writerVerifier->_runsBefore = _newWriters;
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += _newWriters.size();
#endif
				numTests += _newWriters.size();
				
				for (std::set<int>::const_iterator it2 = _lastWriters.begin(); it2 != _lastWriters.end(); it2++) {
					int other = *it2;
					
					if (other != writer)
						writerVerifier->_runsConcurrentlyWith.insert(other);
				}
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += writerVerifier->_runsConcurrentlyWith.size();
#endif
				numTests += writerVerifier->_runsConcurrentlyWith.empty() ? 0 : 1;
			}
			_lastWriters = _newWriters;
			_newWriters.clear();
		}
	}
	
	// Fills out the _runsConcurrentlyWith member of the very last group of accesses
	void flushConcurrent()
	{
		if (_lastAccessType == READERS) {
			for (std::set<int>::const_iterator it = _lastReaders.begin(); it != _lastReaders.end(); it++) {
				int reader = *it;
				
				TaskVerifier *readerVerifier = verifiers[reader];
				assert(readerVerifier != 0);
				
				for (std::set<int>::const_iterator it2 = _lastReaders.begin(); it2 != _lastReaders.end(); it2++) {
					int other = *it2;
					
					if (other != reader)
						readerVerifier->_runsConcurrentlyWith.insert(other);
				}
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += readerVerifier->_runsConcurrentlyWith.size();
#endif
				numTests += readerVerifier->_runsConcurrentlyWith.empty() ? 0 : 1;
			}
		} else {
			assert(_lastAccessType == WRITER || _lastAccessType == CONCURRENT || _lastAccessType == REDUCTION);
			
			for (std::set<int>::const_iterator it = _lastWriters.begin(); it != _lastWriters.end(); it++) {
				int writer = *it;
				TaskVerifier *writerVerifier = verifiers[writer];
				assert(writerVerifier != 0);
				
				for (std::set<int>::const_iterator it2 = _lastWriters.begin(); it2 != _lastWriters.end(); it2++) {
					int other = *it2;
					if (other != writer)
						writerVerifier->_runsConcurrentlyWith.insert(other);
				}
				// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
				numTests += writerVerifier->_runsConcurrentlyWith.size();
#endif
				numTests +=  writerVerifier->_runsConcurrentlyWith.empty() ? 0 : 1;
			}
		}
	}
	
	void handleReader(TaskVerifier *verifier)
	{
		assert(verifier != 0);
		
		// First reader after writers
		if (_lastAccessType != READERS) {
			flush();
			_lastAccessType = READERS;
		}
		
		// There can only be writers before the reader, unless it's the first access
		if (!_lastWriters.empty()) {
			verifier->_runsAfter = _lastWriters;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastWriters.size();
#endif
			numTests += _lastWriters.size();
		}
		
		_lastReaders.insert(verifier->_id);
	}
	
	void handleWriter(TaskVerifier *verifier)
	{
		assert(verifier != 0);
		
		flush();
		
		// Writers before writer
		if (!_lastWriters.empty()) {
			verifier->_runsAfter = _lastWriters;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastWriters.size();
#endif
			numTests += _lastWriters.size();
		// Readers before writer (either this or previous condition will be
		// true, unless it's the first access)
		} else if (!_lastReaders.empty()) {
			verifier->_runsAfter = _lastReaders;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastReaders.size();
#endif
			numTests += _lastReaders.size();
		}
		
		_lastAccessType = WRITER;
		_newWriters.insert(verifier->_id);
	}
	
	void handleConcurrent(TaskVerifier *verifier)
	{
		assert(verifier != 0);
		
		// First concurrent
		if (_lastAccessType != CONCURRENT) {
			flush();
			_lastAccessType = CONCURRENT;
		}
		
		// Writer(s) before writers
		if (!_lastWriters.empty()) {
			verifier->_runsAfter = _lastWriters;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastWriters.size();
#endif
			numTests += _lastWriters.size();
		// Readers before writers (either this or previous condition will be
		// true, unless it's the first access)
		} else if (!_lastReaders.empty()) {
			verifier->_runsAfter = _lastReaders;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastReaders.size();
#endif
			numTests += _lastReaders.size();
		}
		
		_newWriters.insert(verifier->_id);
	}
	
	void handleReducer(TaskVerifier *verifier)
	{
		assert(verifier != 0);
		
		// First reduction
		if (_lastAccessType != REDUCTION) {
			flush();
			_lastAccessType = REDUCTION;
		}
		
		// Writer(s) before writers
		if (!_lastWriters.empty()) {
			verifier->_runsAfter = _lastWriters;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastWriters.size();
#endif
			numTests += _lastWriters.size();
		// Readers before writers (either this or previous condition will be
		// true, unless it's the first access)
		} else if (!_lastReaders.empty()) {
			verifier->_runsAfter = _lastReaders;
			// Increment number of tests, corresponding to tests run by selfcheck and verify
#if FINE_SELF_CHECK
			numTests += _lastReaders.size();
#endif
			numTests += _lastReaders.size();
		}
		
		_newWriters.insert(verifier->_id);
	}
	
	static void selfcheck()
	{
#if FINE_SELF_CHECK
#else
		bool globallyValid = true;
#endif
		for (std::vector<TaskVerifier *>::const_iterator vit = verifiers.begin(); vit != verifiers.end(); vit++) {
			TaskVerifier *verifier = *vit;
			assert(verifier != 0);
			
			for (std::set<int>::const_iterator it = verifier->_runsAfter.begin(); it != verifier->_runsAfter.end(); it++) {
				int predecessor = *it;
				
				TaskVerifier *predecessorVerifier = verifiers[predecessor];
				assert(predecessorVerifier != 0);
				
				{
#if FINE_SELF_CHECK
					std::ostringstream oss;
					oss << "Self verification: " << verifier->_id << " runs after " << predecessorVerifier->_id << " implies " << predecessorVerifier->_id << " runs before " << verifier->_id;
					tap.evaluate(predecessorVerifier->_runsBefore.find(verifier->_id) != predecessorVerifier->_runsBefore.end(), oss.str());
#else
					globallyValid = globallyValid && (predecessorVerifier->_runsBefore.find(verifier->_id) != predecessorVerifier->_runsBefore.end());
#endif
				}
			}
			
			for (std::set<int>::const_iterator it = verifier->_runsBefore.begin(); it != verifier->_runsBefore.end(); it++) {
				int successor = *it;
				
				TaskVerifier *successorVerifier = verifiers[successor];
				assert(successorVerifier != 0);
				
				{
#if FINE_SELF_CHECK
					std::ostringstream oss;
					oss << "Self verification: " << verifier->_id << " runs before " << successorVerifier->_id << " implies " << successorVerifier->_id << " runs after " << verifier->_id;
					tap.evaluate(successorVerifier->_runsAfter.find(verifier->_id) != successorVerifier->_runsAfter.end(), oss.str());
#else
					globallyValid = globallyValid && (successorVerifier->_runsAfter.find(verifier->_id) != successorVerifier->_runsAfter.end());
#endif
				}
			}
			
			for (std::set<int>::const_iterator it = verifier->_runsConcurrentlyWith.begin(); it != verifier->_runsConcurrentlyWith.end(); it++) {
				int concurrent = *it;
				
				TaskVerifier *concurrentVerifier = verifiers[concurrent];
				assert(concurrentVerifier != 0);
				
				{
#if FINE_SELF_CHECK
					std::ostringstream oss;
					oss << "Self verification: " << verifier->_id << " runs concurrently with " << concurrentVerifier->_id << " implies " <<
						concurrentVerifier->_id << " runs concurrently with " << verifier->_id;
					tap.evaluate(concurrentVerifier->_runsConcurrentlyWith.find(verifier->_id) != concurrentVerifier->_runsConcurrentlyWith.end(), oss.str());
#else
					globallyValid = globallyValid && (concurrentVerifier->_runsConcurrentlyWith.find(verifier->_id) != concurrentVerifier->_runsConcurrentlyWith.end());
#endif
				}
			}
		}
		
#if FINE_SELF_CHECK
#else
		tap.evaluate(globallyValid, "Self verification");
#endif
	}
	
};


static VerifierConstraintCalculator _constraintCalculator;


int main(int argc, char **argv)
{
	ncpus = nanos_get_num_cpus();
	
#if TEST_LESS_THREADS
	ncpus = std::min(ncpus, 64U);
#endif
	
	delayMultiplier = sqrt(ncpus);
	
	int var1;
	
	// 1 writer
	TaskVerifier firstWriter(TaskVerifier::WRITE, &var1); verifiers.push_back(&firstWriter); _constraintCalculator.handleWriter(&firstWriter);
	
	// NCPUS reducers
	Atomic<int> numReducers1(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *reducer = new TaskVerifier(TaskVerifier::REDUCTION, &var1, &numReducers1);
		verifiers.push_back(reducer);
		_constraintCalculator.handleReducer(reducer);
	}
	
	// NCPUS readers
	Atomic<int> numConcurrentReaders1(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *reader = new TaskVerifier(TaskVerifier::READ, &var1, &numConcurrentReaders1);
		verifiers.push_back(reader);
		_constraintCalculator.handleReader(reader);
	}
	
	// NCPUS reducers
	Atomic<int> numReducers2(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *reducer = new TaskVerifier(TaskVerifier::REDUCTION, &var1, &numReducers2);
		verifiers.push_back(reducer);
		_constraintCalculator.handleReducer(reducer);
	}

	// NCPUS concurrent
	Atomic<int> numConcurrents1(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *concurrent = new TaskVerifier(TaskVerifier::CONCURRENT, &var1, &numConcurrents1);
		verifiers.push_back(concurrent);
		_constraintCalculator.handleConcurrent(concurrent);
	}
	
	// NCPUS reducers
	Atomic<int> numReducers3(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *reducer = new TaskVerifier(TaskVerifier::REDUCTION, &var1, &numReducers3);
		verifiers.push_back(reducer);
		_constraintCalculator.handleReducer(reducer);
	}
	
	// NCPUS reducers (different operation)
	_constraintCalculator.flush();
	Atomic<int> numReducers4(0);
	for (long i=0; i < ncpus; i++) {
		TaskVerifier *reducer = new TaskVerifier(TaskVerifier::REDUCTION_OTHER, &var1, &numReducers4);
		verifiers.push_back(reducer);
		_constraintCalculator.handleReducer(reducer);
	}
	
	// Forced flush
	_constraintCalculator.flush();
	_constraintCalculator.flushConcurrent();
	
	tap.registerNewTests(numTests);
	tap.begin();
	
	_constraintCalculator.selfcheck();
	
	for (std::vector<TaskVerifier *>::const_iterator vit = verifiers.begin(); vit != verifiers.end(); vit++) {
		TaskVerifier *verifier = *vit;
		assert(verifier != 0);
		verifier->submit();
	}
	
	#pragma oss taskwait
	
	tap.end();
	
	return 0;
}
