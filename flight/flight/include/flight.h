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

	// �R���X�g���N�^�E�f�X�g���N�^
	flight();
	~flight();

	// ���[�h
	bool load_memory(const char* memory);
	bool load_file(const char* filename);

	// ���[�h��������
	bool load_complete();

	// ���s
	void exec();

	// ���
	void release();

	// �z�X�g�֐��̒ǉ�(load_complete��ɍs����)
	template<typename R, typename... Args>
	bool register_function(const std::string& key, void(*pf)(R, Args...));

	// �z�X�g�֐��̒ǉ�(load_complete��ɍs����)
	template<typename R, typename... Args>
	bool register_function(const std::string& key, const std::function<void(R, Args...)>& f);

	// �G���[�����݂��邩�m�F
	bool has_error() const;

	// ���ɏ������I����Ă��邩
	bool is_prepared() const;

	// ����vm�͏I�����Ă��邩
	bool is_exit_vm() const;

	// �Ō�̃G���[���擾����
	flight_errors get_last_error() const;
	std::string get_last_error_string() const;

	// ���s��̃f�[�^���擾����
	const void get_exec_data(flight_variable& var) const;

private:

	// 
	void restart_vm();

	// �s�ԍ�
	bool load_inner(const std::string& raw_code);

	// �Ō�̃G���[��ݒ�
	void set_last_error(const int line_no, const flight_errors last_error, const std::string error_str);

private:

	// �����o�[�ϐ�
	std::shared_ptr<void> _error_info;		// �G���[���
	std::shared_ptr<void> _instructions;	// ����
	std::shared_ptr<void> _vmdata;			// ���z�}�V���f�[�^
	int _signature;							// ���ʎq

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
	// �l�̐ݒ�
	auto holder = std::make_shared<flight_function_holder>();
	flight_register_function(*holder, f);

	// �L�[���쐬
	const std::string signature = key + "-" + std::to_string(holder->_arg_size);
	if (_holders.find(signature) != _holders.end()) { return false; }

	// �֐��y�A�̒ǉ�
	_holders[signature] = holder;

	// �����܂ŗ����ꍇ�͐���
	return true;
}
