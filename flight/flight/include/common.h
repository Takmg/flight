#ifndef __COMMON_H__
#define __COMMON_H__


//=============================================================================
// include files [C++ libraries]
//=============================================================================
#include <functional>	// std::function
#include <map>			// std::map
#include <memory>		// std::shared_ptr
#include <string>		// std::string


//=============================================================================
// pre-process
//=============================================================================
#ifdef FLIGHT_EXPORTS
#	define DLL_EXPORT __declspec(dllexport)
#else
#	define DLL_EXPORT __declspec(dllimport)
#endif


#endif //__COMMON_H__