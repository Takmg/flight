//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <flight> :: constructor
//=============================================================================
flight::flight()
{
	// 識別子を0クリア
	_signature = 0;
}


//=============================================================================
// <flight> :: destructor
//=============================================================================
flight::~flight()
{
	release();
}


//=============================================================================
// <flight> :: load_memory
//=============================================================================
bool flight::load_memory(const char* memory)
{
	// エラー情報設定処理
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// メモリからロード
	const auto raw_code = flight_loadmemory(memory, error_setter);
	return load_inner(raw_code);
}


//=============================================================================
// <flight> :: load_file
//=============================================================================
bool flight::load_file(const char* filename)
{
	// エラー情報設定処理
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// ファイルからロード
	const auto raw_code = flight_loadfile(filename, error_setter);
	return load_inner(raw_code);
}


//=============================================================================
// <flight> :: load_complete
//=============================================================================
bool flight::load_complete()
{
	// 命令が存在しない場合はfalseを返す
	if (_instructions == nullptr) { return false; }

	// vmデータの作成
	_vmdata = std::make_shared<flight_vm_data>();
	std::shared_ptr<flight_vm_data> vmdata = std::static_pointer_cast<flight_vm_data>(_vmdata);

	// 命令の取得
	std::shared_ptr<fi_container> instructions = std::static_pointer_cast<fi_container>(_instructions);
	if (instructions->size() <= 0) { _vmdata.reset(); return false; }

	// エラーメソッド
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// ホスト用の関数を設定
	vmdata->instructions = instructions;
	vmdata->phf_holders = &_holders;

	// 準備を行う
	if (!flight_prepare_instructions(*vmdata, error_setter)) { return false; }
	flight_prepare_vmdata(*vmdata, error_setter);
	flight_prepare_embedded_function(this, *vmdata);

	return true;
}


//=============================================================================
// <flight> :: restart_vm
//=============================================================================
void flight::restart_vm()
{
	// 準備前の場合は何もしない
	if (_vmdata == nullptr || _instructions == nullptr) { return; }

	// メモリ情報の取得
	std::shared_ptr<flight_vm_data> vmdata = std::static_pointer_cast<flight_vm_data>(_vmdata);
	std::shared_ptr<fi_container> instructions = std::static_pointer_cast<fi_container>(_instructions);

	// エラーメソッド
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// ホスト用の関数を設定
	vmdata->instructions = instructions;
	vmdata->phf_holders = &_holders;

	flight_prepare_vmdata(*vmdata, error_setter);
}


//=============================================================================
// <flight> :: exec
//=============================================================================
void flight::exec()
{
	// 準備前の場合は何もしない
	if (!is_prepared()) { return; }

	// メモリの取得
	std::shared_ptr<flight_vm_data> vmdata = std::static_pointer_cast<flight_vm_data>(_vmdata);
	vmdata->instructions = std::static_pointer_cast<fi_container>(_instructions);

	// エラーメソッド
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// もし既にvmが終了していた場合は自動再起動
	if (vmdata->exit_flg) { restart_vm(); }

	// エラーをリセット
	vmdata->error = flight_errors::error_none;

	// 準備を行う
	flight_vm_exec(*vmdata, error_setter);
}


//=============================================================================
// <flight> :: release
//=============================================================================
void flight::release()
{
	_instructions.reset();
	_error_info.reset();
	_vmdata.reset();
}


//=============================================================================
// <flight> :: has_error
//=============================================================================
bool flight::has_error() const { return _error_info != nullptr; }


//=============================================================================
// <flight> :: is_prepared
//=============================================================================
bool flight::is_prepared() const { return _vmdata != nullptr; }


//=============================================================================
// <flight> :: is_exit_vm
//=============================================================================
bool flight::is_exit_vm() const
{
	// 準備前の場合は終了状態
	if (_vmdata == nullptr) { return true; }

	// vmデータが終了しているか確認
	std::shared_ptr<flight_vm_data> vmdata = std::static_pointer_cast<flight_vm_data>(_vmdata);
	return vmdata->exit_flg;
}


//=============================================================================
// <flight> :: load_inner
//=============================================================================
bool flight::load_inner(const std::string& raw_code)
{
	// flight_instruction のメモリ確保 及び 使いやすいように修正
	if (_instructions == nullptr) { _instructions = std::make_shared<fi_container>(); }
	std::shared_ptr<fi_container> instructions_ptr = std::static_pointer_cast<fi_container>(_instructions);

	// エラーがある場合は戻る。
	if (has_error()) { return false; }

	// エラー情報設定処理(引数として渡さずにここでもerror_setterを作る)
	// 理由としてはcommon_innnerにstd::functionのtypedefを宣言しており、
	// 外部から見えるincludeに余計なtypedefを放り込みたく無い為。
	using namespace std::placeholders;
	auto error_setter = std::bind(&flight::set_last_error, this, _1, _2, _3);

	// トークン分割(字句解析)
	auto tokens = flight_tokenize(raw_code, error_setter);

	// 木構造の作成(構文解析)
	flight_tree_generater(*instructions_ptr, _signature, tokens, error_setter);

	return true;
}


