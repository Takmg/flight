//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <global variable> :: _token_reserved_instruction_list [�\���ꗗ(����)]
//=============================================================================
const flight_reserved_data _token_reserved_instruction_list[] = {
	{ "func"	, flight_parser_func },
	{ "while"	, flight_parser_while },
	{ "for"		, flight_parser_for },
	{ "if"		, flight_parser_if  },
	{ "else"	, flight_parser_error },
	{ "var"		, flight_parser_var },
	{ "continue", flight_parser_simple },
	{ "break"   , flight_parser_simple },
	{ "return"  , flight_parser_ret },
	{ "yield"   , flight_parser_ret },

	// �������牺�̓\�[�X����g���Ȃ�����
	{ "__RESET_ACCUM__!" , flight_parser_simple },	// �A�L�������[�^�̃��Z�b�g
};


//=============================================================================
// <global variable> :: _token_reserved_value_list [�\���ꗗ(�E�Ӓl)]
//=============================================================================
const flight_reserved_data _token_reserved_value_list[] = {
	{ "true" },
	{ "false" },
	{ "null" },
};


//=============================================================================
// <global variable> :: _token_reserved_instruction_list_size �\���T�C�Y
//=============================================================================
const int _token_reserved_instruction_list_size = sizeof(_token_reserved_instruction_list) / sizeof(_token_reserved_instruction_list[0]);


//=============================================================================
// <global variable> :: _token_reserved_value_list_size �\���T�C�Y
//=============================================================================
const int _token_reserved_value_list_size = sizeof(_token_reserved_value_list) / sizeof(_token_reserved_value_list[0]);


//=============================================================================
// <global variable> :: _token_operator_list [���Z�q�ꗗ]
//=============================================================================
const char* _token_operator_list = "+-*/%!=&|><";

