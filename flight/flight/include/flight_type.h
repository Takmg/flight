#pragma once


//=============================================================================
// include files
//=============================================================================
#include "common.h"


//=============================================================================
// <enum> :: flight_errors
//=============================================================================
typedef enum __flight_errors {
	error_none = 0,

	// simple error
	error_notfoundfile,				// ファイルが見つからなかった
	error_nullmemory,				// 読み込もうとしたメモリが存在しない

	// token error
	error_notendchar,				// charの終わりのシングルクォートがなかった
	error_toolongchar,				// charの文字が2文字以上
	error_notendstring,				// 文字列の終わりダブルクォートがなかった
	error_tootokenlength,			// 変数名や文字列のトークンが長すぎる
	error_endcomment,				// コメントの終了がない
	error_invalidnumber,			// 無効な数値だった
	error_not_found_closing_block,	// 終わりブロックがない
	error_unknown_token,			// 不明なトークンだった
	error_function_notop,			// 関数がトップレベルになかった
	error_declare_function,			// 関数名の宣言エラー

	// syntax error
	error_syntax,					// 構文エラー

	// vmの実行エラー
	fatal_error,					// 致命的なエラー
	error_not_found_variable,		// 変数が見つからないエラー
	error_not_found_function,		// 関数が見つからないエラー
	error_function_argument_count,	// 引数の個数エラー
	error_not_support_operation,	// 非対応演算子
	error_not_found_label,			// ラベルが見つからないエラー
	error_stack_overflow,			// スタックオーバーフロー
	error_declare_variable,			// 変数の宣言エラー
	error_if_condition,				// if命令の条件結果が数値じゃなかった
	error_return_value,				// return値がおかしかった

} flight_errors;


//=============================================================================
// <enum> :: flight_data_type
//=============================================================================
typedef enum __flight_data_type
{
	FDT_NONE = 0,			// 無し
	FDT_VARNUMBER,			// 数値
	FDT_STRING,				// 文字列
	FDT_VARFUNC,			// 関数アドレス
	FDT_VARARRAY,			// 配列(現段階では未サポート)
} flight_data_type;


//=============================================================================
// <struct> :: flight_variable
//=============================================================================
struct flight_variable
{
	std::string variable_name;		// 変数名
	std::string value;				// 値
	flight_data_type type;			// データタイプ
};


//-----------------------------------------------------------------------------


//=============================================================================
// <struct> :: flight_function_caller_base
//=============================================================================
template<typename R, typename... Args>
struct flight_function_caller_base
{
	// typedefs
	typedef typename std::function<void(R, Args...)> F;
	typedef void(*PF)(R, Args...);

	// enums
	enum { ARG_SIZE = sizeof...(Args) };

	// create関数
	static std::shared_ptr<F> create(const F& f) { return std::make_shared<F>(f); }
	static std::shared_ptr<F> create(PF pf) { return create(F(pf)); }
};


//=============================================================================
// <struct> :: flight_function_caller
//=============================================================================
template<typename T>
struct flight_function_caller {};

template<typename R>
struct flight_function_caller<void(*)(R)> : public flight_function_caller_base<R>
{
	typedef std::function<void(R)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret);
	}
};

template<typename R, typename A1>
struct flight_function_caller<void(*)(R, A1)> : public flight_function_caller_base<R, A1>
{
	typedef std::function<void(R, A1)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0]);
	}
};

template<typename R, typename A1, typename A2>
struct flight_function_caller<void(*)(R, A1, A2)> : public flight_function_caller_base<R, A1, A2>
{
	typedef std::function<void(R, A1, A2)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1]);
	}
};

template<typename R, typename A1, typename A2, typename A3>
struct flight_function_caller<void(*)(R, A1, A2, A3)> : public flight_function_caller_base<R, A1, A2, A3>
{
	typedef std::function<void(R, A1, A2, A3)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4)> : public flight_function_caller_base<R, A1, A2, A3, A4>
{
	typedef std::function<void(R, A1, A2, A3, A4)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2], v[3]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4, A5)> : public flight_function_caller_base<R, A1, A2, A3, A4, A5>
{
	typedef std::function<void(R, A1, A2, A3, A4, A5)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2], v[3], v[4]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4, A5, A6)> : public flight_function_caller_base<R, A1, A2, A3, A4, A5, A6>
{
	typedef std::function<void(R, A1, A2, A3, A4, A5, A6)> F;

	// 呼び出し関数
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2], v[3], v[4], v[5]);
	}
};


//=============================================================================
// <struct> :: flight_function_holder
//=============================================================================
struct flight_function_holder
{
	// 関数
	void(*_caller)(const std::shared_ptr<void>&, flight_variable&, const std::vector<flight_variable>&);
	int _arg_size;						// 引数サイズ
	std::shared_ptr<void> _finstance;	// 関数インスタンス
};


//=============================================================================
// <struct> :: flight_register_function
//=============================================================================
template<typename R, typename... Args>
void flight_register_function(flight_function_holder& holder, const std::function<void(R, Args...)>& f)
{
	typedef flight_function_caller<void(*)(R, Args...)> FCALLER;

	// インスタンスとポインターを取得
	holder._finstance = FCALLER::create(f);
	holder._caller = FCALLER::call;
	holder._arg_size = FCALLER::ARG_SIZE;
}

