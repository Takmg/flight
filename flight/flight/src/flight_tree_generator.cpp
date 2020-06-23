//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//-----------------------------------------------------------------------------
// 
// ◆動作の流れ
//    flight_tree_generater --> flight_make_instruction -> flight_parser_var -> flight_parser_recursive
//                                                      -> flight_parser_xxx -> flight_make_instruction
//																			 -> flight_parser_xxx
//                                                      -> flight_parser_recursive -> flight_parser_recursive
//  
//    上図のようにflight_make_instructionから flight_paraser_xxxを呼び、
//    その中から再度 flight_make_instructionを呼ぶ。
//
//    ifブロックなどの命令にて {}大かっこブロックの先でさらにflight_make_instructionを呼ぶことで、
//    tree処理をうまく作れるようにしている。
//
//-----------------------------------------------------------------------------


//=============================================================================
// prototype
//=============================================================================
static void flight_make_instruction(flight_generate_data&, std::vector<flight_token>&);
static fl_ptr flight_parser_recursive(flight_generate_data&, std::vector<flight_token>&, const int);


//=============================================================================
// <static functions> :: flight_move_toplevel_block
//=============================================================================
static void flight_move_toplevel_block(
	flight_generate_data& gdata, std::vector<flight_token>& dest, std::vector<flight_token>& src)
{
	// スタートがブロックじゃない場合はエラー
	if (src[0].type != flight_token_type::FTK_BLOCK)
	{
		gdata.error_line_no = src[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = src[0];
		return;
	}

	int level = 0;

	// トークンサイズ分ループ
	int index = 0;
	for (; index < (int)src.size(); index++)
	{
		// "{"や"}"で階層を付けていく
		if (src[index].type == flight_token_type::FTK_BLOCK) { ++level; }
		else if (src[index].type == flight_token_type::FTK_ENDBLOCK) { --level; }

		// レベルが0の場合
		if (level == 0 && src[index].type == flight_token_type::FTK_ENDBLOCK) { break; }

		// レベルが0以下ならエラー
		if (level < 0)
		{
			gdata.error_line_no = src[0].line_no;
			gdata.error = flight_errors::error_not_found_closing_block;
			gdata.error_token = src[0];
			return;
		}
	}

	// start_indexがコンテナのサイズ以上なら中断
	if (index >= (int)src.size()) { return; }

	// 一時領域にデータを入れて、元のトークンから削除する。
	dest.insert(dest.begin(), src.begin(), src.begin() + index + 1); // 大かっこ終了を含む
	src.erase(src.begin(), src.begin() + index + 1);				 // 大かっこ終了の次からスタートとする
}


//=============================================================================
// <static functions> :: flight_has_invalid_ins_tokens
//=============================================================================
static const bool flight_has_invalid_ins_tokens(const std::vector<flight_token>& tokens)
{
	// forやwhileやifの括弧内で無効なトークンがある場合はtrueを返す
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		// 演算子だった場合はOK
		if (tokens[i].type == flight_token_type::FTK_OPERATION) { continue; }

		// 変数もOK
		if (tokens[i].type == flight_token_type::FTK_VARIABLE) { continue; }

		// 関数もOK
		if (tokens[i].type == flight_token_type::FTK_FUNCTION) { continue; }

		// セミコロンもOK
		if (tokens[i].type == flight_token_type::FTK_ENDINSTRUCTION) { continue; }

		// 小かっこもOK
		if (tokens[i].type == flight_token_type::FTK_PSES ||
			tokens[i].type == flight_token_type::FTK_ENDPSES) {
			continue;
		}

		// 予約語のうちvarもOK
		if (tokens[i].type == flight_token_type::FTK_INSTRUCTION_RESERVED &&
			tokens[i].token_str == "var") {
			continue;
		}

		// 右辺の予約語もOK
		if (tokens[i].type == flight_token_type::FTK_VALUE_RESERVED) { continue; }

		// 右辺値もOK
		if (tokens[i].type == flight_token_type::FTK_VALUE_NUMBER ||
			tokens[i].type == flight_token_type::FTK_VALUE_CHAR ||
			tokens[i].type == flight_token_type::FTK_VALUE_STR) {
			continue;
		}

		// ここまで来た場合は失敗
		return true;
	}

	return false;
}


