#include "resolve.h"


void nanos_register_weak_read_depinfo(void *handler, void *start, size_t length)
{
	typedef void nanos_register_weak_read_depinfo_t(void *handler, void *start, size_t length);
	
	static nanos_register_weak_read_depinfo_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0)) {
		symbol = (nanos_register_weak_read_depinfo_t *) _nanos6_resolve_symbol("nanos_register_weak_read_depinfo", "weak dependency", "nanos_register_read_depinfo");
	}
	
	(*symbol)(handler, start, length);
}


void nanos_register_weak_write_depinfo(void *handler, void *start, size_t length)
{
	typedef void nanos_register_weak_write_depinfo_t(void *handler, void *start, size_t length);
	
	static nanos_register_weak_write_depinfo_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0)) {
		symbol = (nanos_register_weak_write_depinfo_t *) _nanos6_resolve_symbol("nanos_register_weak_write_depinfo", "weak dependency", "nanos_register_write_depinfo");
	}
	
	(*symbol)(handler, start, length);
}


void nanos_register_weak_readwrite_depinfo(void *handler, void *start, size_t length)
{
	typedef void nanos_register_weak_readwrite_depinfo_t(void *handler, void *start, size_t length);
	
	static nanos_register_weak_readwrite_depinfo_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0)) {
		symbol = (nanos_register_weak_readwrite_depinfo_t *) _nanos6_resolve_symbol("nanos_register_weak_readwrite_depinfo", "weak dependency", "nanos_register_readwrite_depinfo");
	}
	
	(*symbol)(handler, start, length);
}

