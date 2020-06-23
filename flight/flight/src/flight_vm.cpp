//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// prototypes
//=============================================================================
static flight_vm_status flight_vm_exec_recursive(flight_vm_data&, flight_variable&, const fl_ptr&);
static flight_vm_status flight_vm_exec_eblock(flight_vm_data&);


//=============================================================================
// typedefs
//=============================================================================
typedef flight_vm_status(*op_func)(flight_vm_data&, flight_variable&, const fl_ptr&);


//=============================================================================
// <struct> :: op_data 
//=============================================================================
struct op_data
{
	const char* op_str;
	op_func func;
};


//=============================================================================
// <static functions> :: flight_vm_get_now_instruction
//=============================================================================
static const fi_ptr flight_vm_get_now_instruction(const flight_vm_data& vmdata)
{
	// プログラムカウンターに応じた命令を取得
	const auto& insts = *vmdata.instructions;
	return insts[vmdata.program_counter];
}


//=============================================================================
// <static functions> :: flight_vm_get_func
//=============================================================================
static flight_vm_status flight_vm_get_func(
	const flight_vm_data& vmdata, flight_variable& ret_var,
	const std::string& search_funcname)
{
	// 関数を検索
	const auto& funcs = vmdata.functions;
	const auto f = [&](const auto& el) { return el.function_name == search_funcname; };
	const auto& fit = std::find_if(funcs.begin(), funcs.end(), f);

	// ホスト関数を検索
	const auto& hfuncs = *vmdata.phf_holders;
	const auto hf = [&](const auto& el) { return el.first.find(search_funcname) == 0; };
	const auto& fit2 = std::find_if(hfuncs.begin(), hfuncs.end(), hf);

	// 関数リストに存在しなかった場合
	if (fit == funcs.end() && fit2 == hfuncs.end()) { return flight_vm_status::VM_ERROR; }

	// 関数名の設定
	ret_var.variable_name = "tmp";
	ret_var.type = flight_data_type::FDT_VARFUNC;
	ret_var.value = search_funcname;
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_store
//=============================================================================
static flight_vm_status flight_vm_store_inner(
	flight_vm_data& vmdata, const std::string& variable_name,
	const std::function<void(flight_variable&)>& f)
{
	// スタックポインターを順に実行していく
	for (int sp = vmdata.stack_pointer; sp < (int)vmdata.stack_variables.size(); sp++)
	{
		auto& svar = vmdata.stack_variables[sp];

		// スタックポインターが同一でない 又は ブロックポインターが現在地より低い変数 でない場合は抜ける
		if (!(svar.stack_level == vmdata.now_stack_level && svar.block_level <= vmdata.now_block_level))
		{
			// ここまで変数が見つからなかった
			break;
		}

		// 変数名が異なる場合はcontinue
		if (svar.variable_name != variable_name) { continue; }

		// 変数の設定
		f(svar);
		return flight_vm_status::VM_EXEC;
	}

	// グローバル変数から情報を取得
	for (int index = 0; index < (int)vmdata.global_variables.size(); index++)
	{
		// 変数が見つかった場合
		if (vmdata.global_variables[index].variable_name == variable_name)
		{
			// 変数の設定
			f(vmdata.global_variables[index]);
			return flight_vm_status::VM_EXEC;
		}
	}

	// ここまで来た場合は見つからなかった
	return flight_vm_status::VM_ERROR;
}


//=============================================================================
// <static functions> :: flight_vm_store
//=============================================================================
static flight_vm_status flight_vm_store(
	flight_vm_data& vmdata, const flight_variable& var, const std::string& var_name)
{
	const auto f = [&](auto& vm_var)
	{
		vm_var.type = var.type;
		vm_var.value = var.value;
	};
	const auto ret = flight_vm_store_inner(vmdata, var_name, f);
	if (ret != flight_vm_status::VM_EXEC)
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_not_found_variable;
		vmdata.error_token = { vmdata.now_line_no, var_name, };
		return flight_vm_status::VM_ERROR;
	}
	return ret;
}


