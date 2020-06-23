//=============================================================================
// include files
//=============================================================================
#include <stdio.h>
#include "flight.h"


//=============================================================================
// embedded function
//=============================================================================
void add(flight_variable& ret_var, const flight_variable& arg1, const flight_variable& arg2)
{
	ret_var.value = std::to_string(atoi(arg1.value.c_str()) + atoi(arg2.value.c_str()));
	ret_var.type = flight_data_type::FDT_VARNUMBER;
	return;
}


//=============================================================================
// <global functions> :: entry point
//=============================================================================
int main(int argc, char* argv[])
{
	// 
	flight f;
	do
	{
		// 
		if (!f.load_file("script/test.f")) { break; }
		if (!f.load_complete()) { break; }

		// ホストに対して関数の追加
		if (!f.register_function("add", add)) { break; }

		f.exec();
	} while (false);

	// エラーが発生した場合
	if (f.get_last_error() != flight_errors::error_none)
	{
		printf("%s", f.get_last_error_string().c_str());
	}
	else
	{
		// 最後の実行データを取得
		flight_variable var;
		f.get_exec_data(var);
		printf("last data => %s\n", var.value.c_str());
	}
	return 0;
}