//=============================================================================
// <static functions> :: flight_search_toplevel
//=============================================================================
static const int flight_search_toplevel(
	flight_generate_data& gdata, const std::vector<flight_token>& tokens,
	const flight_token_type type, const int start_index = 0)
{
	int level = 0;

	// トークンサイズ分ループ
	int index = start_index;
	for (; index < (int)tokens.size(); index++)
	{
		// "("や")"で階層を付けていく
		if (tokens[index].type == flight_token_type::FTK_PSES) { ++level; }
		else if (tokens[index].type == flight_token_type::FTK_ENDPSES) { --level; }

		// レベルが0の場合
		if (level == 0 && tokens[index].type == type) { break; }

		// レベルが0以下ならエラー
		if (level < 0)
		{
			gdata.error_line_no = tokens[index].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[index];
			return -1;
		}
	}

	// start_indexがコンテナのサイズ以上なら中断
	if (index >= (int)tokens.size()) { return -1; }

	return index;
}


//=============================================================================
// <static functions> :: flight_move_tokens
//=============================================================================
static void flight_move_tokens(
	flight_generate_data& gdata, std::vector<flight_token>& dest,
	std::vector<flight_token>& src, const flight_token_type type)
{
	// 指定したtypeの位置を検索
	const auto index = flight_search_toplevel(gdata, src, type);

	// typeの位置までコピー かつ 元データ から 削除
	std::vector<flight_token> tmp_tokens;
	if (index < 0)
	{
		gdata.error_line_no = src[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = src[0];
		return;
	}

	// 一時領域にデータを入れて、元のトークンから削除する。
	dest.insert(dest.begin(), src.begin(), src.begin() + index + 1); // 検索対象も含む
	src.erase(src.begin(), src.begin() + index + 1);
}


//=============================================================================
// <static functions> :: flight_make_gotolabel_node
//=============================================================================
static fl_ptr flight_make_gotolabel_node(
	const char* sig, const int sigid,
	const int countid, const bool next = true)
{
	// 変数宣言
	const std::string label = std::string(sig) + "-" + std::to_string(sigid) + "-" + std::to_string(countid);
	const flight_token_type type = next ? flight_token_type::FTK_GOTOLABEL : flight_token_type::FTK_GOTOBACKLABEL;

	// nodeを作成する
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { -1 , label , type };

	return ptr;
}


//=============================================================================
// <static functions> :: flight_make_instruction
//=============================================================================
static void flight_make_instruction(flight_generate_data& gdata, fl_ptr root)
{
	// 命令を追加
	fi_ptr inst = std::make_shared<flight_instruction>();
	inst->root = root;
	gdata.container.push_back(inst);
}


//=============================================================================
// <static functions> :: flight_make_gotolabel
//=============================================================================
static void flight_make_gotolabel(
	flight_generate_data& gdata,
	const char* sig, const int sigid,
	const int countid, const bool next = true)
{
	// 命令を作成する
	flight_make_instruction(gdata, flight_make_gotolabel_node(sig, sigid, countid, next));
}


//=============================================================================
// <static functions> :: flight_make_label
//=============================================================================
static void flight_make_label(flight_generate_data& gdata, const char* sig, const int sigid, const int countid)
{
	// ラベル名
	const std::string label = std::string(sig) + "-" + std::to_string(sigid) + "-" + std::to_string(countid);

	// ラベル命令を作成する
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { -1 , label , flight_token_type::FTK_LABEL };
	flight_make_instruction(gdata, ptr);
}


//=============================================================================
// <static functions> :: flight_insert_startblock
//=============================================================================
static void flight_insert_startblock(flight_generate_data& gdata, const int line_no)
{
	// 始まり括弧を作成する
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { line_no , "{" , flight_token_type::FTK_BLOCK };
	flight_make_instruction(gdata, ptr);
}


//=============================================================================
// <static functions> :: flight_insert_endblock
//=============================================================================
static void flight_insert_endblock(flight_generate_data& gdata, const int line_no)
{
	// 終わり括弧を作成する
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { line_no , "}" , flight_token_type::FTK_ENDBLOCK };
	flight_make_instruction(gdata, ptr);
}


//=============================================================================
// <static functions> :: flight_is_reserved
//=============================================================================
static int flight_is_reserved(const flight_token token, const flight_reserved_data data[], const int size)
{
	for (int i = 0; i < size; i++)
	{
		// リストの予約語命令と一致する場合、インデックスを返す
		const auto inst = data[i];
		if (token.token_str != inst.instruction) { continue; }

		return i;
	}

	return -1;
}


//=============================================================================
// <static functions> :: flight_lowp_operator
//=============================================================================
static int flight_lowp_operator(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// 初期値
	static const int initial_value = 0x7FFFFFFF;

	// 優先度
	int priority = initial_value;
	int target_index = -1;

	// トークン処理
	int now_index = -1;
	for (; now_index < (int)tokens.size();)
	{
		// 検索を行う
		const int search_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_OPERATION, now_index + 1);
		if (search_index < 0) { break; }

		// 検索インデックスを上書き
		now_index = search_index;

		// 優先度が低い演算子程、構文木の上にくる
		int lp = initial_value;
		if (tokens[now_index].token_str == "*=") { lp = 1; }
		else if (tokens[now_index].token_str == "/=") { lp = 1; }
		else if (tokens[now_index].token_str == "+=") { lp = 1; }
		else if (tokens[now_index].token_str == "-=") { lp = 1; }
		else if (tokens[now_index].token_str == "=") { lp = 1; }
		else if (tokens[now_index].token_str == "||") { lp = 2; }
		else if (tokens[now_index].token_str == "&&") { lp = 3; }
		else if (tokens[now_index].token_str == "==") { lp = 4; }
		else if (tokens[now_index].token_str == "!=") { lp = 4; }
		else if (tokens[now_index].token_str == "<") { lp = 5; }
		else if (tokens[now_index].token_str == "<=") { lp = 5; }
		else if (tokens[now_index].token_str == ">") { lp = 5; }
		else if (tokens[now_index].token_str == ">=") { lp = 5; }
		else if (tokens[now_index].token_str == "+") { lp = 6; }
		else if (tokens[now_index].token_str == "-") { lp = 6; }
		else if (tokens[now_index].token_str == "*") { lp = 7; }
		else if (tokens[now_index].token_str == "/") { lp = 7; }
		else if (tokens[now_index].token_str == "!") { lp = 8; }

		// 優先度をチェック
		if (lp < priority)
		{
			priority = lp;
			target_index = now_index;
		}
	}

	return target_index;
}


