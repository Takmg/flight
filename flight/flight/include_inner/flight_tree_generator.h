#pragma once


//=============================================================================
// externs
//=============================================================================
extern void flight_parser_func(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_while(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_for(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_if(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_var(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_simple(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_ret(flight_generate_data&, std::vector<flight_token>&);
extern void flight_parser_error(flight_generate_data&, std::vector<flight_token>&);


//=============================================================================
// prototypes
//=============================================================================
void flight_tree_generater(fi_container&, int&, std::vector<flight_token>&, const error_setter&);
