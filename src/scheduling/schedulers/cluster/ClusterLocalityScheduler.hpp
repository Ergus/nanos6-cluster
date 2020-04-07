/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#ifndef CLUSTER_LOCALITY_SCHEDULER_HPP
#define CLUSTER_LOCALITY_SCHEDULER_HPP

#include "scheduling/schedulers/cluster/ClusterSchedulerInterface.hpp"
#include "system/RuntimeInfo.hpp"

#include <ClusterManager.hpp>

class ClusterLocalityScheduler : public ClusterSchedulerInterface {

public:
	ClusterLocalityScheduler() : ClusterSchedulerInterface("ClusterLocalityScheduler")
	{
	}

	~ClusterLocalityScheduler()
	{
	}

	void addReadyTask(Task *task, ComputePlace *computePlace, ReadyTaskHint hint = NO_HINT);
};

#endif // CLUSTER_LOCALITY_SCHEDULER_HPP