//=============================================================================
// <static functions> :: flight_parser_call_func
//=============================================================================
static fl_ptr flight_parser_call_func(
	flight_generate_data& gdata, std::vector<flight_token>& tokens,
	const int recursive_level)
{
	// トークンを設定
	fl_ptr call_root = std::make_shared<flight_token_node>();
	call_root->token = tokens[0];
	call_root->token.type = flight_token_type::FTK_FUNCTION;
	std::vector<flight_token> token_l(tokens.begin() + 1, tokens.end());
	call_root->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);

	// 変数名
	const std::string var_name = std::string("___TMP_VARIABLE___!") + std::to_string(gdata.sigunature++);

	// トークン作成
	flight_token semicolon_token;
	semicolon_token.line_no = tokens[0].line_no;
	semicolon_token.type = flight_token_type::FTK_ENDINSTRUCTION;
	semicolon_token.token_str = ";";

	flight_token var_token;
	var_token.line_no = tokens[0].line_no;
	var_token.type = flight_token_type::FTK_VARIABLE;
	var_token.token_str = var_name;

	// トップノードじゃない場合変数を追加する
	if (recursive_level > 0)
	{
		// 変数の追加
		std::vector<flight_token> make_vars;
		make_vars.resize(3);
		make_vars[0].line_no = tokens[0].line_no;
		make_vars[0].type = flight_token_type::FTK_INSTRUCTION_RESERVED;
		make_vars[0].token_str = "var";

		make_vars[1] = var_token;
		make_vars[2] = semicolon_token;

		flight_make_instruction(gdata, make_vars);
		if (gdata.error != flight_errors::error_none)
		{
			return nullptr;
		}
	}

	// 関数呼び出しの追加
	flight_make_instruction(gdata, call_root);

	// トップノードじゃない場合
	if (recursive_level > 0)
	{
		// 変数への代入を追加
		std::vector<flight_token> make_equal;
		make_equal.resize(4);
		make_equal[0] = var_token;

		make_equal[1].line_no = tokens[0].line_no;
		make_equal[1].type = flight_token_type::FTK_OPERATION;
		make_equal[1].token_str = "=";

		make_equal[2].line_no = tokens[0].line_no;
		make_equal[2].type = flight_token_type::FTK_ACCUMULATOR;
		make_equal[2].token_str = "___ACCUMULATOR___!";

		make_equal[3] = semicolon_token;

		flight_make_instruction(gdata, make_equal);
		if (gdata.error != flight_errors::error_none)
		{
			return nullptr;
		}

		// 変数の代入処理を追加
		fl_ptr ptr = std::make_shared<flight_token_node>();
		ptr->token = var_token;
		return ptr;
	}

	return nullptr;
}


