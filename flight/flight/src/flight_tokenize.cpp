//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <struct> :: token_info
//=============================================================================
typedef struct __token_info {
	int line_no;			// 行番号
	flight_errors error;	// エラー
} token_info;


//=============================================================================
// typedefs
//=============================================================================
typedef int (*token_divider_func)(const char*, token_info&);
typedef std::string(*token_modify_func)(char*, token_info&);


//=============================================================================
// <structs> :: TOKEN_DIVIDER [トークン分割用構造体]
//=============================================================================
typedef struct _TOKEN_DIVIDER {
	flight_token_type token_type;
	token_divider_func divider_func;
	token_modify_func modify_func;
} TOKEN_DIVIDER;


//=============================================================================
// <static functions> :: token_endline
//=============================================================================
static int token_endline(const char* raw_data, token_info& info)
{
	int pos = 0;

	// 改行コードのいずれかがマッチしていた場合はポジションを進める。
	if (strncmp(raw_data, "\r\n", 2) == 0) { pos = 2; }
	else if (*raw_data == '\r') { pos = 1; }
	else if (*raw_data == '\n') { pos = 1; }

	// 進めるポジションが2以上なら次へ
	if (pos > 0) { ++info.line_no; }

	return pos;
}


//=============================================================================
// <static functions> :: token_reserved_list
//=============================================================================
static int token_reserved_list(const char* raw_data, token_info& info, const flight_reserved_data list[], const int list_size)
{
	const int str_size = (int)strlen(raw_data);
	for (int i = 0; i < list_size; i++)
	{
		// 残文字列の長さ以上に予約語の言葉が長かった場合は比較しない。
		const int reserved_len = (int)strlen(list[i].instruction);
		if (reserved_len > str_size) { continue; }

		// 比較した結果一致している場合は予約語として登録する。
		if (strncmp(raw_data, list[i].instruction, reserved_len) == 0 &&
			!isalnum(raw_data[reserved_len]) && raw_data[reserved_len] != '_')
		{
			return reserved_len;
		}
	}

	return 0;
}


//=============================================================================
// <static functions> :: token_reserved_instruction
//=============================================================================
static int token_reserved_instruction(const char* raw_data, token_info& info)
{
	const int size = _token_reserved_instruction_list_size;
	return token_reserved_list(raw_data, info, _token_reserved_instruction_list, size);
}


//=============================================================================
// <static functions> :: token_reserved_value
//=============================================================================
static int token_reserved_value(const char* raw_data, token_info& info)
{
	const int size = _token_reserved_value_list_size;
	return token_reserved_list(raw_data, info, _token_reserved_value_list, size);
}


