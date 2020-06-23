//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <global variable> :: _token_reserved_instruction_list [予約語一覧(命令)]
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

	// ここから下はソースから使えない命令
	{ "__RESET_ACCUM__!" , flight_parser_simple },	// アキュムレータのリセット
};


//=============================================================================
// <global variable> :: _token_reserved_value_list [予約語一覧(右辺値)]
//=============================================================================
const flight_reserved_data _token_reserved_value_list[] = {
	{ "true" },
	{ "false" },
	{ "null" },
};


//=============================================================================
// <global variable> :: _token_reserved_instruction_list_size 予約語サイズ
//=============================================================================
const int _token_reserved_instruction_list_size = sizeof(_token_reserved_instruction_list) / sizeof(_token_reserved_instruction_list[0]);


//=============================================================================
// <global variable> :: _token_reserved_value_list_size 予約語サイズ
//=============================================================================
const int _token_reserved_value_list_size = sizeof(_token_reserved_value_list) / sizeof(_token_reserved_value_list[0]);


//=============================================================================
// <global variable> :: _token_operator_list [演算子一覧]
//=============================================================================
const char* _token_operator_list = "+-*/%!=&|><";

