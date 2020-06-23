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
	error_notfoundfile,				// �t�@�C����������Ȃ�����
	error_nullmemory,				// �ǂݍ������Ƃ��������������݂��Ȃ�

	// token error
	error_notendchar,				// char�̏I���̃V���O���N�H�[�g���Ȃ�����
	error_toolongchar,				// char�̕�����2�����ȏ�
	error_notendstring,				// ������̏I���_�u���N�H�[�g���Ȃ�����
	error_tootokenlength,			// �ϐ����╶����̃g�[�N������������
	error_endcomment,				// �R�����g�̏I�����Ȃ�
	error_invalidnumber,			// �����Ȑ��l������
	error_not_found_closing_block,	// �I���u���b�N���Ȃ�
	error_unknown_token,			// �s���ȃg�[�N��������
	error_function_notop,			// �֐����g�b�v���x���ɂȂ�����
	error_declare_function,			// �֐����̐錾�G���[

	// syntax error
	error_syntax,					// �\���G���[

	// vm�̎��s�G���[
	fatal_error,					// �v���I�ȃG���[
	error_not_found_variable,		// �ϐ���������Ȃ��G���[
	error_not_found_function,		// �֐���������Ȃ��G���[
	error_function_argument_count,	// �����̌��G���[
	error_not_support_operation,	// ��Ή����Z�q
	error_not_found_label,			// ���x����������Ȃ��G���[
	error_stack_overflow,			// �X�^�b�N�I�[�o�[�t���[
	error_declare_variable,			// �ϐ��̐錾�G���[
	error_if_condition,				// if���߂̏������ʂ����l����Ȃ�����
	error_return_value,				// return�l��������������

} flight_errors;


//=============================================================================
// <enum> :: flight_data_type
//=============================================================================
typedef enum __flight_data_type
{
	FDT_NONE = 0,			// ����
	FDT_VARNUMBER,			// ���l
	FDT_STRING,				// ������
	FDT_VARFUNC,			// �֐��A�h���X
	FDT_VARARRAY,			// �z��(���i�K�ł͖��T�|�[�g)
} flight_data_type;


//=============================================================================
// <struct> :: flight_variable
//=============================================================================
struct flight_variable
{
	std::string variable_name;		// �ϐ���
	std::string value;				// �l
	flight_data_type type;			// �f�[�^�^�C�v
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

	// create�֐�
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

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret);
	}
};

template<typename R, typename A1>
struct flight_function_caller<void(*)(R, A1)> : public flight_function_caller_base<R, A1>
{
	typedef std::function<void(R, A1)> F;

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0]);
	}
};

template<typename R, typename A1, typename A2>
struct flight_function_caller<void(*)(R, A1, A2)> : public flight_function_caller_base<R, A1, A2>
{
	typedef std::function<void(R, A1, A2)> F;

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1]);
	}
};

template<typename R, typename A1, typename A2, typename A3>
struct flight_function_caller<void(*)(R, A1, A2, A3)> : public flight_function_caller_base<R, A1, A2, A3>
{
	typedef std::function<void(R, A1, A2, A3)> F;

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4)> : public flight_function_caller_base<R, A1, A2, A3, A4>
{
	typedef std::function<void(R, A1, A2, A3, A4)> F;

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2], v[3]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4, A5)> : public flight_function_caller_base<R, A1, A2, A3, A4, A5>
{
	typedef std::function<void(R, A1, A2, A3, A4, A5)> F;

	// �Ăяo���֐�
	static void call(const std::shared_ptr<void>& f, flight_variable& ret, const std::vector<flight_variable>& v)
	{
		(*std::static_pointer_cast<F>(f))(ret, v[0], v[1], v[2], v[3], v[4]);
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct flight_function_caller<void(*)(R, A1, A2, A3, A4, A5, A6)> : public flight_function_caller_base<R, A1, A2, A3, A4, A5, A6>
{
	typedef std::function<void(R, A1, A2, A3, A4, A5, A6)> F;

	// �Ăяo���֐�
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
	// �֐�
	void(*_caller)(const std::shared_ptr<void>&, flight_variable&, const std::vector<flight_variable>&);
	int _arg_size;						// �����T�C�Y
	std::shared_ptr<void> _finstance;	// �֐��C���X�^���X
};


//=============================================================================
// <struct> :: flight_register_function
//=============================================================================
template<typename R, typename... Args>
void flight_register_function(flight_function_holder& holder, const std::function<void(R, Args...)>& f)
{
	typedef flight_function_caller<void(*)(R, Args...)> FCALLER;

	// �C���X�^���X�ƃ|�C���^�[���擾
	holder._finstance = FCALLER::create(f);
	holder._caller = FCALLER::call;
	holder._arg_size = FCALLER::ARG_SIZE;
}