//=============================================================================
// <flight> :: set_last_error
//=============================================================================
void flight::set_last_error(const int line_no, const flight_errors last_error, const std::string error_str)
{
	// メモリ確保
	if (_error_info == nullptr) { _error_info = std::make_shared<flight_error_info>(); }
	const auto& error_info = std::static_pointer_cast<flight_error_info>(_error_info);

	// 情報設定
	error_info->line_no = line_no;
	error_info->error = last_error;
	error_info->error_token_str = error_str;
}


//=============================================================================
// <struct> :: 
//=============================================================================
struct flight_error_string { flight_errors eid; const char* format; };

//=============================================================================
// <static variables>
//=============================================================================
static const flight_error_string _error_strings[] = {

	{ flight_errors::error_notfoundfile, "ロードするファイルが見つかりません。%s"},
	{ flight_errors::error_nullmemory,"ロードするメモリがnullです。%s"},

	// token error
	{ flight_errors::error_notendchar,"終わりシングルクォートがありません。%s"},
	{ flight_errors::error_toolongchar,"シングルクォートの中身が2文字以上です。%s"},
	{ flight_errors::error_notendstring,"文字列終わりのダブルクォートがありません。%s"},
	{ flight_errors::error_tootokenlength,"一つのトークン文字数が長すぎます。%s"},
	{ flight_errors::error_endcomment,"コメントの終わりがありませんでした。%s"},
	{ flight_errors::error_invalidnumber,"無効な数値が使用されています。%s"},
	{ flight_errors::error_not_found_closing_block,"終わりブロックが見つかりませんでした。%s"},
	{ flight_errors::error_unknown_token,"不明なトークンが見つかりました。%s"},
	{ flight_errors::error_function_notop,"関数がブロック内に見つかりました。%s"},
	{ flight_errors::error_declare_function,"関数の宣言エラーです。%s"},

	// syntax error
	{ flight_errors::error_syntax,"構文解析エラーです。%s"},

	// vmの実行エラー
	{ flight_errors::fatal_error,"致命的なエラーです。%s"},
	{ flight_errors::error_not_found_variable,"変数が見つかりません。%s"},
	{ flight_errors::error_not_found_function,"関数が見つかりません。%s"},
	{ flight_errors::error_function_argument_count,"関数の引数の数に誤りがあります。%s"},
	{ flight_errors::error_not_support_operation,"演算子の使い方に間違いがあります。%s"},
	{ flight_errors::error_not_found_label,"ジャンプ先のラベルが見つかりません。%s"},
	{ flight_errors::error_stack_overflow,"スタックオーバーフローが発生しました。%s"},
	{ flight_errors::error_declare_variable,"変数の宣言に誤りがあります。%s"},
	{ flight_errors::error_if_condition,"条件構文に誤りがあります。%s"},
	{ flight_errors::error_return_value,"関数の戻り値に誤りがあります。%s"},
};


//=============================================================================
// <flight> :: get_last_error
//=============================================================================
flight_errors flight::get_last_error() const
{
	// 変数に戻す
	if (_error_info == nullptr) { return flight_errors::error_none; }
	const auto& error_info = std::static_pointer_cast<flight_error_info>(_error_info);

	// エラー情報の取得
	return error_info->error;
}


//=============================================================================
// <flight> :: get_last_error_string
//=============================================================================
std::string flight::get_last_error_string() const
{
	// 変数に戻す
	if (_error_info == nullptr) { return std::string(); }
	const auto& error_info = std::static_pointer_cast<flight_error_info>(_error_info);

	// エラーが存在しない
	if (error_info->error == flight_errors::error_none) { return std::string(); }

	// エラー文字列を設定する
	char error_str[0x1000] = {};
	for (int i = 0; i < sizeof(_error_strings) / sizeof(_error_strings[0]); i++)
	{
		// エラーIDが異なる場合
		if (_error_strings[i].eid != error_info->error) { continue; }

		// エラー文字列を設定
		sprintf_s(error_str, _error_strings[i].format, error_info->error_token_str.c_str());
	}

	// 文字列の修正
	char formatted_string[0x1000] = {};
	sprintf_s(formatted_string, "LINE-NO => %d\nERROR-ID => %d\nERROR => %s\n",
		error_info->line_no + 1, error_info->error, error_str);

	// エラー文字列を戻す
	return std::string(formatted_string);
}


//=============================================================================
// <flight> :: get_exec_data
//=============================================================================
const void flight::get_exec_data(flight_variable& var) const
{
	if (_vmdata == nullptr) { return; }

	// vmデータが終了しているか確認
	std::shared_ptr<flight_vm_data> vmdata = std::static_pointer_cast<flight_vm_data>(_vmdata);
	var = vmdata->exit_var;
}


