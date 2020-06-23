#pragma once


//=============================================================================
// include files
//=============================================================================
#include "common.h"
#include "flight_type.h"


//=============================================================================
// <class> :: flight
//=============================================================================
class flight
{
public:

	// コンストラクタ・デストラクタ
	flight();
	~flight();

	// ロード
	bool load_memory(const char* memory);
	bool load_file(const char* filename);

	// ロード完了処理
	bool load_complete();

	// 実行
	void exec();

	// 解放
	void release();

	// ホスト関数の追加(load_complete後に行う事)
	template<typename R, typename... Args>
	bool register_function(const std::string& key, void(*pf)(R, Args...));

	// ホスト関数の追加(load_complete後に行う事)
	template<typename R, typename... Args>
	bool register_function(const std::string& key, const std::function<void(R, Args...)>& f);

	// エラーが存在するか確認
	bool has_error() const;

	// 既に準備が終わっているか
	bool is_prepared() const;

	// 既にvmは終了しているか
	bool is_exit_vm() const;

	// 最後のエラーを取得する
	flight_errors get_last_error() const;
	std::string get_last_error_string() const;

	// 実行後のデータを取得する
	const void get_exec_data(flight_variable& var) const;

private:

	// 
	void restart_vm();

	// 行番号
	bool load_inner(const std::string& raw_code);

	// 最後のエラーを設定
	void set_last_error(const int line_no, const flight_errors last_error, const std::string error_str);

private:

	// メンバー変数
	std::shared_ptr<void> _error_info;		// エラー情報
	std::shared_ptr<void> _instructions;	// 命令
	std::shared_ptr<void> _vmdata;			// 仮想マシンデータ
	int _signature;							// 識別子

	std::map<std::string, std::shared_ptr<flight_function_holder>> _holders;
};


//=============================================================================
// <flight> :: register_function
//=============================================================================
template<typename R, typename ...Args>
inline bool flight::register_function(const std::string& key, void(*pf)(R, Args...))
{
	return register_function(key, std::function<void(R, Args...)>(pf));
}


//=============================================================================
// <flight> :: register_function
//=============================================================================
template<typename R, typename ...Args>
inline bool flight::register_function(const std::string& key, const std::function<void(R, Args...)>& f)
{
	// 値の設定
	auto holder = std::make_shared<flight_function_holder>();
	flight_register_function(*holder, f);

	// キーを作成
	const std::string signature = key + "-" + std::to_string(holder->_arg_size);
	if (_holders.find(signature) != _holders.end()) { return false; }

	// 関数ペアの追加
	_holders[signature] = holder;

	// ここまで来た場合は成功
	return true;
}
