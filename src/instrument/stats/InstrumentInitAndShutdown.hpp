/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_STATS_INIT_AND_SHUTDOWN_HPP
#define INSTRUMENT_STATS_INIT_AND_SHUTDOWN_HPP

#include "instrument/api/InstrumentInitAndShutdown.hpp"


namespace Instrument {
	void initialize();
	void shutdown();

	inline void nanos6_preinit_finished()
	{
	}
}


#endif // INSTRUMENT_STATS_INIT_AND_SHUTDOWN_HPP
