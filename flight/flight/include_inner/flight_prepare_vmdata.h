#pragma once


//=============================================================================
// <global functions> :: flight_prepare_vmdata
//=============================================================================
bool flight_prepare_instructions(flight_vm_data&, const error_setter&);
void flight_prepare_vmdata(flight_vm_data&, const error_setter&);
void flight_prepare_embedded_function(flight* instance, flight_vm_data& vmdata);