//=============================================================================
// <static functions> :: flight_vm_restore
//=============================================================================
static flight_vm_status flight_vm_restore(
	flight_vm_data& vmdata, flight_variable& ret_var, const std::string& var_name)
{
	const auto f = [&](auto& vm_var)
	{
		ret_var.type = vm_var.type;
		ret_var.value = vm_var.value;
	};
	auto ret = flight_vm_store_inner(vmdata, var_name, f);
	if (ret != flight_vm_status::VM_EXEC)
	{
		ret = flight_vm_get_func(vmdata, ret_var, var_name);
		if (ret != flight_vm_status::VM_EXEC)
		{
			// 変数が見つからないエラー
			vmdata.error_line_no = vmdata.now_line_no;
			vmdata.error = flight_errors::error_not_found_variable;
			vmdata.error_token = { vmdata.now_line_no, var_name, };
		}
		return ret;
	}
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_comma
//=============================================================================
static flight_vm_status flight_vm_op_comma(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// カンマの処理

	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable var1, var2;
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// 変数に何もないデータを格納する
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";
	ret_var.variable_name = "tmp";

	// 成功を返す
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_cond
//=============================================================================
static flight_vm_status flight_vm_op_cond(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable var1, var2;

	// まず右辺値の計算
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// 左辺値も評価する
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// 戻る
	const auto& op = node->token.token_str;
	ret_var.type = flight_data_type::FDT_VARNUMBER;

	// エラーフラグ
	bool is_error = false;

	// 片方が関数ポインタだった場合エラー
	if (var1.type == flight_data_type::FDT_VARFUNC || var2.type == flight_data_type::FDT_VARFUNC)
	{
		is_error = true;
	}

	// 数値同士の比較
	else if (var1.type == flight_data_type::FDT_VARNUMBER && var2.type == flight_data_type::FDT_VARNUMBER)
	{
		const auto n1 = atoi(var1.value.c_str());
		const auto n2 = atoi(var2.value.c_str());
		if (op == "&&") { ret_var.value = std::to_string(n1 && n2 ? 1 : 0); }
		else if (op == "||") { ret_var.value = std::to_string(n1 || n2 ? 1 : 0); }
		else if (op == ">=") { ret_var.value = std::to_string(n1 >= n2 ? 1 : 0); }
		else if (op == "<=") { ret_var.value = std::to_string(n1 <= n2 ? 1 : 0); }
		else if (op == ">") { ret_var.value = std::to_string(n1 > n2 ? 1 : 0); }
		else if (op == "<") { ret_var.value = std::to_string(n1 < n2 ? 1 : 0); }
		else if (op == "==") { ret_var.value = std::to_string(n1 == n2 ? 1 : 0); }
		else if (op == "!=") { ret_var.value = std::to_string(n1 != n2 ? 1 : 0); }
		else { is_error = true; }
	}

	// 文字列との比較
	else if (var1.type == flight_data_type::FDT_STRING || var2.type == flight_data_type::FDT_STRING)
	{
		const auto& n1 = var1.value;
		const auto& n2 = var2.value;
		if (op == ">=") { ret_var.value = std::to_string(n1 >= n2 ? 1 : 0); }
		else if (op == "<=") { ret_var.value = std::to_string(n1 <= n2 ? 1 : 0); }
		else if (op == ">") { ret_var.value = std::to_string(n1 > n2 ? 1 : 0); }
		else if (op == "<") { ret_var.value = std::to_string(n1 < n2 ? 1 : 0); }
		else if (op == "==") { ret_var.value = std::to_string(n1 == n2 ? 1 : 0); }
		else if (op == "!=") { ret_var.value = std::to_string(n1 != n2 ? 1 : 0); }
		else { is_error = true; }
	}

	// それ以外の処理
	else { is_error = true; }

	// エラーだった場合
	if (is_error)
	{
		// 演算子のエラー
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// 成功
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_not
//=============================================================================
static flight_vm_status flight_vm_op_not(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable var1;

	// 左辺値を評価する
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// 数値以外だった場合は 条件演算子を使用できない
	if (var1.type != flight_data_type::FDT_VARNUMBER)
	{
		// 数値以外で条件演算子を使用しようとした
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// 戻る
	ret_var.type = flight_data_type::FDT_VARNUMBER;

	// not演算子
	const auto n1 = atoi(var1.value.c_str());
	ret_var.value = std::to_string((!n1) ? 1 : 0);

	// 成功
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_common
//=============================================================================
static flight_vm_status flight_vm_op_common(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable var1, var2;

	// まず右辺値の計算
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// 左辺値も評価する
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// 戻る
	const char op = node->token.token_str[0];

	bool is_error = true;

	// 片方が関数ポインタだった場合エラー
	if (var1.type == flight_data_type::FDT_VARFUNC || var2.type == flight_data_type::FDT_VARFUNC)
	{
		is_error = true;
	}

	// 数値同士の計算
	else if (var1.type == flight_data_type::FDT_VARNUMBER && var2.type == flight_data_type::FDT_VARNUMBER)
	{
		// 小数点有数値に変換する
		const auto n1 = atof(var1.value.c_str());
		const auto n2 = atof(var2.value.c_str());
		if (op == '+') { ret_var.value = std::to_string(n1 + n2); }
		if (op == '-') { ret_var.value = std::to_string(n1 - n2); }
		if (op == '*') { ret_var.value = std::to_string(n1 * n2); }
		if (op == '/') { ret_var.value = std::to_string(n1 / n2); }

		// 小数点以下が全て0だった場合は小数点以下を削除する
		bool is_frac = false;
		const int dot_index = (int)ret_var.value.find('.');
		for (int i = dot_index + 1; dot_index >= 0 && i < (int)ret_var.value.length(); i++)
		{
			// 小数点以下で0以外が出た場合は小数フラグをtrueに設定
			if (ret_var.value[i] != '0') { is_frac = true; break; }
		}
		if (!is_frac) { ret_var.value = ret_var.value.substr(0, dot_index); }

		ret_var.type = flight_data_type::FDT_VARNUMBER;

		// エラーなし
		is_error = false;
	}
	// 片方が文字列だった場合
	else if (var1.type == flight_data_type::FDT_STRING || var2.type == flight_data_type::FDT_STRING)
	{
		// +以外だった場合
		if (op != '+')
		{
			// 文字列の加算は+以外認められない
			vmdata.error_line_no = node->token.line_no;
			vmdata.error = flight_errors::error_not_support_operation;
			vmdata.error_token = node->token;
			return flight_vm_status::VM_ERROR;
		}

		// 文字列の単純加算を行う
		ret_var.value = var1.value + var2.value;
		ret_var.type = flight_data_type::FDT_STRING;

		// エラーなし
		is_error = false;
	}

	// 未サポート
	if (is_error)
	{
		// 代入演算子の未サポート
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// 代入演算子だった場合 storeを行う
	if (node->token.token_str.length() >= 2 && node->token.token_str[1] == '=')
	{
		// 変数に値の設定
		const auto status3 = flight_vm_store(vmdata, ret_var, node->left->token.token_str);
		if (status3 != flight_vm_status::VM_EXEC) { return status3; }
	}

	// 成功
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_equal
//=============================================================================
static flight_vm_status flight_vm_op_equal(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable var1, var2;

	// まず右辺値の計算
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// 左辺値も評価する
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// 未設定の変数に代入は使用できない
	if (var2.type == flight_data_type::FDT_NONE)
	{
		// 代入演算子で数値以外を代入しようとした
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// 変数に値の設定
	const auto status3 = flight_vm_store(vmdata, var2, node->left->token.token_str);
	if (status3 != flight_vm_status::VM_EXEC) { return status3; }

	// 成功
	return flight_vm_status::VM_EXEC;
}



//=============================================================================
// static variables
//=============================================================================
static const op_data _op_data[] =
{
	{ "*=", flight_vm_op_common },
	{ "/=", flight_vm_op_common },
	{ "+=", flight_vm_op_common },
	{ "-=", flight_vm_op_common },
	{ "=" , flight_vm_op_equal },
	{ "||", flight_vm_op_cond },
	{ "&&", flight_vm_op_cond },
	{ "==", flight_vm_op_cond },
	{ "!=", flight_vm_op_cond },
	{ "<" , flight_vm_op_cond },
	{ "<=", flight_vm_op_cond },
	{ ">" , flight_vm_op_cond },
	{ ">=", flight_vm_op_cond },
	{ "+" , flight_vm_op_common },
	{ "-" , flight_vm_op_common },
	{ "*" , flight_vm_op_common },
	{ "/" , flight_vm_op_common },
	{ "!" , flight_vm_op_not },
};


//=============================================================================
// <static functions> :: flight_vm_push
//=============================================================================
static flight_vm_status flight_vm_push(flight_vm_data& vmdata, const flight_stack_variable& var)
{
	// スタックポインタを取得
	int& sp = vmdata.stack_pointer;
	--sp;

	// スタックポインターが0未満ならスタックオーバーフロー
	if (sp < 0)
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_stack_overflow;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// 同一変数名の確認
	const auto f = [&](const auto& el)
	{
		return	el.variable_name == var.variable_name &&
			el.block_level == var.block_level &&
			el.stack_level == var.stack_level;
	};
	const auto& it = std::find_if(vmdata.stack_variables.begin() + sp + 1, vmdata.stack_variables.end(), f);
	if (it != vmdata.stack_variables.end())
	{
		// 同一変数名が存在する場合はエラー
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_declare_variable;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// 変数の設定
	vmdata.stack_variables[sp] = var;

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "create local variable = %s", var.variable_name.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_pop
//=============================================================================
static flight_vm_status flight_vm_pop(flight_vm_data& vmdata, flight_variable& var)
{
	// スタックポインタを取得
	int& sp = vmdata.stack_pointer;

	// 同一変数名の確認
	const auto& sv = vmdata.stack_variables[sp];
	var.type = sv.type;
	var.value = sv.value;
	var.variable_name = sv.variable_name;

	// スタックポインターがサイズ以上ならスタックオーバーフロー
	if (++sp > (int)vmdata.stack_variables.size())
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_stack_overflow;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "delete local variable = %s", var.variable_name.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_func_arg_count_recursive
//=============================================================================
static void flight_vm_func_arg_get_recursive(
	flight_vm_data& vmdata, std::vector<flight_variable>& vars, const fl_ptr& node)
{
	// nodeがnullなら何もしない
	if (node == nullptr) { return; }

	// カンマだった場合は値の設定
	if (node->token.type == flight_token_type::FTK_SEPARATOR)
	{
		flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
		flight_vm_func_arg_get_recursive(vmdata, vars, node->right);
		return;
	}

	// 引数の指定
	flight_variable arg;
	const auto status = flight_vm_exec_recursive(vmdata, arg, node);
	if (status == flight_vm_status::VM_EXEC) { vars.push_back(arg); }
}


//=============================================================================
// <static functions> :: flight_vm_host_func
//=============================================================================
static flight_vm_status flight_vm_host_func(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node,
	const std::string& function_name)
{
	// ホスト関数を取得
	const auto& hfuncs = *vmdata.phf_holders;

	// 引数を取得
	std::vector<flight_variable> vars;
	flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// ホスト用関数名を作成
	const std::string func_name = function_name + "-" + std::to_string(vars.size());

	// 関数から情報を取得
	const auto f = [&](const auto& f) { return f.first == func_name; };
	const auto& fit = std::find_if(hfuncs.begin(), hfuncs.end(), f);
	if (fit == hfuncs.end())
	{
		// 関数が見つからなかった
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_found_function;
		vmdata.error_token = { node->token.line_no, function_name };
		return flight_vm_status::VM_ERROR;
	}

	// 引数が異なる場合はエラー
	const auto& arg_size = fit->second->_arg_size;
	if (arg_size != vars.size()) { return flight_vm_status::VM_ERROR; }

#ifdef _DEBUG
	// デバッグ表示で見難くなっている部分を\nで改行させて見やすくする
	char a[0x100];
	sprintf_s(a, "start host function %s", node->token.token_str.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	// ホスト関数の実行
	fit->second->_caller(fit->second->_finstance, ret_var, vars);

#ifdef _DEBUG
	// デバッグ表示で見難くなっている部分を\nで改行させて見やすくする
	puts("");
	flight_vm_debug_state(vmdata, "end host function");
#endif

	// プログラムカウンターを一つ進める
	++vmdata.program_counter;

	// 成功を返す
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_func
//=============================================================================
static flight_vm_status flight_vm_func(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// nodeがnullの場合何もしない
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// 検索名
	std::string search_name = node->token.token_str;

	// まず変数から関数名が取得出来るか試す
	do {

		// 検索処理
		flight_variable var;
		const auto f = [&](auto& vm_var)
		{
			var.type = vm_var.type;
			var.value = vm_var.value;
		};

		// 変数の取得に成功した場合は変数内の値に検索する関数を変える
		auto ret = flight_vm_store_inner(vmdata, search_name, f);
		if (ret == flight_vm_status::VM_EXEC && var.type == flight_data_type::FDT_VARFUNC)
		{
			search_name = var.value;
		}
	} while (false);

	// 関数から情報を取得
	const auto& funcs = vmdata.functions;
	const auto f = [&](const auto& f) { return f.function_name == search_name; };
	const auto& fit = std::find_if(funcs.begin(), funcs.end(), f);
	if (fit == funcs.end())
	{
		// 関数が見つからなかった場合は、ホスト関数を取得する。
		return flight_vm_host_func(vmdata, ret_var, node, search_name);
	}

	// スタックに退避するデータを取得
	const int return_pc = vmdata.program_counter + 1;	// 戻る場所を指定
	const int block_level = vmdata.now_block_level;		// 現在のブロックレベルを退避
	const int stack_level = vmdata.now_stack_level;		// 現在のスタックレベルを退避

	// 値の設定
	flight_stack_variable var_rpc;
	flight_stack_variable var_bl;
	var_rpc.block_level = var_bl.block_level = block_level;
	var_rpc.stack_level = var_bl.stack_level = stack_level;
	var_rpc.type = var_bl.type = flight_data_type::FDT_VARNUMBER;
	var_rpc.variable_name = "___RETURN_PROGRAM_COUNTER___!";
	var_bl.variable_name = "___SAVE_BLOCK_LEVEL___!";
	var_rpc.value = std::to_string(return_pc);
	var_bl.value = std::to_string(block_level);

	// vmに値のpushを行う
	const auto status1 = flight_vm_push(vmdata, var_bl);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	const auto status2 = flight_vm_push(vmdata, var_rpc);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// 引数カウントを取得
	std::vector<flight_variable> vars;
	flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// 引数が異なるためエラー
	if (fit->args.size() != vars.size())
	{
		// 引数が違うエラー
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_function_argument_count;
		vmdata.error_token = { node->token.line_no, search_name };
		return flight_vm_status::VM_ERROR;
	}

	// このタイミングでブロックレベルやスタックレベルを変更
	vmdata.now_block_level = 0;		// ブロックレベルを0クリア
	vmdata.now_stack_level++;		// スタックレベルをインクリメント

	// 引数を設定
	for (int i = 0; i < (int)fit->args.size(); i++)
	{
		flight_stack_variable svar;
		svar.variable_name = fit->args[i].variable_name;
		svar.type = vars[i].type;
		svar.value = vars[i].value;
		svar.block_level = vmdata.now_block_level;
		svar.stack_level = vmdata.now_stack_level;
		const auto ret = flight_vm_push(vmdata, svar);
		if (ret != flight_vm_status::VM_EXEC) { return ret; }
	}

	// 関数のスタートアドレスに移動
	vmdata.program_counter = fit->start_address;

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "start function %s", fit->function_name.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_exit_func
//=============================================================================
static flight_vm_status flight_vm_exit_func(flight_vm_data& vmdata)
{
	// スタックレベルをデクリメント
	--vmdata.now_stack_level;

	// スタックレベル以下の変数を削除
	int& sp = vmdata.stack_pointer;
	const auto& sv = vmdata.stack_variables;
	for (; sp < (int)sv.size(); sp++)
	{
		// スタックレベルが超えているものを削除していく
		if (sv[sp].stack_level > vmdata.now_stack_level)
		{
#ifdef _DEBUG
			char a[0x100];
			sprintf_s(a, "delete local variable = %s", sv[sp].variable_name.c_str());
			flight_vm_debug_state(vmdata, a);
#endif
			continue;
		}

		// スタックポインタが下がりきった為、ループを抜ける
		break;
	}

	// 値のポップを行う
	flight_variable var_bl;
	flight_variable var_rpc;
	flight_vm_pop(vmdata, var_rpc);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }
	flight_vm_pop(vmdata, var_bl);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// プログラムカウンターとブロックレベルを関数呼び出し前に戻す
	vmdata.now_block_level = atoi(var_bl.value.c_str());
	vmdata.program_counter = atoi(var_rpc.value.c_str());

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "end functions");
	flight_vm_debug_state(vmdata, a);
#endif

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_exec_recursive
//=============================================================================
static flight_vm_status flight_vm_exec_recursive(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// ノードがnullだった場合は成功として返す
	if (node == nullptr) { return flight_vm_status::VM_EXEC; }

	// 関数を設定
	const auto error_func = [&]() {
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_syntax;
		vmdata.error_token = { node->token.line_no, node->token.token_str };
	};

	// 数値だった場合
	if (node->token.type == flight_token_type::FTK_VALUE_NUMBER || node->token.type == flight_token_type::FTK_VALUE_CHAR)
	{
		// 数値なのに子供がある場合はエラー
		if (node->left != nullptr || node->right != nullptr)
		{
			// エラー設定
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// 数値の設定
		ret_var.variable_name = "tmp";
		ret_var.value = node->token.token_str;
		ret_var.type = flight_data_type::FDT_VARNUMBER;
		return flight_vm_status::VM_EXEC;
	}
	// 文字列だった場合
	else if (node->token.type == flight_token_type::FTK_VALUE_STR)
	{
		// 文字列なのに子供がある場合はエラー
		if (node->left != nullptr || node->right != nullptr)
		{
			// エラー設定
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// 文字列の設定
		ret_var.variable_name = "tmp";
		ret_var.value = node->token.token_str;
		ret_var.type = flight_data_type::FDT_STRING;
		return flight_vm_status::VM_EXEC;
	}
	// 変数だった場合
	else if (node->token.type == flight_token_type::FTK_VARIABLE)
	{
		// 変数なのに子供がある場合はエラー
		if (node->left != nullptr || node->right != nullptr)
		{
			// エラー設定
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// 変数から情報を取得する
		return flight_vm_restore(vmdata, ret_var, node->token.token_str);
	}
	// カンマだった場合
	else if (node->token.type == flight_token_type::FTK_SEPARATOR)
	{
		return flight_vm_op_comma(vmdata, ret_var, node);
	}
	// 演算子だった場合
	else if (node->token.type == flight_token_type::FTK_OPERATION)
	{
		const int len = sizeof(_op_data) / sizeof(_op_data[0]);
		for (int i = 0; i < len; i++)
		{
			// 各種コードの処理
			if (node->token.token_str == _op_data[i].op_str)
			{
				return _op_data[i].func(vmdata, ret_var, node);
			}
		}
	}
	// アキュムレータだった場合
	else if (node->token.type == flight_token_type::FTK_ACCUMULATOR)
	{
		// アキュムレータなのに子供がある場合はエラー
		if (node->left != nullptr || node->right != nullptr)
		{
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// アキュムレータを設定
		ret_var = vmdata.accumulator;
		return flight_vm_status::VM_EXEC;
	}

	return flight_vm_status::VM_ERROR;
}


//=============================================================================
// <static functions> :: flight_vm_goto
//=============================================================================
static flight_vm_status flight_vm_goto(
	flight_vm_data& vmdata, const std::string& to_label,
	const bool forward = true, const bool full_match = true)
{
	// ステップ値
	const int step = forward ? 1 : -1;

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "goto label = %s", to_label.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	int block_level = 0;

	// 命令コンテナを取得
	const auto& instructions = *vmdata.instructions;

	// 現在のスタックレベルを保持
	const int stack_level = vmdata.now_stack_level;

	// プログラムカウンターを進める
	int& pc = vmdata.program_counter;

	// プログラムカウンターを進めながらラベルを検索する
	for (pc += step; 0 <= pc && pc < (int)instructions.size(); pc += step)
	{
		// ターゲットトークンを取得
		const auto& target_token = instructions[pc]->root->token;

		// ブロック開始ならblock_levelをインクリメント/ブロック終了ならデクリメント
		if (target_token.type == flight_token_type::FTK_BLOCK)
		{
			block_level += step;
			if (block_level < 0)
			{
				const auto ret = flight_vm_exec_eblock(vmdata);
				if (ret != flight_vm_status::VM_EXEC) { return ret; }

				// スタックレベルが異なる場合はエラーとする
				if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0) { return flight_vm_status::VM_ERROR; }

				// ブロックレベルが0未満ならブロックレベルを0に戻す
				// ブロックレベルが-1以下の時に{}が発生しても
				// ブロック終了処理を行わないようにするため。
				block_level = 0;
			}
			continue;
		}
		else if (target_token.type == flight_token_type::FTK_ENDBLOCK)
		{
			// ブロックレベルが0未満ならブロック終了処理を行う
			block_level -= step;
			if (block_level < 0)
			{
				const auto ret = flight_vm_exec_eblock(vmdata);
				if (ret != flight_vm_status::VM_EXEC) { return ret; }

				// スタックレベルが異なる場合はエラーとする
				if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0) { return flight_vm_status::VM_ERROR; }

				// ブロックレベルが0未満ならブロックレベルを0に戻す
				// ブロックレベルが-1以下の時に{}が発生しても
				// ブロック終了処理を行わないようにするため。
				block_level = 0;
			}
			continue;
		}

		// ラベルが見つかった場合、ループを抜ける
		if (block_level <= 0 && target_token.type == flight_token_type::FTK_LABEL)
		{
			// 文字列が一致している場合は抜ける
			if (full_match && target_token.token_str == to_label ||
				!full_match && target_token.token_str.find(to_label) == 0)
			{
				break;
			}
		}
	}

	// エラー プログラムの最後までラベルが存在しなかった
	if (pc < 0 || (int)instructions.size() <= pc)
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_not_found_label;
		vmdata.error_token = { vmdata.now_line_no, to_label };
		return flight_vm_status::VM_ERROR;
	}

	// ラベルの次の行にプログラムカウンターを進める
	++pc;

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_make_stack_var
//=============================================================================
static flight_vm_status flight_vm_make_stack_var(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 変数トークン
	const auto var_token = inst->root->left->token;

	// 一時変数を作成
	flight_stack_variable var;
	var.variable_name = var_token.token_str;
	var.value = "";
	var.block_level = vmdata.now_block_level;
	var.stack_level = vmdata.now_stack_level;
	var.type = flight_data_type::FDT_NONE;

	// スタック領域にpush
	return flight_vm_push(vmdata, var);
}


//=============================================================================
// <static functions> :: flight_vm_make_global_var
//=============================================================================
static flight_vm_status flight_vm_make_global_var(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 変数トークン
	const auto var_token = inst->root->left->token;

	// 変数を作成
	flight_variable var;
	var.variable_name = var_token.token_str;
	var.value = "";
	var.type = flight_data_type::FDT_NONE;

	// 同一変数名の確認
	const auto f = [&](const auto& el) { return el.variable_name == var_token.token_str; };
	const auto& it = std::find_if(vmdata.global_variables.begin(), vmdata.global_variables.end(), f);
	if (it != vmdata.global_variables.end())
	{
		// 同一変数名が存在する場合はエラー
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_declare_variable;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// グローバル変数に追加
	vmdata.global_variables.push_back(var);

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "create global variable = %s", var.variable_name.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	// 成功
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_if
//=============================================================================
static flight_vm_status flight_vm_if(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 各種ステートメントを取得
	const fl_ptr root = inst->root;
	const fl_ptr cond = root != nullptr ? inst->root->left : nullptr;
	const fl_ptr elsc = root != nullptr ? inst->root->right : nullptr;

	// 各種ステートメントがnullだった場合はエラー
	if (root == nullptr || cond == nullptr || elsc == nullptr) { return flight_vm_status::VM_ERROR; }

	// 一時変数の作成
	flight_variable cond_var;
	const auto status1 = flight_vm_exec_recursive(vmdata, cond_var, cond);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// 条件変数の結果を取得
	if (cond_var.type != flight_data_type::FDT_VARNUMBER || cond_var.value.length() <= 0)
	{
		// 条件が数値じゃなかった
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_if_condition;
		vmdata.error_token = { vmdata.now_line_no, cond_var.value };
		return flight_vm_status::VM_ERROR;
	}

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "if condition = %s", cond_var.value != "0" ? "true" : "false");
	flight_vm_debug_state(vmdata, a);
#endif

	// 偽条件ならラベルへジャンプ
	if (cond_var.value == "0")
	{
		return flight_vm_goto(vmdata, elsc->token.token_str, elsc->token.type == flight_token_type::FTK_GOTOLABEL);
	}
	// 真条件なら次へ進む
	else
	{
		++vmdata.program_counter;
	}

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_return
//=============================================================================
static flight_vm_status flight_vm_return(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 各種ステートメントを取得
	const fl_ptr root = inst->root;
	const fl_ptr retv = root != nullptr ? inst->root->left : nullptr;

	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// 戻り変数
	flight_variable ret_var;
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";

	// return valueがある場合は変数を取得する
	if (retv != nullptr)
	{
		// 一時変数の作成
		const auto status1 = flight_vm_exec_recursive(vmdata, ret_var, retv);
		if (status1 != flight_vm_status::VM_EXEC) { return status1; }

		// 条件変数の結果を取得
		if (ret_var.type != flight_data_type::FDT_VARNUMBER || ret_var.value.length() <= 0)
		{
			vmdata.error_line_no = vmdata.now_line_no;
			vmdata.error = flight_errors::error_return_value;
			vmdata.error_token = { vmdata.now_line_no, retv->token.token_str };
			return flight_vm_status::VM_ERROR;
		}
	}
	
	// 関数実行中以外だった場合はそのままプログラムを終了させる
	if (vmdata.now_stack_level <= 0)
	{
		// ホストに返すデータとして値を設定する
		vmdata.exit_var = ret_var;

		vmdata.exit_flg = true;
		return flight_vm_status::VM_SUSPEND;
	}

	// アキュムレータに情報設定
	flight_variable& acm = vmdata.accumulator;
	acm.value = ret_var.value;
	acm.type = ret_var.type;

	// 関数の終了までジャンプ
	return flight_vm_goto(vmdata, "func-end", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_yield
//=============================================================================
static flight_vm_status flight_vm_yield(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 各種ステートメントを取得
	const fl_ptr root = inst->root;
	const fl_ptr retv = root != nullptr ? inst->root->left : nullptr;

	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// 戻り変数
	flight_variable ret_var;
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";

	// return valueがある場合は変数を取得する
	if (retv != nullptr)
	{
		// 一時変数の作成
		const auto status1 = flight_vm_exec_recursive(vmdata, ret_var, retv);
		if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	}

	// yieldの場合も戻り値を設定
	vmdata.exit_var = ret_var;

	// プログラムカウンターを進める(yeildからの復帰先)
	++vmdata.program_counter;

	// 中断命令を返す
	return flight_vm_status::VM_SUSPEND;
}


//=============================================================================
// <static functions> :: flight_vm_break
//=============================================================================
static flight_vm_status flight_vm_break(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 各種ステートメントを取得
	const fl_ptr root = inst->root;
	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// ループの終了までジャンプ
	return flight_vm_goto(vmdata, "loop-end", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_continue
//=============================================================================
static flight_vm_status flight_vm_continue(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 各種ステートメントを取得
	const fl_ptr root = inst->root;
	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// ループの終了までジャンプ
	return flight_vm_goto(vmdata, "loop-continue", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_instruction
//=============================================================================
static flight_vm_status flight_vm_instruction(flight_vm_data& vmdata)
{
	// 命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// トークンの取得
	const auto& root_token = inst->root->token;
	flight_vm_status ret = flight_vm_status::VM_ERROR;

	// 変数宣言だった場合
	if (root_token.token_str == "var")
	{
		// ブロックレベルもスタックレベルも0の場合はグローバル変数
		if (vmdata.now_block_level == 0 && vmdata.now_stack_level == 0)
		{
			ret = flight_vm_make_global_var(vmdata);
		}
		// ここに来た場合はローカル変数
		else
		{
			ret = flight_vm_make_stack_var(vmdata);
		}

		// プログラムカウンターを進める
		++vmdata.program_counter;
	}
	// ifブロックだった場合
	else if (root_token.token_str == "if")
	{
		ret = flight_vm_if(vmdata);
	}
	// returnブロックだった場合
	else if (root_token.token_str == "return")
	{
		ret = flight_vm_return(vmdata);
	}
	// breakだった場合
	else if (root_token.token_str == "break")
	{
		ret = flight_vm_break(vmdata);
	}
	// continueだった場合
	else if (root_token.token_str == "continue")
	{
		ret = flight_vm_continue(vmdata);
	}
	// yieldだった場合
	else if (root_token.token_str == "yield")
	{
		// returnと同様の処理を行う
		ret = flight_vm_yield(vmdata);
	}
	// アキュムレータのリセット
	else if (root_token.token_str == "__RESET_ACCUM__!")
	{
		vmdata.accumulator.type = flight_data_type::FDT_NONE;
		vmdata.accumulator.value = "";

		// プログラムカウンターを進める
		++vmdata.program_counter;

		ret = flight_vm_status::VM_EXEC;
	}

	return ret;
}


//=============================================================================
// <static functions> :: flight_vm_exec_sblock
//=============================================================================
static void flight_vm_exec_sblock(flight_vm_data& vmdata)
{
#ifdef _DEBUG
	flight_vm_debug_state(vmdata, "start block");
#endif

	// ブロックレベルを1つ上げる
	++vmdata.now_block_level;
}


//=============================================================================
// <static functions> :: flight_vm_exec_eblock
//=============================================================================
static flight_vm_status flight_vm_exec_eblock(flight_vm_data& vmdata)
{
	// ブロックレベルを1つ上げる
	if (--vmdata.now_block_level < 0)
	{
		// エラー 終わりブロックが先に来てしまっている。
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_syntax;
		vmdata.error_token = { vmdata.now_line_no , "}" , };
		return flight_vm_status::VM_ERROR;
	}

	// ブロックレベル以下の変数を削除
	int& sp = vmdata.stack_pointer;
	const auto& sv = vmdata.stack_variables;
	for (; sp < (int)sv.size(); sp++)
	{
		// スタックレベルもブロックレベルも上のものを削除していく
		// (ブロックレベルに関しては、本コードの上でデクリメントしているため、
		//  スタック内で超えたものを削除していく。)
		if (sv[sp].block_level > vmdata.now_block_level && sv[sp].stack_level >= vmdata.now_stack_level)
		{
#ifdef _DEBUG
			char a[0x100];
			sprintf_s(a, "delete local variable = %s", sv[sp].variable_name.c_str());
			flight_vm_debug_state(vmdata, a);
#endif
			continue;
		}

		// スタックポインタが下がりきった為、ループを抜ける
		break;
	}

#ifdef _DEBUG
	flight_vm_debug_state(vmdata, "end block");
#endif

	// 成功とする
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_exec_inner
//=============================================================================
static flight_vm_status flight_vm_exec_inner(flight_vm_data& vmdata)
{
	// 現在の命令を取得
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// 現在の行番号を設定
	vmdata.now_line_no = inst->root->token.line_no;

	// トークンの取得
	const auto& root_token = inst->root->token;
	flight_vm_status ret = flight_vm_status::VM_EXEC;

	// 予約語だった場合の処理
	if (root_token.type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		ret = flight_vm_instruction(vmdata);
	}
	// ブロック開始処理
	else if (root_token.type == flight_token_type::FTK_BLOCK)
	{
		flight_vm_exec_sblock(vmdata);

		// プログラムカウンターを次に進める
		++vmdata.program_counter;
	}
	// ブロック終了処理
	else if (root_token.type == flight_token_type::FTK_ENDBLOCK)
	{
		// 内部でエラーが発生する場合がある。
		ret = flight_vm_exec_eblock(vmdata);

		if (ret == flight_vm_status::VM_EXEC)
		{
			// 関数の終了だった場合
			if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0)
			{
				ret = flight_vm_exit_func(vmdata);
			}
			// プログラムカウンターを次に進める
			else
			{
				++vmdata.program_counter;
			}
		}
	}
	// ジャンプ命令
	else if (root_token.type == flight_token_type::FTK_GOTOLABEL)
	{
		ret = flight_vm_goto(vmdata, root_token.token_str, true);
	}
	// 後ろへのジャンプ命令
	else if (root_token.type == flight_token_type::FTK_GOTOBACKLABEL)
	{
		ret = flight_vm_goto(vmdata, root_token.token_str, false);
	}
	// ラベルだった場合の処理
	else if (root_token.type == flight_token_type::FTK_LABEL)
	{
		// 何もしない。プログラムカウンタを進めて終わる
		++vmdata.program_counter;
	}
	// 演算子だった場合
	else if (root_token.type == flight_token_type::FTK_OPERATION)
	{
		// オペレータ処理を実行する
		ret = flight_vm_exec_recursive(vmdata, vmdata.accumulator, inst->root);
		++vmdata.program_counter;
	}
	// 関数呼び出しだった場合
	else if (root_token.type == flight_token_type::FTK_FUNCTION)
	{
		// アキュムレータに情報を設定
		ret = flight_vm_func(vmdata, vmdata.accumulator, inst->root);
	}
	// 不明な処理
	else
	{
		vmdata.error_line_no = root_token.line_no;
		vmdata.error = flight_errors::error_unknown_token;
		vmdata.error_token = root_token;
		return flight_vm_status::VM_ERROR;
	}

	return ret;
}


//=============================================================================
// <global functions> :: flight_vm_exec
//=============================================================================
void flight_vm_exec(flight_vm_data& vmdata, const error_setter& setter)
{
	// 終了フラグをリセット
	vmdata.exit_var.value = "";
	vmdata.exit_var.type = flight_data_type::FDT_NONE;

	while (!vmdata.exit_flg)
	{
		// 既に実行終了しているか確認
		const auto& insts = *vmdata.instructions;
		if (vmdata.program_counter >= (int)insts.size())
		{
			// 全コードが終わってスタックレベルもブロックレベルも0じゃない場合はエラー
			if (vmdata.now_block_level != 0 || vmdata.now_stack_level != 0)
			{
				// 致命的なエラー
				setter(-1, flight_errors::fatal_error, "");
			}

			// 終了フラグをtrueに
			vmdata.exit_flg = true;
			break;
		}

		// 命令を実行(プログラムカウンターは内部で進める)
		const auto ret = flight_vm_exec_inner(vmdata);

		// エラーを設定
		if (vmdata.error != 0)
		{
			setter(vmdata.error_line_no, vmdata.error, vmdata.error_token.token_str.c_str());
			break;
		}

		// 命令が実行状態以外はbreakで抜ける
		if (ret != flight_vm_status::VM_EXEC) { break; }
	}
}