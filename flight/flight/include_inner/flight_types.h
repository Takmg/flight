#pragma once


//=============================================================================
// prototypes
//=============================================================================
struct flight_token_node;
struct flight_instruction;
struct flight_token;
struct flight_generate_data;


// -----------------------------------enums-----------------------------------


//=============================================================================
// <enum> :: flight_token_type
//=============================================================================
typedef enum __flight_token_type
{
	FTK_NONE = 0,				// 無し
	FTK_INSTRUCTION_RESERVED,	// 予約語
	FTK_SEPARATOR,				// 区切り(,)
	FTK_OPERATION,				// 演算子
	FTK_VARIABLE,				// 変数
	FTK_VALUE_RESERVED,			// 予約された値
	FTK_VALUE_CHAR,				// 文字
	FTK_VALUE_STR,				// 文字列
	FTK_VALUE_NUMBER,			// 数値
	FTK_PSES,					// (
	FTK_ENDPSES,				// )
	FTK_BLOCK,					// ブロック
	FTK_ENDBLOCK,				// ブロック終了
	FTK_ENDINSTRUCTION,			// 命令終了(;)
	FTK_NOCODE,					// コメントやスペース
	FTK_LF,						// 改行コード
	FTK_UNKNOWN,				// 謎のトークン

	// 以下はトークン分割で使用しない
	FTK_FUNCTION,				// 関数
	FTK_GOTOLABEL,				// ラベルへジャンプ
	FTK_GOTOBACKLABEL,			// 後ろのラベルへジャンプ
	FTK_LABEL,					// ジャンプされる場所
	FTK_ACCUMULATOR,			// アキュムレータ
} flight_token_type;


//=============================================================================
// <enum> :: __flight_vm_status
//=============================================================================
typedef enum __flight_vm_status
{
	VM_EXEC = 0,		// 実行
	VM_SUSPEND,			// 中断
	VM_ERROR,			// エラー
} flight_vm_status;


// ----------------------------------typedefs----------------------------------


//=============================================================================
// typedefs
//=============================================================================
typedef std::shared_ptr<flight_token_node> fl_ptr;
typedef std::shared_ptr<flight_instruction> fi_ptr;
typedef std::vector<fi_ptr> fi_container;


//=============================================================================
// typedefs (functions)
//=============================================================================
typedef std::function<void(const int, const flight_errors, const std::string)> error_setter;
typedef void(*flight_parser)(flight_generate_data&, std::vector<flight_token>&);


//-----------------------------------structs-----------------------------------


//=============================================================================
// <struct> :: flight_error_info
//=============================================================================
typedef struct __flight_error_info
{
	int line_no;
	flight_errors error;
	std::string error_token_str;
} flight_error_info;


//=============================================================================
// <struct> :: flight_reserved_data
//=============================================================================
struct flight_reserved_data
{
	const char* instruction;
	flight_parser parser;
};


//=============================================================================
// struct token
//=============================================================================
struct flight_token {
	int					line_no;	// トークンの行
	std::string			token_str;	// トークン内容
	flight_token_type	type;		// トークンタイプ
};


//=============================================================================
// <struct> :: flight_token_node
//=============================================================================
struct flight_token_node
{
	fl_ptr left;			// 構文
	fl_ptr right;			// 構文
	flight_token token;		// トークン
};


//=============================================================================
// <struct> :: flight_instruction
//=============================================================================
struct flight_instruction
{
	int address;	// アドレス
	fl_ptr root;	// 構文木のルート
};


//=============================================================================
// <struct> :: flight_generate_data
//=============================================================================
struct flight_generate_data
{
	fi_container container;		// 命令コンテナ
	int sigunature;				// ifやforの識別子
	int error_line_no;			// 発生したエラー行
	flight_errors error;		// 発生したエラー
	flight_token error_token;	// エラートークン
};


//=============================================================================
// <struct> :: flight_stack_variable
//=============================================================================
struct flight_stack_variable : public flight_variable
{
	int block_level;				// ブロックレベル ... 変数のブロックレベル
	int stack_level;				// スタックレベル ... 変数のスタックレベル。
};


//=============================================================================
// <struct> :: flight_arg
//=============================================================================
struct flight_arg
{
	std::string variable_name;		// 引数名
};


//=============================================================================
// <struct> :: flight_function_data
//=============================================================================
struct flight_function_data
{
	std::string function_name;			// 関数名
	int start_address;					// 開始アドレス
	std::vector<flight_arg> args;		// 引数情報
};


//=============================================================================
// <struct> :: flight_vm_data
//=============================================================================
struct flight_vm_data
{
	int program_counter;			// 現在の実行アドレス
	int stack_pointer;				// スタックポインター

	int now_block_level;			// ブロックレベル ... 大かっこの度にブロックレベルが増える。
									//                    大かっこ終了時にブロック内の変数を消す
	int now_stack_level;			// スタックレベル ... 関数呼び出しの度にスタックレベルが増える。
									//                    関数から戻る度に該当スタックの変数を削除する。

	bool exit_flg;					// 終了フラグ

	flight_variable accumulator;	// アキュムレータ
	flight_variable exit_var;		// 終了時に保存されたデータ(yieldも同様)

	std::vector<flight_function_data> functions;		// 関数一覧
	std::vector<flight_variable> global_variables;		// グローバル変数(消えない プログラム終了まで残り続ける)
	std::vector<flight_stack_variable> stack_variables;	// 一般的な変数(stack_pointer以下の変数が使える変数)

	std::shared_ptr<fi_container> instructions;
	std::map<std::string, std::shared_ptr<flight_function_holder>>* phf_holders;	// ホスト関数一覧

	// エラー関連
	int now_line_no;			// 現在の行番号(エラー出力用)
	int error_line_no;			// 発生したエラー行
	flight_errors error;		// 発生したエラー
	flight_token error_token;	// エラートークン

};