//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//-----------------------------------------------------------------------------
// 
// ������̗���
//    flight_tree_generater --> flight_make_instruction -> flight_parser_var -> flight_parser_recursive
//                                                      -> flight_parser_xxx -> flight_make_instruction
//																			 -> flight_parser_xxx
//                                                      -> flight_parser_recursive -> flight_parser_recursive
//  
//    ��}�̂悤��flight_make_instruction���� flight_paraser_xxx���ĂсA
//    ���̒�����ēx flight_make_instruction���ĂԁB
//
//    if�u���b�N�Ȃǂ̖��߂ɂ� {}�傩�����u���b�N�̐�ł����flight_make_instruction���ĂԂ��ƂŁA
//    tree���������܂�����悤�ɂ��Ă���B
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
	// �X�^�[�g���u���b�N����Ȃ��ꍇ�̓G���[
	if (src[0].type != flight_token_type::FTK_BLOCK)
	{
		gdata.error_line_no = src[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = src[0];
		return;
	}

	int level = 0;

	// �g�[�N���T�C�Y�����[�v
	int index = 0;
	for (; index < (int)src.size(); index++)
	{
		// "{"��"}"�ŊK�w��t���Ă���
		if (src[index].type == flight_token_type::FTK_BLOCK) { ++level; }
		else if (src[index].type == flight_token_type::FTK_ENDBLOCK) { --level; }

		// ���x����0�̏ꍇ
		if (level == 0 && src[index].type == flight_token_type::FTK_ENDBLOCK) { break; }

		// ���x����0�ȉ��Ȃ�G���[
		if (level < 0)
		{
			gdata.error_line_no = src[0].line_no;
			gdata.error = flight_errors::error_not_found_closing_block;
			gdata.error_token = src[0];
			return;
		}
	}

	// start_index���R���e�i�̃T�C�Y�ȏ�Ȃ璆�f
	if (index >= (int)src.size()) { return; }

	// �ꎞ�̈�Ƀf�[�^�����āA���̃g�[�N������폜����B
	dest.insert(dest.begin(), src.begin(), src.begin() + index + 1); // �傩�����I�����܂�
	src.erase(src.begin(), src.begin() + index + 1);				 // �傩�����I���̎�����X�^�[�g�Ƃ���
}