//=============================================================================
// <static functions> :: token_value_number
//=============================================================================
static int token_value_number(const char* raw_data, token_info& info)
{
	// 数値じゃない場合
	if (!isdigit(*raw_data)) { return 0; }

	// 検索ポジション
	int pos = 1;		// 次の文字からデータを取得
	if (strncmp(raw_data, "0x", 2) == 0) { ++pos; }

	// 全行を設定
	for (; *(raw_data + pos); pos++)
	{
		// 数値 又は 小数点じゃない場合、そこで終了
		const char d = raw_data[pos];

		// 数値 及び 小数点 及び 英字以外ならcontinue
		if (isalnum(d) || d == '.') { continue; }

		// ここまでくる場合はエラー
		break;
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_variable
//=============================================================================
static int token_variable(const char* raw_data, token_info& info)
{
	// 英字又はアンダーバースタートでない場合
	if (!isalpha(*raw_data) && raw_data[0] != '_') { return 0; }

	// 検索ポジション
	int pos = 1;		// 次の文字からデータを取得

	// 全行を設定
	for (; *(raw_data + pos); pos++)
	{
		// 数値 又は 小数点じゃない場合、そこで終了
		const char d = raw_data[pos];
		if (!isalnum(d) && d != '_') { break; }
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_value_char
//=============================================================================
static int token_value_char(const char* raw_data, token_info& info)
{
	// シングルクォートスタートでない場合
	if (*raw_data != '\'') { return 0; }

	// 検索ポジション
	int pos = 1;		// 次の文字からデータを取得

	// 全行を設定
	for (; *(raw_data + pos); )
	{
		// 途中で改行が挟まっているか？
		const int lpos = token_endline(raw_data + pos, info);
		if (lpos > 0) { pos += lpos; continue; }

		// "\'"の場合は文字列内の文字列として何もしない
		if (strncmp(raw_data + pos, "\\\'", 2) == 0) { ++pos; }

		// ポジションを移動
		++pos;

		// シングルクォートが出た場合は終了
		if (raw_data[pos] == '\'') { break; }
	}

	// もし最終行がシングルクォートだった場合
	if (raw_data[pos] == '\'') { ++pos; }

	// 最終行がシングルクォートでない場合はエラーとなる。
	else
	{
		info.error = flight_errors::error_notendchar;
		return -1;
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_value_str
//=============================================================================
static int token_value_str(const char* raw_data, token_info& info)
{
	// ダブルクォートスタートでない場合
	if (*raw_data != '"') { return 0; }

	// 検索ポジション
	int pos = 1;		// 次の文字からデータを取得

	// 全行を設定
	for (; raw_data[pos]; )
	{
		// 途中で改行が挟まっているか？
		const int lpos = token_endline(raw_data + pos, info);
		if (lpos > 0) { pos += lpos; continue; }

		// "\""の場合は文字列内の文字列として何もしない
		if (strncmp(raw_data + pos, "\\\"", 2) == 0) { ++pos; }

		// ポジションを移動
		++pos;

		// ダブルクォートが出た場合は終了
		if (raw_data[pos] == '"') { break; }
	}

	// もし最終行がダブルクォートだった場合
	if (raw_data[pos] == '"') { ++pos; }

	// 最終行がダブルクォートでない場合はエラーとなる。
	else
	{
		info.error = flight_errors::error_notendstring;
		return -1;
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_notcode
//=============================================================================
static int token_notcode(const char* raw_data, token_info& info)
{
	// 
	int pos = 0;

	// 全行を設定
	for (; raw_data[pos]; pos++)
	{
		// スペースやタブだった場合ループ続行
		const char d = raw_data[pos];
		if (!(d == ' ' || d == '\t')) { break; }
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_comment1
//=============================================================================
static int token_comment1(const char* raw_data, token_info& info)
{
	// "//"のコメントの場合
	if (strncmp(raw_data, "//", 2) != 0) { return 0; }

	// 検索ポジション
	int pos = 2;		// 次の文字からデータを取得

	// 全行を設定
	for (; raw_data[pos]; pos++)
	{
		// 行末だった場合終了
		const char d = raw_data[pos];
		if (d == '\0' || d == '\n') { break; }
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_comment2
//=============================================================================
static int token_comment2(const char* raw_data, token_info& info)
{
	// "/*"のコメントの場合
	if (strncmp(raw_data, "/*", 2) != 0) { return 0; }

	// 検索ポジション
	int pos = 2;		// 次の文字からデータを取得

	// 全行を設定
	for (; raw_data[pos]; pos++)
	{
		// コメントの終了前に文末が出てきた場合終了
		if (raw_data[pos] == '\0')
		{
			return (int)flight_errors::error_endcomment;
		}

		// 終了コメント
		if (strncmp(raw_data + pos, "*/", 2) == 0) { pos += 2; break; }
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_operator
//=============================================================================
static int token_operator(const char* raw_data, token_info& info)
{
	// 演算子のチェック

	// 検索ポジション
	int pos = 0;		// 次の文字からデータを取得

	// 全行を設定
	for (; raw_data[pos]; pos++)
	{
		// 文字が記号じゃない場合は抜ける
		if (strchr(_token_operator_list, raw_data[pos]) == nullptr)
		{
			break;
		}
	}

	// 戻り値を返す
	return pos;
}


//=============================================================================
// <static functions> :: token_separator
//=============================================================================
static int token_separator(const char* raw_data, token_info& info) { return *raw_data == ',' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_parentheses
//=============================================================================
static int token_parentheses(const char* raw_data, token_info& info) { return *raw_data == '(' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_endparentheses
//=============================================================================
static int token_endparentheses(const char* raw_data, token_info& info) { return *raw_data == ')' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_block
//=============================================================================
static int token_block(const char* raw_data, token_info& info) { return *raw_data == '{' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_endblock
//=============================================================================
static int token_endblock(const char* raw_data, token_info& info) { return *raw_data == '}' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_cend
//=============================================================================
static int token_cend(const char* raw_data, token_info& info) { return *raw_data == ';' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_comma
//=============================================================================
static int token_comma(const char* raw_data, token_info& info) { return *raw_data == ',' ? 1 : 0; }


//=============================================================================
// <static functions> :: token_unknown
//=============================================================================
static int token_unknown(const char* raw_data, token_info& info)
{
	info.error = error_unknown_token;
	return -1;
}


//-----------------------------------------------------------------------------


//=============================================================================
// <static functions> :: token_nofix
//=============================================================================
static std::string token_nofix(char* token_str, token_info& info)
{
	return std::string(token_str);
}


//=============================================================================
// <static functions> :: token_modify_str
//=============================================================================
static std::string token_modify_str(char* token_str, token_info& info)
{
	static const char* escapes[][2] = {
		{ "\\n" , "\n" },
		{ "\\b" , "\b" },
		{ "\\t" , "\t" },
		{ "\\a" , "\a" },
		{ "\\\\", "\\" },
		{ "\\\"", "\"" },
		{ "\\'" , "'"  },
	};
	static const int escape_size = sizeof(escapes) / sizeof(escapes[0]);

	// 先頭と最後のダブルクォートを消す
	++token_str;
	token_str[strlen(token_str) - 1] = '\0';

	// エスケープ処理を施す
	std::string ret;
	while (*token_str)
	{
		// エスケープ処理
		int e = 0;
		for (; e < escape_size; e++)
		{
			// エスケープ文字列とマッチしていない
			if (strncmp(escapes[e][0], token_str, 2) != 0) { continue; }

			// エスケープ文字列変換
			ret += escapes[e][1];
			token_str += strlen(escapes[e][0]);
			break;
		}

		// 既にエスケープ処理を行っている場合はcontinue
		if (e != escape_size) { continue; }

		// 文字列に追加
		char character[2] = { 0 };
		character[0] = *token_str;
		ret += character;
		++token_str;
	}

	return ret;
}


//=============================================================================
// <static functions> :: token_modify_char
//=============================================================================
static std::string token_modify_char(char* token_str, token_info& info)
{
	// 文字の判定
	const auto ret = token_modify_str(token_str, info);

	// 文字の長さが1文字以外ならエラー
	if (ret.length() != 1)
	{
		info.error = error_toolongchar;
		return std::string();
	}

	// 文字列を数値に変換してから戻す。(文字は計算として使用可能)
	return std::to_string(ret[0]);
}


//=============================================================================
// <static functions> :: token_modify_number
//=============================================================================
static std::string token_modify_number(char* token_str, token_info& info)
{
	// 進数を設定する
	int number_type = 10;
	if (strncmp(token_str, "0x", 2) == 0) { number_type = 16; }
	else if (*token_str == '0') { number_type = 8; }

	// 頭の0x(16進数)や0(8進数)などの接頭語を省く
	token_str += number_type != 10 ? number_type / 8 : 0;

	// 文字列トークンの中から英字のものを小文字に変換する
	for (int i = 0; i < (int)strlen(token_str); i++)
	{
		if (isalpha(token_str[i]))
		{
			token_str[i] = tolower(token_str[i]);
		}
	}

	// もし10進数以外で小数点がある場合はエラー
	if (number_type != 10 && strchr(token_str, '.'))
	{
		info.error = error_invalidnumber;
		return std::string();
	}

	// もし小数点が2つ以上ある場合もエラー
	if (std::count_if(token_str, token_str + strlen(token_str), [](const auto& a) { return a == '.'; }) >= 2)
	{
		info.error = error_invalidnumber;
		return std::string();
	}

	// チェック文字列の必要な部分まで\0にする。
	char check_string[] = "0123456789abcdef";
	check_string[number_type] = '\0';
	for (int i = 0; i < (int)strlen(token_str); i++)
	{
		// 進数の文字列チェック(8進数などに8以上の値を使っているか？を確認)
		if (strchr(check_string, token_str[i]) == nullptr && token_str[i] != '.')
		{
			info.error = error_invalidnumber;
			return std::string();
		}
	}

	// 小数点が存在しない場合
	if (strchr(token_str, '.') == nullptr)
	{
		const int real_number = (int)strtol(token_str, nullptr, number_type);
		return std::to_string(real_number);
	}

	// 小数点が存在する場合
	return std::to_string(atof(token_str));

}


//-----------------------------------------------------------------------------



//=============================================================================
// constant variables
//=============================================================================
static const TOKEN_DIVIDER _token_divider[] =
{
	{ flight_token_type::FTK_NOCODE					, token_endline				, token_nofix },		// 行終了
	{ flight_token_type::FTK_NOCODE					, token_notcode				, token_nofix },		// スペースやタブなど
	{ flight_token_type::FTK_NOCODE					, token_comment1			, token_nofix },		// コメント
	{ flight_token_type::FTK_NOCODE					, token_comment2			, token_nofix },		// コメント
	{ flight_token_type::FTK_INSTRUCTION_RESERVED   , token_reserved_instruction, token_nofix },		// 予約語
	{ flight_token_type::FTK_VALUE_RESERVED			, token_reserved_value		, token_nofix },		// 予約語
	{ flight_token_type::FTK_PSES					, token_parentheses			, token_nofix },		// (
	{ flight_token_type::FTK_ENDPSES				, token_endparentheses		, token_nofix },		// )
	{ flight_token_type::FTK_BLOCK					, token_block				, token_nofix },		// {
	{ flight_token_type::FTK_ENDBLOCK				, token_endblock			, token_nofix },		// }
	{ flight_token_type::FTK_ENDINSTRUCTION			, token_cend				, token_nofix },		// ;
	{ flight_token_type::FTK_SEPARATOR				, token_separator			, token_nofix },		// +-*/%=
	{ flight_token_type::FTK_OPERATION				, token_operator			, token_nofix },		// +-*/%=
	{ flight_token_type::FTK_VALUE_NUMBER			, token_value_char			, token_modify_char },	// 文字
	{ flight_token_type::FTK_VALUE_STR				, token_value_str			, token_modify_str },	// 文字列
	{ flight_token_type::FTK_VALUE_NUMBER			, token_value_number		, token_modify_number },// 数値
	{ flight_token_type::FTK_VARIABLE				, token_variable			, token_nofix },		// 変数
	{ flight_token_type::FTK_UNKNOWN				, token_unknown				, token_nofix },		// 謎のトークン
};


//=============================================================================
// <static functions> :: flight_check_token_nest_recursive
//=============================================================================
static bool flight_check_token_nest_recursive(
	const std::vector<flight_token>& tokens, token_info& info, int& index)
{
	const int start_index = index > 0 ? index - 1 : index;

	for (; index < (int)tokens.size(); index++)
	{
		// 終わり括弧ならreturnで戻る
		if (tokens[index].type == flight_token_type::FTK_ENDBLOCK ||
			tokens[index].type == flight_token_type::FTK_ENDPSES)
		{
			return true;
		}

		// エラー
		bool error = false;

		// (始まりなら )を再起処理に持っていく
		if (tokens[index].type == flight_token_type::FTK_PSES)
		{
			// 再帰処理から失敗が返ってきた場合はfalseを返す
			if (!flight_check_token_nest_recursive(tokens, info, ++index)) { return false; }

			// この階層の始まりと異なる終わり括弧だった場合はfalseで返る
			if (tokens[index].type != flight_token_type::FTK_ENDPSES) { error = true; }
		}
		else if (tokens[index].type == flight_token_type::FTK_BLOCK)
		{
			// 再帰処理から失敗が返ってきた場合はfalseを返す
			if (!flight_check_token_nest_recursive(tokens, info, ++index)) { return false; }

			// この階層の始まりと異なる終わり括弧だった場合はfalseで返る
			if (tokens[index].type != flight_token_type::FTK_ENDBLOCK) { error = true; }
		}

		// 対応した終わり括弧がない
		if (error)
		{
			info.line_no = tokens[start_index].line_no;
			info.error = error_not_found_closing_block;
			return false;
		}
	}

	return true;
}


//=============================================================================
// <static functions> :: flight_check_token_nest
//=============================================================================
static bool flight_check_token_nest(const std::vector<flight_token>& tokens, token_info& info)
{
	int index = 0;
	if (!flight_check_token_nest_recursive(tokens, info, index)) { return false; }

	// 最後まで再帰で廻れた場合はtrueを返す
	return index == (int)tokens.size();
}


//=============================================================================
// <static functions> :: flight_check_function
//=============================================================================
static bool flight_check_function(std::vector<flight_token>& tokens, token_info& info)
{
	// 現在のブロックレベル
	int now_block_level = 0;

	// 変数情報を取得
	for (int now_index = 0; now_index < (int)tokens.size(); now_index++)
	{
		// tokenを指定
		const auto& token = tokens[now_index];

		// 関数だった場合でブロックレベルが0以外ならエラー
		if (token.type == flight_token_type::FTK_INSTRUCTION_RESERVED && token.token_str == "func")
		{
			if (now_block_level != 0)
			{
				info.line_no = token.line_no;
				info.error = flight_errors::error_function_notop;
				return false;
			}
		}

		// 現在のブロックレベルを修正
		if (token.type == flight_token_type::FTK_BLOCK) { ++now_block_level; }
		else if (token.type == flight_token_type::FTK_ENDBLOCK) { --now_block_level; }
	}

	return true;
}


//=============================================================================
// <global_functions> :: flight_tokenize
//=============================================================================
std::vector<flight_token> flight_tokenize(const std::string& raw_code, const error_setter& setter)
{
	// トークン
	std::vector<flight_token> ret;

	// コードを文字列にする。
	const std::string s = raw_code + "	    ";	// 余分な文字列を追加したデータを設定
	const char* raw_data = s.c_str();
	static const int max_token_size = 0x7FFF;

	// ファイル情報を設定
	token_info info;
	memset(&info, 0, sizeof(token_info));

	// 全行ループ
	while (*raw_data)
	{
		// 数値だった場合
		static const int divider_length = (int)(sizeof(_token_divider) / sizeof(_token_divider[0]));
		int divider_pos = 0;
		for (; divider_pos < divider_length; divider_pos++)
		{
			// 元情報を保存
			const auto origin_info = info;

			// dividerの取得
			const auto& divider = _token_divider[divider_pos];

			// トークン分割
			const int length = divider.divider_func(raw_data, info);
			if (length == 0) { continue; }

			// トークンの長さ
			if (length < 0)
			{
				setter(info.line_no, info.error, "");
				return std::vector<flight_token>();
			}

			// -------------------ここまでくる場合はトークンの長さが1以上-------------------
			if (length >= max_token_size)
			{
				setter(origin_info.line_no, flight_errors::error_tootokenlength, "");
				return std::vector<flight_token>();
			}

			// コメントじゃない場合はトークン保存
			if (_token_divider[divider_pos].token_type != flight_token_type::FTK_NOCODE)
			{
				// 文字列をコピー
				char mem[max_token_size];
				memset(mem, 0, sizeof(mem));
				memcpy(mem, raw_data, length);

				// トークン
				flight_token token;
				token.type = divider.token_type;
				token.token_str = divider.modify_func(mem, info);
				token.line_no = info.line_no;

				// エラーが発生した場合
				if (info.error != 0)
				{
					setter(info.line_no, info.error, "");
					return std::vector<flight_token>();
				}

				// トークン追加
				ret.push_back(token);
			}

			// 文字列加算
			raw_data += length;

			break;
		}

		// 最後までチェックがスルーだった場合、何もしない
		if (divider_pos == divider_length) { ++raw_data; }
	}

	// トークンの階層でエラーが発生していた場合、エラー
	if (!flight_check_token_nest(ret, info) || !flight_check_function(ret, info))
	{
		setter(info.line_no, info.error, "");
		return std::vector<flight_token>();
	}

	return ret;
}