//=============================================================================
// <static functions> :: flight_parser_recursive
//=============================================================================
static fl_ptr flight_parser_recursive(
	flight_generate_data& gdata, std::vector<flight_token>& tokens,
	const int recursive_level = 0)
{
	// トークンサイズがない場合やエラーが存在する場合はエラー
	if (tokens.size() <= 0 || gdata.error != 0) { return nullptr; }

	// もし予約語だった場合は下に行く
	if (tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		// 予約語が再帰処理中に出現することはない
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return nullptr;
	}

	// 大かっこ開始や終了はブロック開始
	if (tokens[0].type == flight_token_type::FTK_BLOCK || tokens[0].type == flight_token_type::FTK_ENDBLOCK)
	{
		// エラー 大かっこが再帰処理中に出現することはない
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return nullptr;
	}

	// トークンの最初と最後が"("と ")"の場合、省く
	if (tokens.begin()->type == flight_token_type::FTK_PSES &&
		tokens.rbegin()->type == flight_token_type::FTK_ENDPSES)
	{
		tokens.erase(tokens.begin());
		tokens.erase(tokens.end() - 1);
	}

	// トークンがない場合は何もしない
	if (tokens.size() <= 0) { return nullptr; }

	// もしトークンが1つだけだった場合
	if (tokens.size() == 1)
	{
		// トークンを設定
		fl_ptr node = std::make_shared<flight_token_node>();
		node->token = tokens[0];
		return node;
	}

	// 演算子の処理 
	// var a = ( 3 + 4 ) * 2 , b = ( 2 * 4 ) + 3; みたいな処理に対応できるように
	// まず括弧外から検索する。
	const int op_index = flight_lowp_operator(gdata, tokens);
	const int comma_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_SEPARATOR);

	// ターゲットインデックスを取得
	int target_index = -1;
	if (op_index >= 0) { target_index = op_index; }
	if (comma_index >= 0) { target_index = comma_index; }

	// トップレベルでトークンが出てきた場合
	if (target_index >= 0)
	{
		// トークンを設定
		fl_ptr node = std::make_shared<flight_token_node>();
		node->token = tokens[target_index];

		// ！の場合は特殊(leftのみにデータを設定する)
		if (node->token.token_str == "!")
		{
			// 中途半端な位置に！マークが出てきた場合はエラー
			if (target_index != 0)
			{
				// エラー 中途半端な位置にビックリマークが出てきた
				gdata.error_line_no = tokens[target_index].line_no;
				gdata.error = flight_errors::error_syntax;
				gdata.error_token = tokens[target_index];
				return nullptr;
			}

			// データの設定
			std::vector<flight_token> token_l(tokens.begin() + 1, tokens.end());
			node->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);
		}
		else
		{
			// 分割処理を行う
			std::vector<flight_token> token_l(tokens.begin(), tokens.begin() + target_index);
			std::vector<flight_token> token_r(tokens.begin() + target_index + 1, tokens.end());
			node->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);
			node->right = flight_parser_recursive(gdata, token_r, recursive_level + 1);

			// もしトークンが代入である場合 かつ 左辺が変数以外だった場合はエラー
			const std::string op_str = node->token.token_str;
			if ((op_str == "=" || op_str == "+=" || op_str == "-=" || op_str == "*=" || op_str == "/=") &&
				(node->left == nullptr || node->left->token.type != flight_token_type::FTK_VARIABLE))
			{
				gdata.error = flight_errors::error_syntax;
				if (node->left == nullptr)
				{
					gdata.error_line_no = node->token.line_no;
					gdata.error_token = node->token;
				}
				else
				{
					gdata.error_line_no = node->left->token.line_no;
					gdata.error_token = node->left->token;
				}
				return nullptr;
			}
		}
		return node;
	}
	// トップレベルで符号が出てこなかった場合
	else
	{
		// 関数の場合
		if (tokens[0].type == flight_token_type::FTK_VARIABLE &&
			tokens[1].type == flight_token_type::FTK_PSES &&
			tokens.rbegin()->type == flight_token_type::FTK_ENDPSES)
		{
			return flight_parser_call_func(gdata, tokens, recursive_level);
		}
	}

	// ここまで来た場合は無効なトークンが登場した(インデックスを0じゃなく1つ目に指定)
	gdata.error_line_no = tokens[1].line_no;
	gdata.error = flight_errors::error_syntax;
	gdata.error_token = tokens[1];

	return nullptr;
}


