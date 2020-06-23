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
	// �v���O�����J�E���^�[�ɉ��������߂��擾
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
	// �֐�������
	const auto& funcs = vmdata.functions;
	const auto f = [&](const auto& el) { return el.function_name == search_funcname; };
	const auto& fit = std::find_if(funcs.begin(), funcs.end(), f);

	// �z�X�g�֐�������
	const auto& hfuncs = *vmdata.phf_holders;
	const auto hf = [&](const auto& el) { return el.first.find(search_funcname) == 0; };
	const auto& fit2 = std::find_if(hfuncs.begin(), hfuncs.end(), hf);

	// �֐����X�g�ɑ��݂��Ȃ������ꍇ
	if (fit == funcs.end() && fit2 == hfuncs.end()) { return flight_vm_status::VM_ERROR; }

	// �֐����̐ݒ�
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
	// �X�^�b�N�|�C���^�[�����Ɏ��s���Ă���
	for (int sp = vmdata.stack_pointer; sp < (int)vmdata.stack_variables.size(); sp++)
	{
		auto& svar = vmdata.stack_variables[sp];

		// �X�^�b�N�|�C���^�[������łȂ� ���� �u���b�N�|�C���^�[�����ݒn���Ⴂ�ϐ� �łȂ��ꍇ�͔�����
		if (!(svar.stack_level == vmdata.now_stack_level && svar.block_level <= vmdata.now_block_level))
		{
			// �����܂ŕϐ���������Ȃ�����
			break;
		}

		// �ϐ������قȂ�ꍇ��continue
		if (svar.variable_name != variable_name) { continue; }

		// �ϐ��̐ݒ�
		f(svar);
		return flight_vm_status::VM_EXEC;
	}

	// �O���[�o���ϐ���������擾
	for (int index = 0; index < (int)vmdata.global_variables.size(); index++)
	{
		// �ϐ������������ꍇ
		if (vmdata.global_variables[index].variable_name == variable_name)
		{
			// �ϐ��̐ݒ�
			f(vmdata.global_variables[index]);
			return flight_vm_status::VM_EXEC;
		}
	}

	// �����܂ŗ����ꍇ�͌�����Ȃ�����
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
			// �ϐ���������Ȃ��G���[
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
	// �J���}�̏���

	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable var1, var2;
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// �ϐ��ɉ����Ȃ��f�[�^���i�[����
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";
	ret_var.variable_name = "tmp";

	// ������Ԃ�
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_cond
//=============================================================================
static flight_vm_status flight_vm_op_cond(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable var1, var2;

	// �܂��E�Ӓl�̌v�Z
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// ���Ӓl���]������
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// �߂�
	const auto& op = node->token.token_str;
	ret_var.type = flight_data_type::FDT_VARNUMBER;

	// �G���[�t���O
	bool is_error = false;

	// �Е����֐��|�C���^�������ꍇ�G���[
	if (var1.type == flight_data_type::FDT_VARFUNC || var2.type == flight_data_type::FDT_VARFUNC)
	{
		is_error = true;
	}

	// ���l���m�̔�r
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

	// ������Ƃ̔�r
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

	// ����ȊO�̏���
	else { is_error = true; }

	// �G���[�������ꍇ
	if (is_error)
	{
		// ���Z�q�̃G���[
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// ����
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_not
//=============================================================================
static flight_vm_status flight_vm_op_not(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable var1;

	// ���Ӓl��]������
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// ���l�ȊO�������ꍇ�� �������Z�q���g�p�ł��Ȃ�
	if (var1.type != flight_data_type::FDT_VARNUMBER)
	{
		// ���l�ȊO�ŏ������Z�q���g�p���悤�Ƃ���
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// �߂�
	ret_var.type = flight_data_type::FDT_VARNUMBER;

	// not���Z�q
	const auto n1 = atoi(var1.value.c_str());
	ret_var.value = std::to_string((!n1) ? 1 : 0);

	// ����
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_common
//=============================================================================
static flight_vm_status flight_vm_op_common(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable var1, var2;

	// �܂��E�Ӓl�̌v�Z
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// ���Ӓl���]������
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// �߂�
	const char op = node->token.token_str[0];

	bool is_error = true;

	// �Е����֐��|�C���^�������ꍇ�G���[
	if (var1.type == flight_data_type::FDT_VARFUNC || var2.type == flight_data_type::FDT_VARFUNC)
	{
		is_error = true;
	}

	// ���l���m�̌v�Z
	else if (var1.type == flight_data_type::FDT_VARNUMBER && var2.type == flight_data_type::FDT_VARNUMBER)
	{
		// �����_�L���l�ɕϊ�����
		const auto n1 = atof(var1.value.c_str());
		const auto n2 = atof(var2.value.c_str());
		if (op == '+') { ret_var.value = std::to_string(n1 + n2); }
		if (op == '-') { ret_var.value = std::to_string(n1 - n2); }
		if (op == '*') { ret_var.value = std::to_string(n1 * n2); }
		if (op == '/') { ret_var.value = std::to_string(n1 / n2); }

		// �����_�ȉ����S��0�������ꍇ�͏����_�ȉ����폜����
		bool is_frac = false;
		const int dot_index = (int)ret_var.value.find('.');
		for (int i = dot_index + 1; dot_index >= 0 && i < (int)ret_var.value.length(); i++)
		{
			// �����_�ȉ���0�ȊO���o���ꍇ�͏����t���O��true�ɐݒ�
			if (ret_var.value[i] != '0') { is_frac = true; break; }
		}
		if (!is_frac) { ret_var.value = ret_var.value.substr(0, dot_index); }

		ret_var.type = flight_data_type::FDT_VARNUMBER;

		// �G���[�Ȃ�
		is_error = false;
	}
	// �Е��������񂾂����ꍇ
	else if (var1.type == flight_data_type::FDT_STRING || var2.type == flight_data_type::FDT_STRING)
	{
		// +�ȊO�������ꍇ
		if (op != '+')
		{
			// ������̉��Z��+�ȊO�F�߂��Ȃ�
			vmdata.error_line_no = node->token.line_no;
			vmdata.error = flight_errors::error_not_support_operation;
			vmdata.error_token = node->token;
			return flight_vm_status::VM_ERROR;
		}

		// ������̒P�����Z���s��
		ret_var.value = var1.value + var2.value;
		ret_var.type = flight_data_type::FDT_STRING;

		// �G���[�Ȃ�
		is_error = false;
	}

	// ���T�|�[�g
	if (is_error)
	{
		// ������Z�q�̖��T�|�[�g
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// ������Z�q�������ꍇ store���s��
	if (node->token.token_str.length() >= 2 && node->token.token_str[1] == '=')
	{
		// �ϐ��ɒl�̐ݒ�
		const auto status3 = flight_vm_store(vmdata, ret_var, node->left->token.token_str);
		if (status3 != flight_vm_status::VM_EXEC) { return status3; }
	}

	// ����
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_op_equal
//=============================================================================
static flight_vm_status flight_vm_op_equal(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable var1, var2;

	// �܂��E�Ӓl�̌v�Z
	const auto status2 = flight_vm_exec_recursive(vmdata, var2, node->right);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// ���Ӓl���]������
	const auto status1 = flight_vm_exec_recursive(vmdata, var1, node->left);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// ���ݒ�̕ϐ��ɑ���͎g�p�ł��Ȃ�
	if (var2.type == flight_data_type::FDT_NONE)
	{
		// ������Z�q�Ő��l�ȊO�������悤�Ƃ���
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_support_operation;
		vmdata.error_token = node->token;
		return flight_vm_status::VM_ERROR;
	}

	// �ϐ��ɒl�̐ݒ�
	const auto status3 = flight_vm_store(vmdata, var2, node->left->token.token_str);
	if (status3 != flight_vm_status::VM_EXEC) { return status3; }

	// ����
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
	// �X�^�b�N�|�C���^���擾
	int& sp = vmdata.stack_pointer;
	--sp;

	// �X�^�b�N�|�C���^�[��0�����Ȃ�X�^�b�N�I�[�o�[�t���[
	if (sp < 0)
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_stack_overflow;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// ����ϐ����̊m�F
	const auto f = [&](const auto& el)
	{
		return	el.variable_name == var.variable_name &&
			el.block_level == var.block_level &&
			el.stack_level == var.stack_level;
	};
	const auto& it = std::find_if(vmdata.stack_variables.begin() + sp + 1, vmdata.stack_variables.end(), f);
	if (it != vmdata.stack_variables.end())
	{
		// ����ϐ��������݂���ꍇ�̓G���[
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_declare_variable;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// �ϐ��̐ݒ�
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
	// �X�^�b�N�|�C���^���擾
	int& sp = vmdata.stack_pointer;

	// ����ϐ����̊m�F
	const auto& sv = vmdata.stack_variables[sp];
	var.type = sv.type;
	var.value = sv.value;
	var.variable_name = sv.variable_name;

	// �X�^�b�N�|�C���^�[���T�C�Y�ȏ�Ȃ�X�^�b�N�I�[�o�[�t���[
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
	// node��null�Ȃ牽�����Ȃ�
	if (node == nullptr) { return; }

	// �J���}�������ꍇ�͒l�̐ݒ�
	if (node->token.type == flight_token_type::FTK_SEPARATOR)
	{
		flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
		flight_vm_func_arg_get_recursive(vmdata, vars, node->right);
		return;
	}

	// �����̎w��
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
	// �z�X�g�֐����擾
	const auto& hfuncs = *vmdata.phf_holders;

	// �������擾
	std::vector<flight_variable> vars;
	flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// �z�X�g�p�֐������쐬
	const std::string func_name = function_name + "-" + std::to_string(vars.size());

	// �֐���������擾
	const auto f = [&](const auto& f) { return f.first == func_name; };
	const auto& fit = std::find_if(hfuncs.begin(), hfuncs.end(), f);
	if (fit == hfuncs.end())
	{
		// �֐���������Ȃ�����
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_not_found_function;
		vmdata.error_token = { node->token.line_no, function_name };
		return flight_vm_status::VM_ERROR;
	}

	// �������قȂ�ꍇ�̓G���[
	const auto& arg_size = fit->second->_arg_size;
	if (arg_size != vars.size()) { return flight_vm_status::VM_ERROR; }

#ifdef _DEBUG
	// �f�o�b�O�\���Ō���Ȃ��Ă��镔����\n�ŉ��s�����Č��₷������
	char a[0x100];
	sprintf_s(a, "start host function %s", node->token.token_str.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	// �z�X�g�֐��̎��s
	fit->second->_caller(fit->second->_finstance, ret_var, vars);

#ifdef _DEBUG
	// �f�o�b�O�\���Ō���Ȃ��Ă��镔����\n�ŉ��s�����Č��₷������
	puts("");
	flight_vm_debug_state(vmdata, "end host function");
#endif

	// �v���O�����J�E���^�[����i�߂�
	++vmdata.program_counter;

	// ������Ԃ�
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_func
//=============================================================================
static flight_vm_status flight_vm_func(
	flight_vm_data& vmdata, flight_variable& ret_var, const fl_ptr& node)
{
	// node��null�̏ꍇ�������Ȃ�
	if (node == nullptr) { return flight_vm_status::VM_ERROR; }

	// ������
	std::string search_name = node->token.token_str;

	// �܂��ϐ�����֐������擾�o���邩����
	do {

		// ��������
		flight_variable var;
		const auto f = [&](auto& vm_var)
		{
			var.type = vm_var.type;
			var.value = vm_var.value;
		};

		// �ϐ��̎擾�ɐ��������ꍇ�͕ϐ����̒l�Ɍ�������֐���ς���
		auto ret = flight_vm_store_inner(vmdata, search_name, f);
		if (ret == flight_vm_status::VM_EXEC && var.type == flight_data_type::FDT_VARFUNC)
		{
			search_name = var.value;
		}
	} while (false);

	// �֐���������擾
	const auto& funcs = vmdata.functions;
	const auto f = [&](const auto& f) { return f.function_name == search_name; };
	const auto& fit = std::find_if(funcs.begin(), funcs.end(), f);
	if (fit == funcs.end())
	{
		// �֐���������Ȃ������ꍇ�́A�z�X�g�֐����擾����B
		return flight_vm_host_func(vmdata, ret_var, node, search_name);
	}

	// �X�^�b�N�ɑޔ�����f�[�^���擾
	const int return_pc = vmdata.program_counter + 1;	// �߂�ꏊ���w��
	const int block_level = vmdata.now_block_level;		// ���݂̃u���b�N���x����ޔ�
	const int stack_level = vmdata.now_stack_level;		// ���݂̃X�^�b�N���x����ޔ�

	// �l�̐ݒ�
	flight_stack_variable var_rpc;
	flight_stack_variable var_bl;
	var_rpc.block_level = var_bl.block_level = block_level;
	var_rpc.stack_level = var_bl.stack_level = stack_level;
	var_rpc.type = var_bl.type = flight_data_type::FDT_VARNUMBER;
	var_rpc.variable_name = "___RETURN_PROGRAM_COUNTER___!";
	var_bl.variable_name = "___SAVE_BLOCK_LEVEL___!";
	var_rpc.value = std::to_string(return_pc);
	var_bl.value = std::to_string(block_level);

	// vm�ɒl��push���s��
	const auto status1 = flight_vm_push(vmdata, var_bl);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	const auto status2 = flight_vm_push(vmdata, var_rpc);
	if (status2 != flight_vm_status::VM_EXEC) { return status2; }

	// �����J�E���g���擾
	std::vector<flight_variable> vars;
	flight_vm_func_arg_get_recursive(vmdata, vars, node->left);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// �������قȂ邽�߃G���[
	if (fit->args.size() != vars.size())
	{
		// �������Ⴄ�G���[
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_function_argument_count;
		vmdata.error_token = { node->token.line_no, search_name };
		return flight_vm_status::VM_ERROR;
	}

	// ���̃^�C�~���O�Ńu���b�N���x����X�^�b�N���x����ύX
	vmdata.now_block_level = 0;		// �u���b�N���x����0�N���A
	vmdata.now_stack_level++;		// �X�^�b�N���x�����C���N�������g

	// ������ݒ�
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

	// �֐��̃X�^�[�g�A�h���X�Ɉړ�
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
	// �X�^�b�N���x�����f�N�������g
	--vmdata.now_stack_level;

	// �X�^�b�N���x���ȉ��̕ϐ����폜
	int& sp = vmdata.stack_pointer;
	const auto& sv = vmdata.stack_variables;
	for (; sp < (int)sv.size(); sp++)
	{
		// �X�^�b�N���x���������Ă�����̂��폜���Ă���
		if (sv[sp].stack_level > vmdata.now_stack_level)
		{
#ifdef _DEBUG
			char a[0x100];
			sprintf_s(a, "delete local variable = %s", sv[sp].variable_name.c_str());
			flight_vm_debug_state(vmdata, a);
#endif
			continue;
		}

		// �X�^�b�N�|�C���^�������肫�����ׁA���[�v�𔲂���
		break;
	}

	// �l�̃|�b�v���s��
	flight_variable var_bl;
	flight_variable var_rpc;
	flight_vm_pop(vmdata, var_rpc);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }
	flight_vm_pop(vmdata, var_bl);
	if (vmdata.error != flight_errors::error_none) { return flight_vm_status::VM_ERROR; }

	// �v���O�����J�E���^�[�ƃu���b�N���x�����֐��Ăяo���O�ɖ߂�
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
	// �m�[�h��null�������ꍇ�͐����Ƃ��ĕԂ�
	if (node == nullptr) { return flight_vm_status::VM_EXEC; }

	// �֐���ݒ�
	const auto error_func = [&]() {
		vmdata.error_line_no = node->token.line_no;
		vmdata.error = flight_errors::error_syntax;
		vmdata.error_token = { node->token.line_no, node->token.token_str };
	};

	// ���l�������ꍇ
	if (node->token.type == flight_token_type::FTK_VALUE_NUMBER || node->token.type == flight_token_type::FTK_VALUE_CHAR)
	{
		// ���l�Ȃ̂Ɏq��������ꍇ�̓G���[
		if (node->left != nullptr || node->right != nullptr)
		{
			// �G���[�ݒ�
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// ���l�̐ݒ�
		ret_var.variable_name = "tmp";
		ret_var.value = node->token.token_str;
		ret_var.type = flight_data_type::FDT_VARNUMBER;
		return flight_vm_status::VM_EXEC;
	}
	// �����񂾂����ꍇ
	else if (node->token.type == flight_token_type::FTK_VALUE_STR)
	{
		// ������Ȃ̂Ɏq��������ꍇ�̓G���[
		if (node->left != nullptr || node->right != nullptr)
		{
			// �G���[�ݒ�
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// ������̐ݒ�
		ret_var.variable_name = "tmp";
		ret_var.value = node->token.token_str;
		ret_var.type = flight_data_type::FDT_STRING;
		return flight_vm_status::VM_EXEC;
	}
	// �ϐ��������ꍇ
	else if (node->token.type == flight_token_type::FTK_VARIABLE)
	{
		// �ϐ��Ȃ̂Ɏq��������ꍇ�̓G���[
		if (node->left != nullptr || node->right != nullptr)
		{
			// �G���[�ݒ�
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// �ϐ���������擾����
		return flight_vm_restore(vmdata, ret_var, node->token.token_str);
	}
	// �J���}�������ꍇ
	else if (node->token.type == flight_token_type::FTK_SEPARATOR)
	{
		return flight_vm_op_comma(vmdata, ret_var, node);
	}
	// ���Z�q�������ꍇ
	else if (node->token.type == flight_token_type::FTK_OPERATION)
	{
		const int len = sizeof(_op_data) / sizeof(_op_data[0]);
		for (int i = 0; i < len; i++)
		{
			// �e��R�[�h�̏���
			if (node->token.token_str == _op_data[i].op_str)
			{
				return _op_data[i].func(vmdata, ret_var, node);
			}
		}
	}
	// �A�L�������[�^�������ꍇ
	else if (node->token.type == flight_token_type::FTK_ACCUMULATOR)
	{
		// �A�L�������[�^�Ȃ̂Ɏq��������ꍇ�̓G���[
		if (node->left != nullptr || node->right != nullptr)
		{
			error_func();
			return flight_vm_status::VM_ERROR;
		}

		// �A�L�������[�^��ݒ�
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
	// �X�e�b�v�l
	const int step = forward ? 1 : -1;

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "goto label = %s", to_label.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	int block_level = 0;

	// ���߃R���e�i���擾
	const auto& instructions = *vmdata.instructions;

	// ���݂̃X�^�b�N���x����ێ�
	const int stack_level = vmdata.now_stack_level;

	// �v���O�����J�E���^�[��i�߂�
	int& pc = vmdata.program_counter;

	// �v���O�����J�E���^�[��i�߂Ȃ��烉�x������������
	for (pc += step; 0 <= pc && pc < (int)instructions.size(); pc += step)
	{
		// �^�[�Q�b�g�g�[�N�����擾
		const auto& target_token = instructions[pc]->root->token;

		// �u���b�N�J�n�Ȃ�block_level���C���N�������g/�u���b�N�I���Ȃ�f�N�������g
		if (target_token.type == flight_token_type::FTK_BLOCK)
		{
			block_level += step;
			if (block_level < 0)
			{
				const auto ret = flight_vm_exec_eblock(vmdata);
				if (ret != flight_vm_status::VM_EXEC) { return ret; }

				// �X�^�b�N���x�����قȂ�ꍇ�̓G���[�Ƃ���
				if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0) { return flight_vm_status::VM_ERROR; }

				// �u���b�N���x����0�����Ȃ�u���b�N���x����0�ɖ߂�
				// �u���b�N���x����-1�ȉ��̎���{}���������Ă�
				// �u���b�N�I���������s��Ȃ��悤�ɂ��邽�߁B
				block_level = 0;
			}
			continue;
		}
		else if (target_token.type == flight_token_type::FTK_ENDBLOCK)
		{
			// �u���b�N���x����0�����Ȃ�u���b�N�I���������s��
			block_level -= step;
			if (block_level < 0)
			{
				const auto ret = flight_vm_exec_eblock(vmdata);
				if (ret != flight_vm_status::VM_EXEC) { return ret; }

				// �X�^�b�N���x�����قȂ�ꍇ�̓G���[�Ƃ���
				if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0) { return flight_vm_status::VM_ERROR; }

				// �u���b�N���x����0�����Ȃ�u���b�N���x����0�ɖ߂�
				// �u���b�N���x����-1�ȉ��̎���{}���������Ă�
				// �u���b�N�I���������s��Ȃ��悤�ɂ��邽�߁B
				block_level = 0;
			}
			continue;
		}

		// ���x�������������ꍇ�A���[�v�𔲂���
		if (block_level <= 0 && target_token.type == flight_token_type::FTK_LABEL)
		{
			// �����񂪈�v���Ă���ꍇ�͔�����
			if (full_match && target_token.token_str == to_label ||
				!full_match && target_token.token_str.find(to_label) == 0)
			{
				break;
			}
		}
	}

	// �G���[ �v���O�����̍Ō�܂Ń��x�������݂��Ȃ�����
	if (pc < 0 || (int)instructions.size() <= pc)
	{
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_not_found_label;
		vmdata.error_token = { vmdata.now_line_no, to_label };
		return flight_vm_status::VM_ERROR;
	}

	// ���x���̎��̍s�Ƀv���O�����J�E���^�[��i�߂�
	++pc;

	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_make_stack_var
//=============================================================================
static flight_vm_status flight_vm_make_stack_var(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �ϐ��g�[�N��
	const auto var_token = inst->root->left->token;

	// �ꎞ�ϐ����쐬
	flight_stack_variable var;
	var.variable_name = var_token.token_str;
	var.value = "";
	var.block_level = vmdata.now_block_level;
	var.stack_level = vmdata.now_stack_level;
	var.type = flight_data_type::FDT_NONE;

	// �X�^�b�N�̈��push
	return flight_vm_push(vmdata, var);
}


//=============================================================================
// <static functions> :: flight_vm_make_global_var
//=============================================================================
static flight_vm_status flight_vm_make_global_var(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �ϐ��g�[�N��
	const auto var_token = inst->root->left->token;

	// �ϐ����쐬
	flight_variable var;
	var.variable_name = var_token.token_str;
	var.value = "";
	var.type = flight_data_type::FDT_NONE;

	// ����ϐ����̊m�F
	const auto f = [&](const auto& el) { return el.variable_name == var_token.token_str; };
	const auto& it = std::find_if(vmdata.global_variables.begin(), vmdata.global_variables.end(), f);
	if (it != vmdata.global_variables.end())
	{
		// ����ϐ��������݂���ꍇ�̓G���[
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_declare_variable;
		vmdata.error_token = { vmdata.now_line_no, var.variable_name };
		return flight_vm_status::VM_ERROR;
	}

	// �O���[�o���ϐ��ɒǉ�
	vmdata.global_variables.push_back(var);

#ifdef _DEBUG
	char a[0x100];
	sprintf_s(a, "create global variable = %s", var.variable_name.c_str());
	flight_vm_debug_state(vmdata, a);
#endif

	// ����
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_if
//=============================================================================
static flight_vm_status flight_vm_if(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �e��X�e�[�g�����g���擾
	const fl_ptr root = inst->root;
	const fl_ptr cond = root != nullptr ? inst->root->left : nullptr;
	const fl_ptr elsc = root != nullptr ? inst->root->right : nullptr;

	// �e��X�e�[�g�����g��null�������ꍇ�̓G���[
	if (root == nullptr || cond == nullptr || elsc == nullptr) { return flight_vm_status::VM_ERROR; }

	// �ꎞ�ϐ��̍쐬
	flight_variable cond_var;
	const auto status1 = flight_vm_exec_recursive(vmdata, cond_var, cond);
	if (status1 != flight_vm_status::VM_EXEC) { return status1; }

	// �����ϐ��̌��ʂ��擾
	if (cond_var.type != flight_data_type::FDT_VARNUMBER || cond_var.value.length() <= 0)
	{
		// ���������l����Ȃ�����
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

	// �U�����Ȃ烉�x���փW�����v
	if (cond_var.value == "0")
	{
		return flight_vm_goto(vmdata, elsc->token.token_str, elsc->token.type == flight_token_type::FTK_GOTOLABEL);
	}
	// �^�����Ȃ玟�֐i��
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
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �e��X�e�[�g�����g���擾
	const fl_ptr root = inst->root;
	const fl_ptr retv = root != nullptr ? inst->root->left : nullptr;

	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// �߂�ϐ�
	flight_variable ret_var;
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";

	// return value������ꍇ�͕ϐ����擾����
	if (retv != nullptr)
	{
		// �ꎞ�ϐ��̍쐬
		const auto status1 = flight_vm_exec_recursive(vmdata, ret_var, retv);
		if (status1 != flight_vm_status::VM_EXEC) { return status1; }

		// �����ϐ��̌��ʂ��擾
		if (ret_var.type != flight_data_type::FDT_VARNUMBER || ret_var.value.length() <= 0)
		{
			vmdata.error_line_no = vmdata.now_line_no;
			vmdata.error = flight_errors::error_return_value;
			vmdata.error_token = { vmdata.now_line_no, retv->token.token_str };
			return flight_vm_status::VM_ERROR;
		}
	}
	
	// �֐����s���ȊO�������ꍇ�͂��̂܂܃v���O�������I��������
	if (vmdata.now_stack_level <= 0)
	{
		// �z�X�g�ɕԂ��f�[�^�Ƃ��Ēl��ݒ肷��
		vmdata.exit_var = ret_var;

		vmdata.exit_flg = true;
		return flight_vm_status::VM_SUSPEND;
	}

	// �A�L�������[�^�ɏ��ݒ�
	flight_variable& acm = vmdata.accumulator;
	acm.value = ret_var.value;
	acm.type = ret_var.type;

	// �֐��̏I���܂ŃW�����v
	return flight_vm_goto(vmdata, "func-end", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_yield
//=============================================================================
static flight_vm_status flight_vm_yield(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �e��X�e�[�g�����g���擾
	const fl_ptr root = inst->root;
	const fl_ptr retv = root != nullptr ? inst->root->left : nullptr;

	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// �߂�ϐ�
	flight_variable ret_var;
	ret_var.type = flight_data_type::FDT_NONE;
	ret_var.value = "";

	// return value������ꍇ�͕ϐ����擾����
	if (retv != nullptr)
	{
		// �ꎞ�ϐ��̍쐬
		const auto status1 = flight_vm_exec_recursive(vmdata, ret_var, retv);
		if (status1 != flight_vm_status::VM_EXEC) { return status1; }
	}

	// yield�̏ꍇ���߂�l��ݒ�
	vmdata.exit_var = ret_var;

	// �v���O�����J�E���^�[��i�߂�(yeild����̕��A��)
	++vmdata.program_counter;

	// ���f���߂�Ԃ�
	return flight_vm_status::VM_SUSPEND;
}


//=============================================================================
// <static functions> :: flight_vm_break
//=============================================================================
static flight_vm_status flight_vm_break(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �e��X�e�[�g�����g���擾
	const fl_ptr root = inst->root;
	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// ���[�v�̏I���܂ŃW�����v
	return flight_vm_goto(vmdata, "loop-end", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_continue
//=============================================================================
static flight_vm_status flight_vm_continue(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �e��X�e�[�g�����g���擾
	const fl_ptr root = inst->root;
	if (root == nullptr) { return flight_vm_status::VM_ERROR; }

	// ���[�v�̏I���܂ŃW�����v
	return flight_vm_goto(vmdata, "loop-continue", true, false);
}


//=============================================================================
// <static functions> :: flight_vm_instruction
//=============================================================================
static flight_vm_status flight_vm_instruction(flight_vm_data& vmdata)
{
	// ���߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// �g�[�N���̎擾
	const auto& root_token = inst->root->token;
	flight_vm_status ret = flight_vm_status::VM_ERROR;

	// �ϐ��錾�������ꍇ
	if (root_token.token_str == "var")
	{
		// �u���b�N���x�����X�^�b�N���x����0�̏ꍇ�̓O���[�o���ϐ�
		if (vmdata.now_block_level == 0 && vmdata.now_stack_level == 0)
		{
			ret = flight_vm_make_global_var(vmdata);
		}
		// �����ɗ����ꍇ�̓��[�J���ϐ�
		else
		{
			ret = flight_vm_make_stack_var(vmdata);
		}

		// �v���O�����J�E���^�[��i�߂�
		++vmdata.program_counter;
	}
	// if�u���b�N�������ꍇ
	else if (root_token.token_str == "if")
	{
		ret = flight_vm_if(vmdata);
	}
	// return�u���b�N�������ꍇ
	else if (root_token.token_str == "return")
	{
		ret = flight_vm_return(vmdata);
	}
	// break�������ꍇ
	else if (root_token.token_str == "break")
	{
		ret = flight_vm_break(vmdata);
	}
	// continue�������ꍇ
	else if (root_token.token_str == "continue")
	{
		ret = flight_vm_continue(vmdata);
	}
	// yield�������ꍇ
	else if (root_token.token_str == "yield")
	{
		// return�Ɠ��l�̏������s��
		ret = flight_vm_yield(vmdata);
	}
	// �A�L�������[�^�̃��Z�b�g
	else if (root_token.token_str == "__RESET_ACCUM__!")
	{
		vmdata.accumulator.type = flight_data_type::FDT_NONE;
		vmdata.accumulator.value = "";

		// �v���O�����J�E���^�[��i�߂�
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

	// �u���b�N���x����1�グ��
	++vmdata.now_block_level;
}


//=============================================================================
// <static functions> :: flight_vm_exec_eblock
//=============================================================================
static flight_vm_status flight_vm_exec_eblock(flight_vm_data& vmdata)
{
	// �u���b�N���x����1�グ��
	if (--vmdata.now_block_level < 0)
	{
		// �G���[ �I���u���b�N����ɗ��Ă��܂��Ă���B
		vmdata.error_line_no = vmdata.now_line_no;
		vmdata.error = flight_errors::error_syntax;
		vmdata.error_token = { vmdata.now_line_no , "}" , };
		return flight_vm_status::VM_ERROR;
	}

	// �u���b�N���x���ȉ��̕ϐ����폜
	int& sp = vmdata.stack_pointer;
	const auto& sv = vmdata.stack_variables;
	for (; sp < (int)sv.size(); sp++)
	{
		// �X�^�b�N���x�����u���b�N���x������̂��̂��폜���Ă���
		// (�u���b�N���x���Ɋւ��ẮA�{�R�[�h�̏�Ńf�N�������g���Ă��邽�߁A
		//  �X�^�b�N���Œ��������̂��폜���Ă����B)
		if (sv[sp].block_level > vmdata.now_block_level && sv[sp].stack_level >= vmdata.now_stack_level)
		{
#ifdef _DEBUG
			char a[0x100];
			sprintf_s(a, "delete local variable = %s", sv[sp].variable_name.c_str());
			flight_vm_debug_state(vmdata, a);
#endif
			continue;
		}

		// �X�^�b�N�|�C���^�������肫�����ׁA���[�v�𔲂���
		break;
	}

#ifdef _DEBUG
	flight_vm_debug_state(vmdata, "end block");
#endif

	// �����Ƃ���
	return flight_vm_status::VM_EXEC;
}


//=============================================================================
// <static functions> :: flight_vm_exec_inner
//=============================================================================
static flight_vm_status flight_vm_exec_inner(flight_vm_data& vmdata)
{
	// ���݂̖��߂��擾
	const auto& inst = flight_vm_get_now_instruction(vmdata);

	// ���݂̍s�ԍ���ݒ�
	vmdata.now_line_no = inst->root->token.line_no;

	// �g�[�N���̎擾
	const auto& root_token = inst->root->token;
	flight_vm_status ret = flight_vm_status::VM_EXEC;

	// �\��ꂾ�����ꍇ�̏���
	if (root_token.type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		ret = flight_vm_instruction(vmdata);
	}
	// �u���b�N�J�n����
	else if (root_token.type == flight_token_type::FTK_BLOCK)
	{
		flight_vm_exec_sblock(vmdata);

		// �v���O�����J�E���^�[�����ɐi�߂�
		++vmdata.program_counter;
	}
	// �u���b�N�I������
	else if (root_token.type == flight_token_type::FTK_ENDBLOCK)
	{
		// �����ŃG���[����������ꍇ������B
		ret = flight_vm_exec_eblock(vmdata);

		if (ret == flight_vm_status::VM_EXEC)
		{
			// �֐��̏I���������ꍇ
			if (vmdata.now_block_level == 0 && vmdata.now_stack_level > 0)
			{
				ret = flight_vm_exit_func(vmdata);
			}
			// �v���O�����J�E���^�[�����ɐi�߂�
			else
			{
				++vmdata.program_counter;
			}
		}
	}
	// �W�����v����
	else if (root_token.type == flight_token_type::FTK_GOTOLABEL)
	{
		ret = flight_vm_goto(vmdata, root_token.token_str, true);
	}
	// ���ւ̃W�����v����
	else if (root_token.type == flight_token_type::FTK_GOTOBACKLABEL)
	{
		ret = flight_vm_goto(vmdata, root_token.token_str, false);
	}
	// ���x���������ꍇ�̏���
	else if (root_token.type == flight_token_type::FTK_LABEL)
	{
		// �������Ȃ��B�v���O�����J�E���^��i�߂ďI���
		++vmdata.program_counter;
	}
	// ���Z�q�������ꍇ
	else if (root_token.type == flight_token_type::FTK_OPERATION)
	{
		// �I�y���[�^���������s����
		ret = flight_vm_exec_recursive(vmdata, vmdata.accumulator, inst->root);
		++vmdata.program_counter;
	}
	// �֐��Ăяo���������ꍇ
	else if (root_token.type == flight_token_type::FTK_FUNCTION)
	{
		// �A�L�������[�^�ɏ���ݒ�
		ret = flight_vm_func(vmdata, vmdata.accumulator, inst->root);
	}
	// �s���ȏ���
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
	// �I���t���O�����Z�b�g
	vmdata.exit_var.value = "";
	vmdata.exit_var.type = flight_data_type::FDT_NONE;

	while (!vmdata.exit_flg)
	{
		// ���Ɏ��s�I�����Ă��邩�m�F
		const auto& insts = *vmdata.instructions;
		if (vmdata.program_counter >= (int)insts.size())
		{
			// �S�R�[�h���I����ăX�^�b�N���x�����u���b�N���x����0����Ȃ��ꍇ�̓G���[
			if (vmdata.now_block_level != 0 || vmdata.now_stack_level != 0)
			{
				// �v���I�ȃG���[
				setter(-1, flight_errors::fatal_error, "");
			}

			// �I���t���O��true��
			vmdata.exit_flg = true;
			break;
		}

		// ���߂����s(�v���O�����J�E���^�[�͓����Ői�߂�)
		const auto ret = flight_vm_exec_inner(vmdata);

		// �G���[��ݒ�
		if (vmdata.error != 0)
		{
			setter(vmdata.error_line_no, vmdata.error, vmdata.error_token.token_str.c_str());
			break;
		}

		// ���߂����s��ԈȊO��break�Ŕ�����
		if (ret != flight_vm_status::VM_EXEC) { break; }
	}
}