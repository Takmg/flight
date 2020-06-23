//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <static functions> :: flight_prepare_func_args_recursive
//=============================================================================
static void flight_prepare_func_args_recursive(const fl_ptr ptr, std::vector<flight_arg>& args)
{
	// �m�[�h���Ȃ��ꍇ�A�߂�
	if (ptr == nullptr) { return; }

	// �J���}�������ꍇ�͎��̃m�[�h����������
	if (ptr->token.type == flight_token_type::FTK_SEPARATOR)
	{
		flight_prepare_func_args_recursive(ptr->left, args);
		flight_prepare_func_args_recursive(ptr->right, args);
		return;
	}

	// �����̒ǉ�
	flight_arg arg;
	arg.variable_name = ptr->token.token_str;
	args.push_back(arg);
}


//=============================================================================
// <static functions> :: flight_prepare_functions
//=============================================================================
static bool flight_prepare_functions(flight_vm_data& vmdata, const error_setter& error_setter)
{
	auto& insts = *vmdata.instructions;

	// �֐������擾
	for (int now_index = 0; now_index < (int)insts.size(); ++now_index)
	{
		// token���w��
		const auto& ins = insts[now_index];
		const auto& token = ins->root->token;

		// �֐��ǉ�
		if (token.type == flight_token_type::FTK_INSTRUCTION_RESERVED && token.token_str == "func")
		{
			// �֐���
			const std::string& func_name = ins->root->left->token.token_str;

			// ���Ɋ֐����o�^����Ă���ꍇ�̓G���[�ƂȂ�
			const auto f = [&](const auto& el) { return el.function_name == func_name; };
			const auto& it = std::find_if(vmdata.functions.begin(), vmdata.functions.end(), f);

			// ����֐��������݂���ꍇ�A�G���[
			if (it != vmdata.functions.end())
			{
				// �G���[
				error_setter(vmdata.now_line_no, flight_errors::error_declare_function, func_name.c_str());
				return false;
			}

			// �֐��̐ݒ�
			flight_function_data function;
			function.function_name = func_name;
			function.start_address = ins->address + 1;
			flight_prepare_func_args_recursive(ins->root->right, function.args);

			// �֐����̒ǉ�
			vmdata.functions.push_back(function);
		}
	}

	return true;
}


//=============================================================================
// <static functions> :: flight_vm_reserved_value
//=============================================================================
static void flight_vm_reserved_value(flight_vm_data& vmdata)
{
	flight_variable var_true;
	var_true.variable_name = "true";
	var_true.value = "1";
	var_true.type = flight_data_type::FDT_VARNUMBER;

	flight_variable var_false;
	var_false.variable_name = "false";
	var_false.value = "0";
	var_true.type = flight_data_type::FDT_VARNUMBER;

	flight_variable var_null;
	var_null.variable_name = "null";
	var_null.value = "0";
	var_true.type = flight_data_type::FDT_VARNUMBER;

	// �ǉ�
	vmdata.global_variables.push_back(var_true);
	vmdata.global_variables.push_back(var_false);
	vmdata.global_variables.push_back(var_null);
}


//=============================================================================
// <global functions> :: flight_prepare_instructions
//=============================================================================
bool flight_prepare_instructions(flight_vm_data& vmdata, const error_setter& error_setter)
{
	auto& insts = *vmdata.instructions;

	// �e���߂ɃA�h���X��ݒ�
	for (int i = 0; i < (int)insts.size(); i++) { insts[i]->address = i; }

	// VM�f�[�^�Ɋ֐��̏��ݒ�
	if (!flight_prepare_functions(vmdata, error_setter)) { return false; }

	return true;
}


//=============================================================================
// <global functions> :: flight_prepare_vmdata
//=============================================================================
void flight_prepare_vmdata(flight_vm_data& vmdata, const error_setter& error_setter)
{
	// �X�^�b�N�ɕϐ��̈��ǉ�
	static const int stack_size = 1000;
	vmdata.stack_variables.resize(stack_size);
	vmdata.stack_pointer = stack_size;

	// �u���b�N���x���ƃX�^�b�N���x����0�ɂ���B
	vmdata.now_block_level = vmdata.now_stack_level = 0;

	// �v���O�����J�E���^�[��0�ɂ���
	vmdata.program_counter = 0;

	// �A�L�������[�^�̃��Z�b�g
	vmdata.accumulator = flight_stack_variable();

	// �I���t���O��false
	vmdata.exit_flg = false;
}


//----------------------------------------------------------------------------


//=============================================================================
// <static functions> :: print
//=============================================================================
static void print(flight_variable& ret, const flight_variable& arg1)
{
	ret.type = flight_data_type::FDT_NONE;
	ret.value = "";
	printf("%s", arg1.value.c_str());
}


//=============================================================================
// <static functions> :: print_format
//=============================================================================
static void print_format2(flight_variable& ret, const flight_variable& arg1, const flight_variable& arg2)
{
	ret.type = flight_data_type::FDT_NONE;
	ret.value = "";
	printf(arg1.value.c_str(), arg2.value.c_str());
}


//=============================================================================
// <static functions> :: print_format
//=============================================================================
static void print_format3(flight_variable& ret, const flight_variable& arg1, const flight_variable& arg2
	, const flight_variable& arg3)
{
	ret.type = flight_data_type::FDT_NONE;
	ret.value = "";
	printf(arg1.value.c_str(), arg2.value.c_str(), arg3.value.c_str());
}


//=============================================================================
// <static functions> :: print_format
//=============================================================================
static void print_format4(flight_variable& ret, const flight_variable& arg1, const flight_variable& arg2
	, const flight_variable& arg3, const flight_variable& arg4)
{
	ret.type = flight_data_type::FDT_NONE;
	ret.value = "";
	printf(arg1.value.c_str(), arg2.value.c_str(), arg3.value.c_str(), arg4.value.c_str());
}


//=============================================================================
// <static functions> :: print_format
//=============================================================================
static void print_format5(flight_variable& ret, const flight_variable& arg1, const flight_variable& arg2
	, const flight_variable& arg3, const flight_variable& arg4, const flight_variable& arg5)
{
	ret.type = flight_data_type::FDT_NONE;
	ret.value = "";
	printf(arg1.value.c_str(), arg2.value.c_str(), arg3.value.c_str(), arg4.value.c_str(), arg5.value.c_str());
}


//=============================================================================
// <global functions> :: flight_prepare_embedded_function
//=============================================================================
void flight_prepare_embedded_function(flight* instance, flight_vm_data& vmdata)
{
	// �֐�����o�^
	instance->register_function("print", print);
	instance->register_function("printf", print);
	instance->register_function("printf", print_format2);
	instance->register_function("printf", print_format3);
	instance->register_function("printf", print_format4);
	instance->register_function("printf", print_format5);
}