//=============================================================================
// <static functions> :: flight_has_invalid_ins_tokens
//=============================================================================
static const bool flight_has_invalid_ins_tokens(const std::vector<flight_token>& tokens)
{
	// for��while��if�̊��ʓ��Ŗ����ȃg�[�N��������ꍇ��true��Ԃ�
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		// ���Z�q�������ꍇ��OK
		if (tokens[i].type == flight_token_type::FTK_OPERATION) { continue; }

		// �ϐ���OK
		if (tokens[i].type == flight_token_type::FTK_VARIABLE) { continue; }

		// �֐���OK
		if (tokens[i].type == flight_token_type::FTK_FUNCTION) { continue; }

		// �Z�~�R������OK
		if (tokens[i].type == flight_token_type::FTK_ENDINSTRUCTION) { continue; }

		// ����������OK
		if (tokens[i].type == flight_token_type::FTK_PSES ||
			tokens[i].type == flight_token_type::FTK_ENDPSES) {
			continue;
		}

		// �\���̂���var��OK
		if (tokens[i].type == flight_token_type::FTK_INSTRUCTION_RESERVED &&
			tokens[i].token_str == "var") {
			continue;
		}

		// �E�ӂ̗\����OK
		if (tokens[i].type == flight_token_type::FTK_VALUE_RESERVED) { continue; }

		// �E�Ӓl��OK
		if (tokens[i].type == flight_token_type::FTK_VALUE_NUMBER ||
			tokens[i].type == flight_token_type::FTK_VALUE_CHAR ||
			tokens[i].type == flight_token_type::FTK_VALUE_STR) {
			continue;
		}

		// �����܂ŗ����ꍇ�͎��s
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

	// �g�[�N���T�C�Y�����[�v
	int index = start_index;
	for (; index < (int)tokens.size(); index++)
	{
		// "("��")"�ŊK�w��t���Ă���
		if (tokens[index].type == flight_token_type::FTK_PSES) { ++level; }
		else if (tokens[index].type == flight_token_type::FTK_ENDPSES) { --level; }

		// ���x����0�̏ꍇ
		if (level == 0 && tokens[index].type == type) { break; }

		// ���x����0�ȉ��Ȃ�G���[
		if (level < 0)
		{
			gdata.error_line_no = tokens[index].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[index];
			return -1;
		}
	}

	// start_index���R���e�i�̃T�C�Y�ȏ�Ȃ璆�f
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
	// �w�肵��type�̈ʒu������
	const auto index = flight_search_toplevel(gdata, src, type);

	// type�̈ʒu�܂ŃR�s�[ ���� ���f�[�^ ���� �폜
	std::vector<flight_token> tmp_tokens;
	if (index < 0)
	{
		gdata.error_line_no = src[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = src[0];
		return;
	}

	// �ꎞ�̈�Ƀf�[�^�����āA���̃g�[�N������폜����B
	dest.insert(dest.begin(), src.begin(), src.begin() + index + 1); // �����Ώۂ��܂�
	src.erase(src.begin(), src.begin() + index + 1);
}


//=============================================================================
// <static functions> :: flight_make_gotolabel_node
//=============================================================================
static fl_ptr flight_make_gotolabel_node(
	const char* sig, const int sigid,
	const int countid, const bool next = true)
{
	// �ϐ��錾
	const std::string label = std::string(sig) + "-" + std::to_string(sigid) + "-" + std::to_string(countid);
	const flight_token_type type = next ? flight_token_type::FTK_GOTOLABEL : flight_token_type::FTK_GOTOBACKLABEL;

	// node���쐬����
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { -1 , label , type };

	return ptr;
}


//=============================================================================
// <static functions> :: flight_make_instruction
//=============================================================================
static void flight_make_instruction(flight_generate_data& gdata, fl_ptr root)
{
	// ���߂�ǉ�
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
	// ���߂��쐬����
	flight_make_instruction(gdata, flight_make_gotolabel_node(sig, sigid, countid, next));
}


//=============================================================================
// <static functions> :: flight_make_label
//=============================================================================
static void flight_make_label(flight_generate_data& gdata, const char* sig, const int sigid, const int countid)
{
	// ���x����
	const std::string label = std::string(sig) + "-" + std::to_string(sigid) + "-" + std::to_string(countid);

	// ���x�����߂��쐬����
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { -1 , label , flight_token_type::FTK_LABEL };
	flight_make_instruction(gdata, ptr);
}


//=============================================================================
// <static functions> :: flight_insert_startblock
//=============================================================================
static void flight_insert_startblock(flight_generate_data& gdata, const int line_no)
{
	// �n�܂芇�ʂ��쐬����
	fl_ptr ptr = std::make_shared<flight_token_node>();
	ptr->token = { line_no , "{" , flight_token_type::FTK_BLOCK };
	flight_make_instruction(gdata, ptr);
}


//=============================================================================
// <static functions> :: flight_insert_endblock
//=============================================================================
static void flight_insert_endblock(flight_generate_data& gdata, const int line_no)
{
	// �I��芇�ʂ��쐬����
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
		// ���X�g�̗\��ꖽ�߂ƈ�v����ꍇ�A�C���f�b�N�X��Ԃ�
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
	// �����l
	static const int initial_value = 0x7FFFFFFF;

	// �D��x
	int priority = initial_value;
	int target_index = -1;

	// �g�[�N������
	int now_index = -1;
	for (; now_index < (int)tokens.size();)
	{
		// �������s��
		const int search_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_OPERATION, now_index + 1);
		if (search_index < 0) { break; }

		// �����C���f�b�N�X���㏑��
		now_index = search_index;

		// �D��x���Ⴂ���Z�q���A�\���؂̏�ɂ���
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

		// �D��x���`�F�b�N
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
	// �g�[�N����ݒ�
	fl_ptr call_root = std::make_shared<flight_token_node>();
	call_root->token = tokens[0];
	call_root->token.type = flight_token_type::FTK_FUNCTION;
	std::vector<flight_token> token_l(tokens.begin() + 1, tokens.end());
	call_root->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);

	// �ϐ���
	const std::string var_name = std::string("___TMP_VARIABLE___!") + std::to_string(gdata.sigunature++);

	// �g�[�N���쐬
	flight_token semicolon_token;
	semicolon_token.line_no = tokens[0].line_no;
	semicolon_token.type = flight_token_type::FTK_ENDINSTRUCTION;
	semicolon_token.token_str = ";";

	flight_token var_token;
	var_token.line_no = tokens[0].line_no;
	var_token.type = flight_token_type::FTK_VARIABLE;
	var_token.token_str = var_name;

	// �g�b�v�m�[�h����Ȃ��ꍇ�ϐ���ǉ�����
	if (recursive_level > 0)
	{
		// �ϐ��̒ǉ�
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

	// �֐��Ăяo���̒ǉ�
	flight_make_instruction(gdata, call_root);

	// �g�b�v�m�[�h����Ȃ��ꍇ
	if (recursive_level > 0)
	{
		// �ϐ��ւ̑����ǉ�
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

		// �ϐ��̑��������ǉ�
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
	// �g�[�N���T�C�Y���Ȃ��ꍇ��G���[�����݂���ꍇ�̓G���[
	if (tokens.size() <= 0 || gdata.error != 0) { return nullptr; }

	// �����\��ꂾ�����ꍇ�͉��ɍs��
	if (tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		// �\��ꂪ�ċA�������ɏo�����邱�Ƃ͂Ȃ�
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return nullptr;
	}

	// �傩�����J�n��I���̓u���b�N�J�n
	if (tokens[0].type == flight_token_type::FTK_BLOCK || tokens[0].type == flight_token_type::FTK_ENDBLOCK)
	{
		// �G���[ �傩�������ċA�������ɏo�����邱�Ƃ͂Ȃ�
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return nullptr;
	}

	// �g�[�N���̍ŏ��ƍŌオ"("�� ")"�̏ꍇ�A�Ȃ�
	if (tokens.begin()->type == flight_token_type::FTK_PSES &&
		tokens.rbegin()->type == flight_token_type::FTK_ENDPSES)
	{
		tokens.erase(tokens.begin());
		tokens.erase(tokens.end() - 1);
	}

	// �g�[�N�����Ȃ��ꍇ�͉������Ȃ�
	if (tokens.size() <= 0) { return nullptr; }

	// �����g�[�N����1�����������ꍇ
	if (tokens.size() == 1)
	{
		// �g�[�N����ݒ�
		fl_ptr node = std::make_shared<flight_token_node>();
		node->token = tokens[0];
		return node;
	}

	// ���Z�q�̏��� 
	// var a = ( 3 + 4 ) * 2 , b = ( 2 * 4 ) + 3; �݂����ȏ����ɑΉ��ł���悤��
	// �܂����ʊO���猟������B
	const int op_index = flight_lowp_operator(gdata, tokens);
	const int comma_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_SEPARATOR);

	// �^�[�Q�b�g�C���f�b�N�X���擾
	int target_index = -1;
	if (op_index >= 0) { target_index = op_index; }
	if (comma_index >= 0) { target_index = comma_index; }

	// �g�b�v���x���Ńg�[�N�����o�Ă����ꍇ
	if (target_index >= 0)
	{
		// �g�[�N����ݒ�
		fl_ptr node = std::make_shared<flight_token_node>();
		node->token = tokens[target_index];

		// �I�̏ꍇ�͓���(left�݂̂Ƀf�[�^��ݒ肷��)
		if (node->token.token_str == "!")
		{
			// ���r���[�Ȉʒu�ɁI�}�[�N���o�Ă����ꍇ�̓G���[
			if (target_index != 0)
			{
				// �G���[ ���r���[�Ȉʒu�Ƀr�b�N���}�[�N���o�Ă���
				gdata.error_line_no = tokens[target_index].line_no;
				gdata.error = flight_errors::error_syntax;
				gdata.error_token = tokens[target_index];
				return nullptr;
			}

			// �f�[�^�̐ݒ�
			std::vector<flight_token> token_l(tokens.begin() + 1, tokens.end());
			node->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);
		}
		else
		{
			// �����������s��
			std::vector<flight_token> token_l(tokens.begin(), tokens.begin() + target_index);
			std::vector<flight_token> token_r(tokens.begin() + target_index + 1, tokens.end());
			node->left = flight_parser_recursive(gdata, token_l, recursive_level + 1);
			node->right = flight_parser_recursive(gdata, token_r, recursive_level + 1);

			// �����g�[�N��������ł���ꍇ ���� ���ӂ��ϐ��ȊO�������ꍇ�̓G���[
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
	// �g�b�v���x���ŕ������o�Ă��Ȃ������ꍇ
	else
	{
		// �֐��̏ꍇ
		if (tokens[0].type == flight_token_type::FTK_VARIABLE &&
			tokens[1].type == flight_token_type::FTK_PSES &&
			tokens.rbegin()->type == flight_token_type::FTK_ENDPSES)
		{
			return flight_parser_call_func(gdata, tokens, recursive_level);
		}
	}

	// �����܂ŗ����ꍇ�͖����ȃg�[�N�����o�ꂵ��(�C���f�b�N�X��0����Ȃ�1�ڂɎw��)
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
	// func���̊m�F
	if (tokens.size() < 5 || tokens[0].token_str != "func" || tokens[1].type != FTK_VARIABLE)
	{
		// �G���[����
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// ���ʎq���擾
	const int signature = gdata.sigunature++;

	// �擪�v�f�̍폜(func���̍폜)
	const auto func_token = tokens[0];
	const auto name_token = tokens[1];
	tokens.erase(tokens.begin(), tokens.begin() + 2);

	// PSES
	if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
	{
		// �G���[ func�̌�͊��ʂ��K�v
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// funcname�m�[�h�̍쐬
	auto func_name = std::make_shared<flight_token_node>();
	func_name->token = name_token;

	// func�m�[�h�̍쐬
	auto func_root = std::make_shared<flight_token_node>();
	func_root->token = func_token;
	func_root->left = func_name;
	func_root->right = nullptr;

	// ()�̊Ԃ��擾����
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// ()���폜����
	tmp_tokens.erase(tmp_tokens.begin());
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// ��������var������
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

		// ���̃C���f�b�N�X�֑J��
		++now_index;
	}

	// �������X�g�̃`�F�b�N
	for (int i = 0; i < (int)arg_tokens.size(); i++)
	{
		// �ϐ� �܂��� �J���}�ȊO�������ꍇ�G���[�ƂȂ�
		if (!(arg_tokens[i].type == FTK_VARIABLE || arg_tokens[i].type == FTK_SEPARATOR))
		{
			gdata.error_line_no = arg_tokens[i].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = arg_tokens[i];
			return;
		}
	}

	// �������X�g���m�[�h�ɐݒ�
	func_root->right = flight_parser_recursive(gdata, arg_tokens);

	// �֐��O�Ɋ֐��I���܂ŃW�����v���鏈����ǉ�����
	flight_make_gotolabel(gdata, (name_token.token_str + "-exit").c_str(), signature, 0);

	// �֐��̊J�n������ǉ�
	flight_make_instruction(gdata, func_root);

	// �J�n�u���b�N��ǉ�����B
	flight_insert_startblock(gdata, tokens[0].line_no);

	// �傩�������擾
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// �傩�������̏������쐬
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// �A�L�������[�^�̃��Z�b�g������ǉ�
	auto reset_root = std::make_shared<flight_token_node>();
	reset_root->token.line_no = -1;
	reset_root->token.token_str = "__RESET_ACCUM__!";
	reset_root->token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;
	reset_root->left = nullptr;
	reset_root->right = nullptr;
	flight_make_instruction(gdata, reset_root);

	// ���x���������쐬
	flight_make_label(gdata, "func-end", signature, 0);

	// �I���u���b�N��ǉ�����B
	flight_insert_endblock(gdata, -1);

	// �֐��u���b�N�I���̃��x�����w��
	flight_make_label(gdata, (name_token.token_str + "-exit").c_str(), signature, 0);
}


//=============================================================================
// <static functions> :: flight_parser_while
//=============================================================================
void flight_parser_while(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// while���̊m�F
	if (tokens.size() < 5 || tokens[0].token_str != "while" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// �J�n�u���b�N��ǉ�����B
	flight_insert_startblock(gdata, tokens[0].line_no);

	// ���ʎq���擾
	const int signature = gdata.sigunature++;

	// �擪�v�f�̍폜(while���̍폜)
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

	// ()�̊Ԃ��擾����
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// �g�[�N�����擾
	std::vector<flight_token> cond_tokens = tmp_tokens;
	if (flight_has_invalid_ins_tokens(cond_tokens))
	{
		// �G���[ �����ȃg�[�N����������
		gdata.error_line_no = cond_tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = cond_tokens[0];
		return;
	}

	// ���x���̐ݒ�
	flight_make_label(gdata, "loop-start", signature, 0);

	// ���[�v�̉���
	auto cond_node = flight_parser_recursive(gdata, cond_tokens);
	if (gdata.error != 0) { return; }

	// if�g�[�N���̍쐬
	flight_token if_token;
	if_token.line_no = while_token.line_no;
	if_token.token_str = "if";
	if_token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;

	// ���[�v�̏������쐬(�U�����Ȃ�loop_end�܂ŃW�����v)
	auto cond_root = std::make_shared<flight_token_node>();
	cond_root->token = if_token;
	cond_root->left = cond_node;
	cond_root->right = flight_make_gotolabel_node("loop-end", signature, 0);
	flight_make_instruction(gdata, cond_root);

	// �傩�������擾
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// �傩�������̏������쐬
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ���x���̐ݒ�
	flight_make_label(gdata, "loop-continue", signature, 0);

	// ���Ɉړ�����
	flight_make_gotolabel(gdata, "loop-start", signature, 0, false);

	// ���x���������쐬
	flight_make_label(gdata, "loop-end", signature, 0);

	// �I���u���b�N��ǉ�����B
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <static functions> :: flight_parser_for
//=============================================================================
void flight_parser_for(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// for���̊m�F
	if (tokens.size() < 5 || tokens[0].token_str != "for" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// �J�n�u���b�N��ǉ�����B
	flight_insert_startblock(gdata, tokens[0].line_no);

	// ���ʎq���擾
	const int signature = gdata.sigunature++;

	// �擪�v�f�̍폜(for���̍폜)
	const auto for_token = tokens[0];
	tokens.erase(tokens.begin());

	// PSES
	if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
	{
		// �G���[ for�̌�͊��ʂ��K�v
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// ()�̊Ԃ��擾����
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);
	if (gdata.error != 0) { return; }

	// ()���폜����
	tmp_tokens.erase(tmp_tokens.begin());
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// �����l�E���[�v�E�J�E���^�[�̃g�[�N�����擾
	std::vector<flight_token> initial_tokens, condition_token, counter_token;
	flight_move_tokens(gdata, initial_tokens, tmp_tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }
	flight_move_tokens(gdata, condition_token, tmp_tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// �Ō�ɃZ�~�R������t����
	counter_token = tmp_tokens;
	counter_token.push_back(*condition_token.rbegin());

	// �����ȃg�[�N�����m�F
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
				// �G���[ �����ȃg�[�N����������
				gdata.error_line_no = v[0].line_no;
				gdata.error = flight_errors::error_syntax;
				gdata.error_token = v[0];
				return;
			}
		}
	}

	// �����l�̉���
	while (initial_tokens.size() > 0)
	{
		flight_make_instruction(gdata, initial_tokens);
		if (gdata.error != 0) { return; }
	}

	// ���x���̐ݒ�
	flight_make_label(gdata, "loop-start", signature, 0);

	// ���[�v�̉���
	condition_token.erase(condition_token.end() - 1);
	auto node = flight_parser_recursive(gdata, condition_token);
	if (gdata.error != 0) { return; }

	// if�g�[�N���̍쐬
	flight_token if_token;
	if_token.line_no = for_token.line_no;
	if_token.token_str = "if";
	if_token.type = flight_token_type::FTK_INSTRUCTION_RESERVED;

	// ���[�v�̏������쐬(�U�����Ȃ�loop_end�܂ŃW�����v)
	auto cond_root = std::make_shared<flight_token_node>();
	cond_root->token = if_token;
	cond_root->left = node;
	cond_root->right = flight_make_gotolabel_node("loop-end", signature, 0);
	flight_make_instruction(gdata, cond_root);

	// �傩�������擾
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// �傩�������̏������쐬
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ���x���̐ݒ�
	flight_make_label(gdata, "loop-continue", signature, 0);

	// �J�E���^�[�̉���
	while (counter_token.size() > 0)
	{
		flight_make_instruction(gdata, counter_token);
		if (gdata.error != 0) { return; }
	}

	// ���Ɉړ�����
	flight_make_gotolabel(gdata, "loop-start", signature, 0, false);

	// ���x���������쐬
	flight_make_label(gdata, "loop-end", signature, 0);

	// �I���u���b�N��ǉ�����B
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <static functions> :: flight_parser_if_inner
//=============================================================================
static void flight_parser_if_inner(
	flight_generate_data& gdata, std::vector<flight_token>& tokens,
	const int signature, const int count)
{
	// �g�[�N���̈ꎞ�t�@�C����ǉ�
	std::vector<flight_token> tmp_tokens;

	// if�u���b�N�̏ꍇ��()���܂܂��ׁA�������擾����
	if (tokens[0].token_str == "if")
	{
		// if�g�[�N���̎擾
		const auto if_token = tokens[0];

		// if�L�[���[�h���O��
		tokens.erase(tokens.begin());

		// PSES
		if (tokens[0].type != flight_token_type::FTK_PSES || tokens.size() < 3)
		{
			// �G���[ for�̌�͊��ʂ��K�v
			gdata.error_line_no = tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[0];
			return;
		}

		// ()�̊Ԃ��擾����
		flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDPSES);

		// �����ȃg�[�N�����m�F
		if (flight_has_invalid_ins_tokens(tmp_tokens))
		{
			// �G���[ �����ȃg�[�N����������
			gdata.error_line_no = tmp_tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tmp_tokens[0];
			return;
		}

		// if�̊��ʓ��ŕϐ��錾������Ă���ꍇ�A���߂��쐬����
		if (tmp_tokens[1].token_str == "var")
		{
			flight_make_instruction(gdata, tmp_tokens);
		}

		// �������
		auto condition = flight_parser_recursive(gdata, tmp_tokens);
		auto else_ins = flight_make_gotolabel_node("if", signature, count);

		// if�p��node���쐬����
		fl_ptr if_node = std::make_shared<flight_token_node>();
		if_node->token = if_token;
		if_node->left = condition;
		if_node->right = else_ins;

		// ���߂�ǉ�
		flight_make_instruction(gdata, if_node);
	}

	// �傩�����̓��e�擾
	tmp_tokens.clear();
	flight_move_toplevel_block(gdata, tmp_tokens, tokens);
	if (gdata.error != 0) { return; }

	// �傩�������̏������쐬
	while (tmp_tokens.size() > 0)
	{
		flight_make_instruction(gdata, tmp_tokens);
		if (gdata.error != 0) { return; }
	}

	// ���x���W�����v�������쐬
	flight_make_gotolabel(gdata, "if-end", signature, 0);
	flight_make_label(gdata, "if", signature, count);
}


//=============================================================================
// <global functions> :: flight_parser_if
//=============================================================================
void flight_parser_if(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// if���̊m�F(���b�Z�[�W�̍ŏ���else�̏ꍇ�A�G���[)
	if (tokens.size() < 5 || tokens[0].token_str != "if" || tokens[1].type != flight_token_type::FTK_PSES)
	{
		gdata.error_line_no = tokens[0].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tokens[0];
		return;
	}

	// �J�n�u���b�N��ǉ�����B
	flight_insert_startblock(gdata, tokens[0].line_no);

	// ���ʎq���擾
	const int signature = gdata.sigunature++;
	int count = 0;

	// if�u���b�N�̃��[�v����
	while (true)
	{
		// �����I��if����
		flight_parser_if_inner(gdata, tokens, signature, count++);

		// �G���[���������Ă���ꍇ��������
		if (gdata.error != 0) { return; }

		// �g�[�N�����I�����Ă���ꍇ�͔�����
		if (tokens.size() <= 0) { break; }

		// if�u���b�N ���� else �u���b�N�łȂ��ꍇ��break
		if (!(tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED && tokens[0].token_str == "else")) { break; }

		// �����܂ł���ꍇ��else�̏ꍇ
		tokens.erase(tokens.begin());
	}

	// ���x���������쐬
	flight_make_label(gdata, "if-end", signature, 0);

	// �I���u���b�N��ǉ�����B
	flight_insert_endblock(gdata, -1);
}


//=============================================================================
// <global functions> :: flight_parser_var
//=============================================================================
void flight_parser_var(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// var�g�[�N���̃o�b�N�A�b�v
	flight_token var_token = tokens[0];

	// �擪�v�f�̍폜
	tokens.erase(tokens.begin());

	// �J���}��؂�
	while (true)
	{
		// �ϐ��ƃZ�~�R���������킹�ď��Ȃ��Ƃ�2�ȏ�̃g�[�N�������݂��Ȃ��ꍇ�̓G���[
		if (tokens.size() < 2)
		{
			// �G���[ for�̌�͊��ʂ��K�v
			gdata.error_line_no = var_token.line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = var_token;
			return;
		}

		// �g�[�N�������A�g�[�N�����폜
		auto node = std::make_shared<flight_token_node>();
		node->token = var_token;
		node->left = std::make_shared<flight_token_node>();
		node->left->token = tokens[0];

		// var�����̕ϐ��錾���߂��쐬
		fi_ptr inst = std::make_shared<flight_instruction>();
		inst->root = node;
		gdata.container.push_back(inst);

		// ���̃J���}��؂�ƃZ�~�R�����C���f�b�N�X���擾
		const int semi_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_ENDINSTRUCTION);
		const int comma_index = flight_search_toplevel(gdata, tokens, flight_token_type::FTK_SEPARATOR);

		// �ŏ��̃C���f�b�N�X���擾����
		int next_index = -1;
		if (comma_index >= 0) { next_index = comma_index; }
		if (semi_index >= 0) { next_index = semi_index < next_index || comma_index < 0 ? semi_index : next_index; }

		// �Z�~�R�������J���}���K���o������͂��Ȃ̂ɁA�łȂ��ꍇ�̓G���[
		if (next_index < 0)
		{
			// �G���[ for�̌�͊��ʂ��K�v
			gdata.error_line_no = tokens[0].line_no;
			gdata.error = flight_errors::error_syntax;
			gdata.error_token = tokens[0];
			return;
		}

		// �Z�~�R�����܂Ŏ擾
		std::vector<flight_token> tmp_tokens;
		tmp_tokens.insert(tmp_tokens.end(), tokens.begin(), tokens.begin() + next_index + 1);
		tokens.erase(tokens.begin(), tokens.begin() + next_index + 1);

		// �J���}��Z�~�R�����̃C���f�b�N�X��1�ȏ�Ȃ牉�Z�q�������s��
		if (next_index >= 2)
		{
			// �Ōオ�J���}�������ꍇ�͍폜���s��
			if (tmp_tokens.rbegin()->type == flight_token_type::FTK_SEPARATOR ||
				tmp_tokens.rbegin()->type == flight_token_type::FTK_ENDINSTRUCTION)
			{
				tmp_tokens.erase(tmp_tokens.end() - 1);
			}

			// �ϐ��錾�Ɠ����ɍs���������������
			fi_ptr op = std::make_shared<flight_instruction>();
			op->root = flight_parser_recursive(gdata, tmp_tokens);

			// �G���[������ꍇ�͑��I��
			if (gdata.error != 0) { return; }

			if (op->root->left != nullptr || op->root->right != nullptr)
			{
				gdata.container.push_back(op);
			}
		}

		// �g�[�N���T�C�Y��0�ȉ� ���� �����Z�~�R�����������ꍇ�͔�����
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
	// �Z�~�R�����܂ł��擾
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// �Ō�̃Z�~�R�������폜
	tmp_tokens.erase(tmp_tokens.end() - 1);

	// �L�[���[�h��ݒ�
	auto node = std::make_shared<flight_token_node>();
	node->token = tmp_tokens[0];

	// �擪���폜
	tmp_tokens.erase(tmp_tokens.begin());

	// �ꎞ�g�[�N�������݂���ꍇ��left�ɐݒ�
	if (tmp_tokens.size() > 0)
	{
		node->left = flight_parser_recursive(gdata, tmp_tokens, 1);
	}

	// ���ߍ쐬
	flight_make_instruction(gdata, node);
}


//=============================================================================
// <global functions> :: flight_parser_simple
//=============================================================================
void flight_parser_simple(flight_generate_data& gdata, std::vector<flight_token>& tokens)
{
	// �Z�~�R�����܂ł��擾
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (gdata.error != 0) { return; }

	// 1�ڂ��Z�~�R�����łȂ��ꍇ�̓G���[
	if (tmp_tokens[1].type != flight_token_type::FTK_ENDINSTRUCTION)
	{
		gdata.error_line_no = tmp_tokens[1].line_no;
		gdata.error = flight_errors::error_syntax;
		gdata.error_token = tmp_tokens[1];
		return;
	}

	// ���ߍ쐬
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
	// �g�[�N���������Ȃ�����I��
	if (tokens.size() <= 0) { return; }

	// �����\��ꂾ�����ꍇ�͉��ɍs��
	if (tokens[0].type == flight_token_type::FTK_INSTRUCTION_RESERVED)
	{
		// �C���f�b�N�X���擾
		const int index = flight_is_reserved(tokens[0], _token_reserved_instruction_list, _token_reserved_instruction_list_size);

		// �C���f�b�N�X��0�ȏ�̏ꍇ
		if (index >= 0)
		{
			// �g�[�N�����̏����ɑJ��
			_token_reserved_instruction_list[index].parser(gdata, tokens);
			return;
		}
	}

	// �傩�����J�n��I���̓u���b�N�J�n
	if (tokens[0].type == flight_token_type::FTK_BLOCK || tokens[0].type == flight_token_type::FTK_ENDBLOCK)
	{
		// �g�[�N�������A�g�[�N�����폜
		auto node = std::make_shared<flight_token_node>();
		node->token = tokens[0];
		node->left = node->right = nullptr;
		tokens.erase(tokens.begin());

		// ���[�g��ݒ肵�R���e�i�ɒǉ�
		flight_make_instruction(gdata, node);
		return;
	}

	// �����܂ł���ꍇ�͉������疽�߂�����p�^�[��
	// �擪���\���̏ꍇ��傩�����̏ꍇ���������A
	// �֐��Ăяo���≉�Z�q��p�����v�Z���ȉ��ōs���B

	// �Z�~�R�����̈ʒu�܂ō폜
	std::vector<flight_token> tmp_tokens;
	flight_move_tokens(gdata, tmp_tokens, tokens, flight_token_type::FTK_ENDINSTRUCTION);
	if (tmp_tokens.size() > 0 && tmp_tokens.rbegin()->type == flight_token_type::FTK_ENDINSTRUCTION) {
		tmp_tokens.erase(tmp_tokens.end() - 1);
	}

	// �G���[������ꍇ�⏈���������Ȃ��ꍇ�͑��I��
	if (gdata.error != 0 || tmp_tokens.size() <= 0) { return; }

	// root��ݒ�
	fi_ptr inst = std::make_shared<flight_instruction>();
	inst->root = flight_parser_recursive(gdata, tmp_tokens, 0);

	// root���ϐ������������ꍇ�������Ȃ�
	// ���֐��̖߂�l���Z�b�g����ϐ����Ԃ��Ă��������̉\�������邽�߁B
	if (inst->root == nullptr) { return; }

	gdata.container.push_back(inst);
}


//=============================================================================
// <global functions> :: flight_tree_generater [syntax tree���쐬����]
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
	// �����f�[�^�̍쐬
	flight_generate_data gdata;
	gdata.error = flight_errors::error_none;
	gdata.error_line_no = -1;
	gdata.sigunature = signature;

	// �󖽗߂̒ǉ�
	flight_make_label(gdata, "program-start", gdata.sigunature++, 0);

	// ���߂̒ǉ�
	while (tokens.size() > 0)
	{
		// ���߂��쐬
		flight_make_instruction(gdata, tokens);

		// �G���[���������Ă���ꍇ�͒��f����
		if (gdata.error != 0)
		{
			setter(gdata.error_line_no, gdata.error, "");
			return;
		}
	}

	// �󖽗߂̒ǉ�
	flight_make_label(gdata, "program-end", gdata.sigunature++, 0);

	// �e�R���e�i�Ɍ���������
	container.insert(container.end(), gdata.container.begin(), gdata.container.end());

	// �e�f�[�^�ɒl��Ԃ�
	signature = gdata.sigunature;
}