//=============================================================================
// <global functions> :: flight_parser_func
//=============================================================================
void flight_parser_func(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// func文の確認
	if (tokens.size() < 5 || tokens[0].token_str != "func" || tokens[1].type != FTK_VARIABLE)
	{
		// エラー発生
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// 識別子を取得
	const int signature = gdata.sigunature++;

	// 先頭要素の削除(func文の削除)
	const auto func_token = tokens[0];
	const auto name_token = tokens[1];
	tokens.erase(tokens.begin(), tokens.begin() + 2);

	// PSES
	if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
	{
		// エラー funcの後は括弧が必要
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// funcnameノードの作成
	auto func_name = std::make_shared<flight_token_node>();
	func_name->token = name_token;

	// funcノードの作成
	auto func_root = std::make_shared<flight_token_node>();
	func_root->token = func_token;
	func_root->left = func_name;
	func_root->right = nullptr;

	// ()の間を取得する
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// ()を削除する
	tmp_tokens.erase(tmp_tokens.begin());
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// 引数からvarを除く
	int now_index = 0;
	std::vector<flight_token> arg_tokens = tmp_tokens;
	while (now_index < (int)arg_tokens.size())
	{
		if (arg_tokens[now_index].type == flight_token_type::FTK_INSTRUCTION_RESERVED &&
			arg_tokens[now_index].token_str == "var")
		{
			arg_tokens.erase(arg_tokens.begin() + now_index);
			continue;
		}

		// 次のインデックスへ遷移
		++now_index;
	}

	// 引数リストのチェック
	for (int i = 0; i < (int)arg_tokens.size(); i++)
	{
		// 変数 または カンマ以外だった場合エラーとなる
		if (!(arg_tokens[i].type == FTK_VARIABLE || arg_tokens[i].type == FTK_SEPARATOR))
		{
			gdata.error_line_no = arg_tokens[i].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = arg_tokens[i];
			return;
		}
	}

	// 引数リストをノードに設定
	func_root->right = flight_parser_recursive(gdata, arg_tokens);

	// 関数前に関数終わりまでジャンプする処理を追加する
	flight_make_gotolabel(gdata, (name_token.token_str + "-exit").c_str(), signature, 0);

	// 関数の開始処理を追加
	flight_make_instruction(gdata, func_root);

	// 開始ブロックを追加する。
	flight_insert_startblock(gdata, tokens[0].line_no);

	// 大かっこを取得
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// 大かっこ内の処理を作成
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// アキュムレータのリセット処理を追加
	auto reset_root = std::make_shared<flight_token_node>();
	reset_root->token.line_no = -1;
	reset_root->token.token_str = "__RESET_ACCUM__!";
	reset_root->token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;
	reset_root->left = nullptr;
	reset_root->right = nullptr;
	flight_make_instruction(gdata, reset_root);

	// ラベル処理を作成
	flight_make_label(gdata, "func-end", signature, 0);

	// 終了ブロックを追加する。
	flight_insert_endblock(gdata, -1);

	// 関数ブロック終わりのラベルを指定
	flight_make_label(gdata, (name_token.token_str + "-exit").c_str(), signature, 0);
}


//=============================================================================
// <static functions> :: flight_parser_while
//=============================================================================
void flight_parser_while(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// while文の確認
	if (tokens.size() < 5 || tokens[0].token_str != "while" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// 開始ブロックを追加する。
	flight_insert_startblock(gdata, tokens[0].line_no);

	// 識別子を取得
	const int signature = gdata.sigunature++;

	// 先頭要素の削除(while文の削除)
	const auto while_token = tokens[0];
	tokens.erase(tokens.begin());

	// PSES
	if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// ()の間を取得する
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// トークンを取得
	std::vector<flight_token> cond_tokens = tmp_tokens;
	if (flight_has_invalid_ins_tokens(cond_tokens))
	{
		// エラー 無効なトークンがあった
		gdata.error_line_no = cond_tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = cond_tokens[0];
		return;
	}

	// ラベルの設定
	flight_make_label(gdata, "loop-start", signature, 0);

	// ループの解釈
	auto cond_node = flight_parser_recursive(gdata, cond_tokens);
	if (gdata.error != 0) { return; }

	// ifトークンの作成
	flight_token if_token;
	if_token.line_no = while_token.line_no;
	if_token.token_str = "if";
	if_token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;

	// ループの処理を作成(偽条件ならloop_endまでジャンプ)
	auto cond_root = std::make_shared<flight_token_node>();
	cond_root->token = if_token;
	cond_root->left = cond_node;
	cond_root->right = flight_make_gotolabel_node("loop-end", signature, 0);
	flight_make_instruction(gdata, cond_root);

	// 大かっこを取得
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// 大かっこ内の処理を作成
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ラベルの設定
	flight_make_label(gdata, "loop-continue", signature, 0);

	// 後ろに移動する
	flight_make_gotolabel(gdata, "loop-start", signature, 0, false);

	// ラベル処理を作成
	flight_make_label(gdata, "loop-end", signature, 0);

	// 終了ブロックを追加する。
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <static functions> :: flight_parser_for
//=============================================================================
void flight_parser_for(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// for文の確認
	if (tokens.size() < 5 || tokens[0].token_str != "for" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// 開始ブロックを追加する。
	flight_insert_startblock(gdata, tokens[0].line_no);

	// 識別子を取得
	const int signature = gdata.sigunature++;

	// 先頭要素の削除(for文の削除)
	const auto for_token = tokens[0];
	tokens.erase(tokens.begin());

	// PSES
	if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
	{
		// エラー forの後は括弧が必要
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// ()の間を取得する
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// ()を削除する
	tmp_tokens.erase(tmp_tokens.begin());
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// 初期値・ループ・カウンターのトークンを取得
	std::vector<flight_token> initial_tokens, condition_token, counter_token;
	flight_move_tokens(gdata, initial_tokens, tmp_tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }
	flight_move_tokens(gdata, condition_token, tmp_tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// 最後にセミコロンを付ける
	counter_token = tmp_tokens;
	counter_token.push_back(*condition_token.rbegin());

	// 無効なトークンか確認
	{
		std::vector<std::vector<flight_token>*> token_ary;
		token_ary.push_back(&initial_tokens);
		token_ary.push_back(&condition_token);
		token_ary.push_back(&counter_token);

		for (int i = 0; i < (int)token_ary.size(); i++)
		{
			const auto& v = *(token_ary[i]);
			if (flight_has_invalid_ins_tokens(v))
			{
				// エラー 無効なトークンがあった
				gdata.error_line_no = v[0].line_no;
				gdata.error = flight_errors::error_syntax;
				gdata.error_token = v[0];
				return;
			}
		}
	}

	// 初期値の解釈
	while (initial_tokens.size() > 0)
	{
		flight_make_instruction(gdata, initial_tokens);
		if (gdata.error != 0) { return; }
	}

	// ラベルの設定
	flight_make_label(gdata, "loop-start", signature, 0);

	// ループの解釈
	condition_token.erase(condition_token.end() - 1);
	auto node = flight_parser_recursive(gdata, condition_token);
	if (gdata.error != 0) { return; }

	// ifトークンの作成
	flight_token if_token;
	if_token.line_no = for_token.line_no;
	if_token.token_str = "if";
	if_token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;

	// ループの処理を作成(偽条件ならloop_endまでジャンプ)
	auto cond_root = std::make_shared<flight_token_node>();
	cond_root->token = if_token;
	cond_root->left = node;
	cond_root->right = flight_make_gotolabel_node("loop-end", signature, 0);
	flight_make_instruction(gdata, cond_root);

	// 大かっこを取得
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// 大かっこ内の処理を作成
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ラベルの設定
	flight_make_label(gdata, "loop-continue", signature, 0);

	// カウンターの解釈
	while (counter_token.size() > 0)
	{
		flight_make_instruction(gdata, counter_token);
		if (gdata.error != 0) { return; }
	}

	// 後ろに移動する
	flight_make_gotolabel(gdata, "loop-start", signature, 0, false);

	// ラベル処理を作成
	flight_make_label(gdata, "loop-end", signature, 0);

	// 終了ブロックを追加する。
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <static functions> :: flight_parser_if_inner
//=============================================================================
static void flight_parser_if_inner(
	flight_generate_data& gdata, std::vector<flight_token>& tokens,
	const int signature, const int count)
{
	// トークンの一時ファイルを追加
	std::vector<flight_token> tmp_tokens;

	// ifブロックの場合は()が含まれる為、条件を取得する
	if (tokens[0].token_str == "if")
	{
		// ifトークンの取得
		const auto if_token = tokens[0];

		// ifキーワードを外す
		tokens.erase(tokens.begin());

		// PSES
		if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
		{
			// エラー forの後は括弧が必要
			gdata.error_line_no = tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[0];
			return;
		}

		// ()の間を取得する
		flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);

		// 無効なトークンか確認
		if (flight_has_invalid_ins_tokens(tmp_tokens))
		{
			// エラー 無効なトークンがあった
			gdata.error_line_no = tmp_tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tmp_tokens[0];
			return;
		}

		// ifの括弧内で変数宣言がされている場合、命令を作成する
		if (tmp_tokens[1].token_str == "var")
		{
			flight_make_instruction(gdata, tmp_tokens);
		}

		// 分岐条件
		auto condition = flight_parser_recursive(gdata, tmp_tokens);
		auto else_ins = flight_make_gotolabel_node("if", signature, count);

		// if用のnodeを作成する
		fl_ptr if_node = std::make_shared<flight_token_node>();
		if_node->token = if_token;
		if_node->left = condition;
		if_node->right = else_ins;

		// 命令を追加
		flight_make_instruction(gdata, if_node);
	}

	// 大かっこの内容取得
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// 大かっこ内の処理を作成
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ラベルジャンプ処理を作成
	flight_make_gotolabel(gdata, "if-end", signature, 0);
	flight_make_label(gdata, "if", signature, count);
}


//=============================================================================
// <global functions> :: flight_parser_if
//=============================================================================
void flight_parser_if(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// if文の確認(メッセージの最初がelseの場合、エラー)
	if (tokens.size() < 5 || tokens[0].token_str != "if" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// 開始ブロックを追加する。
	flight_insert_startblock(gdata, tokens[0].line_no);

	// 識別子を取得
	const int signature = gdata.sigunature++;
	int count = 0;

	// ifブロックのループ処理
	while (true)
	{
		// 内部的なif処理
		flight_parser_if_inner(gdata, tokens, signature, count++);

		// エラーが発生している場合も抜ける
		if (gdata.error != 0) { return; }

		// トークンが終了している場合は抜ける
		if (tokens.size() <= 0) { break; }

		// ifブロック 又は else ブロックでない場合はbreak
		if (!(tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED && tokens[0].token_str == "else")) { break; }

		// ここまでくる場合はelseの場合
		tokens.erase(tokens.begin());
	}

	// ラベル処理を作成
	flight_make_label(gdata, "if-end", signature, 0);

	// 終了ブロックを追加する。
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <global functions> :: flight_parser_var
//=============================================================================
void flight_parser_var(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// varトークンのバックアップ
	flight_token var_token = tokens[0];

	// 先頭要素の削除
	tokens.erase(tokens.begin());

	// カンマ区切り
	while (true)
	{
		// 変数とセミコロンを合わせて少なくとも2以上のトークンが存在しない場合はエラー
		if (tokens.size() < 2)
		{
			// エラー forの後は括弧が必要
			gdata.error_line_no = var_token.line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = var_token;
			return;
		}

		// トークン代入後、トークンを削除
		auto node = std::make_shared<flight_token_node>();
		node->token = var_token;
		node->left = std::make_shared<flight_token_node>();
		node->left->token = tokens[0];

		// varだけの変数宣言命令を作成
		fi_ptr inst = std::make_shared<flight_instruction>();
		inst->root = node;
		gdata.container.push_back(inst);

		// 次のカンマ区切りとセミコロンインデックスを取得
		const int semi_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_ENDINSTRUCTION);
		const int comma_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_SEPARATOR);

		// 最小のインデックスを取得する
		int next_index = -1;
		if (comma_index >= 0) { next_index = comma_index; }
		if (semi_index >= 0) { next_index = semi_index < next_index || comma_index < 0 ? semi_index : next_index; }

		// セミコロンかカンマが必ず出現するはずなのに、でない場合はエラー
		if (next_index < 0)
		{
			// エラー forの後は括弧が必要
			gdata.error_line_no = tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[0];
			return;
		}

		// セミコロンまで取得
		std::vector<flight_token> tmp_tokens;
		tmp_tokens.insert(tmp_tokens.end(), tokens.begin(), tokens.begin() + next_index + 1);
		tokens.erase(tokens.begin(), tokens.begin() + next_index + 1);

		// カンマやセミコロンのインデックスが1以上なら演算子処理を行う
		if (next_index >= 2)
		{
			// 最後がカンマだった場合は削除を行う
			if (tmp_tokens.rbegin()->type == flight_token_type::FTK_SEPARATOR ||
				tmp_tokens.rbegin()->type == flight_token_type::FTK_ENDINSTRUCTION)
			{
				tmp_tokens.erase(tmp_tokens.end() - 1);
			}

			// 変数宣言と同時に行う代入処理を解決
			fi_ptr op = std::make_shared<flight_instruction>();
			op->root = flight_parser_recursive(gdata, tmp_tokens);

			// エラーがある場合は即終了
			if (gdata.error != 0) { return; }

			if (op->root->left != nullptr || op->root->right != nullptr)
			{
				gdata.container.push_back(op);
			}
		}

		// トークンサイズが0以下 又は 次がセミコロンだった場合は抜ける
		if (tokens.size() <= 0 || comma_index < 0 || comma_index > semi_index)
		{
			break;
		}
	}
}


//=============================================================================
// <global functions> :: flight_parser_ret
//=============================================================================
void flight_parser_ret(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// セミコロンまでを取得
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// 最後のセミコロンを削除
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// キーワードを設定
	auto node = std::make_shared<flight_token_node>();
	node->token = tmp_tokens[0];

	// 先頭を削除
	tmp_tokens.erase(tmp_tokens.begin());

	// 一時トークンが存在する場合はleftに設定
	if (tmp_tokens.size() > 0)
	{
		node->left = flight_parser_recursive(gdata, tmp_tokens, 1);
	}

	// 命令作成
	flight_make_instruction(gdata, node);
}


//=============================================================================
// <global functions> :: flight_parser_simple
//=============================================================================
void flight_parser_simple(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// セミコロンまでを取得
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// 1つ目がセミコロンでない場合はエラー
	if (tmp_tokens[1].type != flight_token_type::FTK_ENDINSTRUCTION)
	{
		gdata.error_line_no = tmp_tokens[1].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tmp_tokens[1];
		return;
	}

	// 命令作成
	auto node = std::make_shared<flight_token_node>();
	node->token = tmp_tokens[0];
	flight_make_instruction(gdata, node);
}


//=============================================================================
// <static functions> :: flight_parser_error
//=============================================================================
void flight_parser_error(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	gdata.error_line_no = tokens[1].line_no;
	gdata.error = flight_errors::error_syntax;
	gdata.error_token = tokens[1];
}


//=============================================================================
// <static functions> :: flight_make_instruction
//=============================================================================
static void flight_make_instruction(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// トークンが無くなったら終了
	if (tokens.size() <= 0) { return; }

	// もし予約語だった場合は下に行く
	if (tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		// インデックスを取得
		const int index = flight_is_reserved(tokens[0], _token_reserved_instruction_list, _token_reserved_instruction_list_size);

		// インデックスが0以上の場合
		if (index >= 0)
		{
			// トークン毎の処理に遷移
			_token_reserved_instruction_list[index].parser(gdata, tokens);
			return;
		}
	}

	// 大かっこ開始や終了はブロック開始
	if (tokens[0].type == flight_token_type::FTK_BLOCK || tokens[0].type == flight_token_type::FTK_ENDBLOCK)
	{
		// トークン代入後、トークンを削除
		auto node = std::make_shared<flight_token_node>();
		node->token = tokens[0];
		node->left = node->right = nullptr;
		tokens.erase(tokens.begin());

		// ルートを設定しコンテナに追加
		flight_make_instruction(gdata, node);
		return;
	}

	// ここまでくる場合は何かしら命令があるパターン
	// 先頭が予約語の場合や大かっこの場合を除いた、
	// 関数呼び出しや演算子を用いた計算を以下で行う。

	// セミコロンの位置まで削除
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (tmp_tokens.size() > 0 && tmp_tokens.rbegin()->type == flight_token_type::FTK_ENDINSTRUCTION) {
		tmp_tokens.erase(tmp_tokens.end() - 1);
	}

	// エラーがある場合や処理が何もない場合は即終了
	if (gdata.error != 0 || tmp_tokens.size() <= 0) { return; }

	// rootを設定
	fi_ptr inst = std::make_shared<flight_instruction>();
	inst->root = flight_parser_recursive(gdata, tmp_tokens, 0);

	// rootが変数だけだった場合何もしない
	// ※関数の戻り値をセットする変数が返ってきただけの可能性があるため。
	if (inst->root == nullptr) { return; }

	gdata.container.push_back(inst);
}


//=============================================================================
// <global functions> :: flight_tree_generater [syntax treeを作成する]
//=============================================================================
void flight_tree_generater(fi_container& container, int& signature, std::vector<flight_token>& tokens, const error_setter& setter)
{
#ifdef _DEBUG
	printf("tokens------------------------------------------------------------\n");
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		printf("%s ", tokens[i].token_str.c_str());
	}
	printf("------------------------------------------------------------------\n\n");
#endif 
	// 初期データの作成
	flight_generate_data gdata;
	gdata.error = flight_errors::error_none;
	gdata.error_line_no = -1;
	gdata.sigunature = signature;

	// 空命令の追加
	flight_make_label(gdata, "program-start", gdata.sigunature++, 0);

	// 命令の追加
	while (tokens.size() > 0)
	{
		// 命令を作成
		flight_make_instruction(gdata, tokens);

		// エラーが発生している場合は中断する
		if (gdata.error != 0)
		{
			setter(gdata.error_line_no, gdata.error, "");
			return;
		}
	}

	// 空命令の追加
	flight_make_label(gdata, "program-end", gdata.sigunature++, 0);

	// 親コンテナに結合させる
	container.insert(container.end(), gdata.container.begin(), gdata.container.end());

	// 親データに値を返す
	signature = gdata.sigunature;
}
