//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <global functions> :: flight_vm_debug_state
//=============================================================================
void flight_vm_debug_state(const flight_vm_data& vmdata, const char* state)
{
	// デバッグステート表示
	printf("%04d : ", vmdata.program_counter);
	for (int i = 0; i < vmdata.now_block_level; i++) { printf("  "); }
	printf("%s , ", state);
	printf("sl = %d , ", vmdata.now_stack_level);
	printf("sp = %d ", vmdata.stack_pointer);
	printf("\n");
}
