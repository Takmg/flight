//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <struct> :: token_info
//=============================================================================
typedef struct __token_info {
	int line_no;			// �s�ԍ�
	flight_errors error;	// �G���[
} token_info;


//=============================================================================
// typedefs
//=============================================================================
typedef int (*token_divider_func)(const char*, token_info&);
typedef std::string(*token_modify_func)(char*, token_info&);


//=============================================================================
// <structs> :: TOKEN_DIVIDER [�g�[�N�������p�\����]
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

	// ���s�R�[�h�̂����ꂩ���}�b�`���Ă����ꍇ�̓|�W�V������i�߂�B
	if (strncmp(raw_data, "\r\n", 2) == 0) { pos = 2; }
	else if (*raw_data == '\r') { pos = 1; }
	else if (*raw_data == '\n') { pos = 1; }

	// �i�߂�|�W�V������2�ȏ�Ȃ玟��
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
		// �c������̒����ȏ�ɗ\���̌��t�����������ꍇ�͔�r���Ȃ��B
		const int reserved_len = (int)strlen(list[i].instruction);
		if (reserved_len > str_size) { continue; }

		// ��r�������ʈ�v���Ă���ꍇ�͗\���Ƃ��ēo�^����B
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
	// ���l����Ȃ��ꍇ
	if (!isdigit(*raw_data)) { return 0; }

	// �����|�W�V����
	int pos = 1;		// ���̕�������f�[�^���擾
	if (strncmp(raw_data, "0x", 2) == 0) { ++pos; }

	// �S�s��ݒ�
	for (; *(raw_data + pos); pos++)
	{
		// ���l ���� �����_����Ȃ��ꍇ�A�����ŏI��
		const char d = raw_data[pos];

		// ���l �y�� �����_ �y�� �p���ȊO�Ȃ�continue
		if (isalnum(d) || d == '.') { continue; }

		// �����܂ł���ꍇ�̓G���[
		break;
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_variable
//=============================================================================
static int token_variable(const char* raw_data, token_info& info)
{
	// �p�����̓A���_�[�o�[�X�^�[�g�łȂ��ꍇ
	if (!isalpha(*raw_data) && raw_data[0] != '_') { return 0; }

	// �����|�W�V����
	int pos = 1;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; *(raw_data + pos); pos++)
	{
		// ���l ���� �����_����Ȃ��ꍇ�A�����ŏI��
		const char d = raw_data[pos];
		if (!isalnum(d) && d != '_') { break; }
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_value_char
//=============================================================================
static int token_value_char(const char* raw_data, token_info& info)
{
	// �V���O���N�H�[�g�X�^�[�g�łȂ��ꍇ
	if (*raw_data != '\'') { return 0; }

	// �����|�W�V����
	int pos = 1;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; *(raw_data + pos); )
	{
		// �r���ŉ��s�����܂��Ă��邩�H
		const int lpos = token_endline(raw_data + pos, info);
		if (lpos > 0) { pos += lpos; continue; }

		// "\'"�̏ꍇ�͕�������̕�����Ƃ��ĉ������Ȃ�
		if (strncmp(raw_data + pos, "\\\'", 2) == 0) { ++pos; }

		// �|�W�V�������ړ�
		++pos;

		// �V���O���N�H�[�g���o���ꍇ�͏I��
		if (raw_data[pos] == '\'') { break; }
	}

	// �����ŏI�s���V���O���N�H�[�g�������ꍇ
	if (raw_data[pos] == '\'') { ++pos; }

	// �ŏI�s���V���O���N�H�[�g�łȂ��ꍇ�̓G���[�ƂȂ�B
	else
	{
		info.error = flight_errors::error_notendchar;
		return -1;
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_value_str
//=============================================================================
static int token_value_str(const char* raw_data, token_info& info)
{
	// �_�u���N�H�[�g�X�^�[�g�łȂ��ꍇ
	if (*raw_data != '"') { return 0; }

	// �����|�W�V����
	int pos = 1;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; raw_data[pos]; )
	{
		// �r���ŉ��s�����܂��Ă��邩�H
		const int lpos = token_endline(raw_data + pos, info);
		if (lpos > 0) { pos += lpos; continue; }

		// "\""�̏ꍇ�͕�������̕�����Ƃ��ĉ������Ȃ�
		if (strncmp(raw_data + pos, "\\\"", 2) == 0) { ++pos; }

		// �|�W�V�������ړ�
		++pos;

		// �_�u���N�H�[�g���o���ꍇ�͏I��
		if (raw_data[pos] == '"') { break; }
	}

	// �����ŏI�s���_�u���N�H�[�g�������ꍇ
	if (raw_data[pos] == '"') { ++pos; }

	// �ŏI�s���_�u���N�H�[�g�łȂ��ꍇ�̓G���[�ƂȂ�B
	else
	{
		info.error = flight_errors::error_notendstring;
		return -1;
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_notcode
//=============================================================================
static int token_notcode(const char* raw_data, token_info& info)
{
	// 
	int pos = 0;

	// �S�s��ݒ�
	for (; raw_data[pos]; pos++)
	{
		// �X�y�[�X��^�u�������ꍇ���[�v���s
		const char d = raw_data[pos];
		if (!(d == ' ' || d == '\t')) { break; }
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_comment1
//=============================================================================
static int token_comment1(const char* raw_data, token_info& info)
{
	// "//"�̃R�����g�̏ꍇ
	if (strncmp(raw_data, "//", 2) != 0) { return 0; }

	// �����|�W�V����
	int pos = 2;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; raw_data[pos]; pos++)
	{
		// �s���������ꍇ�I��
		const char d = raw_data[pos];
		if (d == '\0' || d == '\n') { break; }
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_comment2
//=============================================================================
static int token_comment2(const char* raw_data, token_info& info)
{
	// "/*"�̃R�����g�̏ꍇ
	if (strncmp(raw_data, "/*", 2) != 0) { return 0; }

	// �����|�W�V����
	int pos = 2;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; raw_data[pos]; pos++)
	{
		// �R�����g�̏I���O�ɕ������o�Ă����ꍇ�I��
		if (raw_data[pos] == '\0')
		{
			return (int)flight_errors::error_endcomment;
		}

		// �I���R�����g
		if (strncmp(raw_data + pos, "*/", 2) == 0) { pos += 2; break; }
	}

	// �߂�l��Ԃ�
	return pos;
}


//=============================================================================
// <static functions> :: token_operator
//=============================================================================
static int token_operator(const char* raw_data, token_info& info)
{
	// ���Z�q�̃`�F�b�N

	// �����|�W�V����
	int pos = 0;		// ���̕�������f�[�^���擾

	// �S�s��ݒ�
	for (; raw_data[pos]; pos++)
	{
		// �������L������Ȃ��ꍇ�͔�����
		if (strchr(_token_operator_list, raw_data[pos]) == nullptr)
		{
			break;
		}
	}

	// �߂�l��Ԃ�
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

	// �擪�ƍŌ�̃_�u���N�H�[�g������
	++token_str;
	token_str[strlen(token_str) - 1] = '\0';

	// �G�X�P�[�v�������{��
	std::string ret;
	while (*token_str)
	{
		// �G�X�P�[�v����
		int e = 0;
		for (; e < escape_size; e++)
		{
			// �G�X�P�[�v������ƃ}�b�`���Ă��Ȃ�
			if (strncmp(escapes[e][0], token_str, 2) != 0) { continue; }

			// �G�X�P�[�v������ϊ�
			ret += escapes[e][1];
			token_str += strlen(escapes[e][0]);
			break;
		}

		// ���ɃG�X�P�[�v�������s���Ă���ꍇ��continue
		if (e != escape_size) { continue; }

		// ������ɒǉ�
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
	// �����̔���
	const auto ret = token_modify_str(token_str, info);

	// �����̒�����1�����ȊO�Ȃ�G���[
	if (ret.length() != 1)
	{
		info.error = error_toolongchar;
		return std::string();
	}

	// ������𐔒l�ɕϊ����Ă���߂��B(�����͌v�Z�Ƃ��Ďg�p�\)
	return std::to_string(ret[0]);
}


//=============================================================================
// <static functions> :: token_modify_number
//=============================================================================
static std::string token_modify_number(char* token_str, token_info& info)
{
	// �i����ݒ肷��
	int number_type = 10;
	if (strncmp(token_str, "0x", 2) == 0) { number_type = 16; }
	else if (*token_str == '0') { number_type = 8; }

	// ����0x(16�i��)��0(8�i��)�Ȃǂ̐ړ�����Ȃ�
	token_str += number_type != 10 ? number_type / 8 : 0;

	// ������g�[�N���̒�����p���̂��̂��������ɕϊ�����
	for (int i = 0; i < (int)strlen(token_str); i++)
	{
		if (isalpha(token_str[i]))
		{
			token_str[i] = tolower(token_str[i]);
		}
	}

	// ����10�i���ȊO�ŏ����_������ꍇ�̓G���[
	if (number_type != 10 && strchr(token_str, '.'))
	{
		info.error = error_invalidnumber;
		return std::string();
	}

	// ���������_��2�ȏ゠��ꍇ���G���[
	if (std::count_if(token_str, token_str + strlen(token_str), [](const auto& a) { return a == '.'; }) >= 2)
	{
		info.error = error_invalidnumber;
		return std::string();
	}

	// �`�F�b�N������̕K�v�ȕ����܂�\0�ɂ���B
	char check_string[] = "0123456789abcdef";
	check_string[number_type] = '\0';
	for (int i = 0; i < (int)strlen(token_str); i++)
	{
		// �i���̕�����`�F�b�N(8�i���Ȃǂ�8�ȏ�̒l���g���Ă��邩�H���m�F)
		if (strchr(check_string, token_str[i]) == nullptr && token_str[i] != '.')
		{
			info.error = error_invalidnumber;
			return std::string();
		}
	}

	// �����_�����݂��Ȃ��ꍇ
	if (strchr(token_str, '.') == nullptr)
	{
		const int real_number = (int)strtol(token_str, nullptr, number_type);
		return std::to_string(real_number);
	}

	// �����_�����݂���ꍇ
	return std::to_string(atof(token_str));

}


//-----------------------------------------------------------------------------



//=============================================================================
// constant variables
//=============================================================================
static const TOKEN_DIVIDER _token_divider[] =
{
	{ flight_token_type::FTK_NOCODE					, token_endline				, token_nofix },		// �s�I��
	{ flight_token_type::FTK_NOCODE					, token_notcode				, token_nofix },		// �X�y�[�X��^�u�Ȃ�
	{ flight_token_type::FTK_NOCODE					, token_comment1			, token_nofix },		// �R�����g
	{ flight_token_type::FTK_NOCODE					, token_comment2			, token_nofix },		// �R�����g
	{ flight_token_type::FTK_INSTRUCTION_RESERVED   , token_reserved_instruction, token_nofix },		// �\���
	{ flight_token_type::FTK_VALUE_RESERVED			, token_reserved_value		, token_nofix },		// �\���
	{ flight_token_type::FTK_PSES					, token_parentheses			, token_nofix },		// (
	{ flight_token_type::FTK_ENDPSES				, token_endparentheses		, token_nofix },		// )
	{ flight_token_type::FTK_BLOCK					, token_block				, token_nofix },		// {
	{ flight_token_type::FTK_ENDBLOCK				, token_endblock			, token_nofix },		// }
	{ flight_token_type::FTK_ENDINSTRUCTION			, token_cend				, token_nofix },		// ;
	{ flight_token_type::FTK_SEPARATOR				, token_separator			, token_nofix },		// +-*/%=
	{ flight_token_type::FTK_OPERATION				, token_operator			, token_nofix },		// +-*/%=
	{ flight_token_type::FTK_VALUE_NUMBER			, token_value_char			, token_modify_char },	// ����
	{ flight_token_type::FTK_VALUE_STR				, token_value_str			, token_modify_str },	// ������
	{ flight_token_type::FTK_VALUE_NUMBER			, token_value_number		, token_modify_number },// ���l
	{ flight_token_type::FTK_VARIABLE				, token_variable			, token_nofix },		// �ϐ�
	{ flight_token_type::FTK_UNKNOWN				, token_unknown				, token_nofix },		// ��̃g�[�N��
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
		// �I��芇�ʂȂ�return�Ŗ߂�
		if (tokens[index].type == flight_token_type::FTK_ENDBLOCK ||
			tokens[index].type == flight_token_type::FTK_ENDPSES)
		{
			return true;
		}

		// �G���[
		bool error = false;

		// (�n�܂�Ȃ� )���ċN�����Ɏ����Ă���
		if (tokens[index].type == flight_token_type::FTK_PSES)
		{
			// �ċA�������玸�s���Ԃ��Ă����ꍇ��false��Ԃ�
			if (!flight_check_token_nest_recursive(tokens, info, ++index)) { return false; }

			// ���̊K�w�̎n�܂�ƈقȂ�I��芇�ʂ������ꍇ��false�ŕԂ�
			if (tokens[index].type != flight_token_type::FTK_ENDPSES) { error = true; }
		}
		else if (tokens[index].type == flight_token_type::FTK_BLOCK)
		{
			// �ċA�������玸�s���Ԃ��Ă����ꍇ��false��Ԃ�
			if (!flight_check_token_nest_recursive(tokens, info, ++index)) { return false; }

			// ���̊K�w�̎n�܂�ƈقȂ�I��芇�ʂ������ꍇ��false�ŕԂ�
			if (tokens[index].type != flight_token_type::FTK_ENDBLOCK) { error = true; }
		}

		// �Ή������I��芇�ʂ��Ȃ�
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

	// �Ō�܂ōċA�ŉ�ꂽ�ꍇ��true��Ԃ�
	return index == (int)tokens.size();
}


//=============================================================================
// <static functions> :: flight_check_function
//=============================================================================
static bool flight_check_function(std::vector<flight_token>& tokens, token_info& info)
{
	// ���݂̃u���b�N���x��
	int now_block_level = 0;

	// �ϐ������擾
	for (int now_index = 0; now_index < (int)tokens.size(); now_index++)
	{
		// token���w��
		const auto& token = tokens[now_index];

		// �֐��������ꍇ�Ńu���b�N���x����0�ȊO�Ȃ�G���[
		if (token.type == flight_token_type::FTK_INSTRUCTION_RESERVED && token.token_str == "func")
		{
			if (now_block_level != 0)
			{
				info.line_no = token.line_no;
				info.error = flight_errors::error_function_notop;
				return false;
			}
		}

		// ���݂̃u���b�N���x�����C��
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
	// �g�[�N��
	std::vector<flight_token> ret;

	// �R�[�h�𕶎���ɂ���B
	const std::string s = raw_code + "	    ";	// �]���ȕ������ǉ������f�[�^��ݒ�
	const char* raw_data = s.c_str();
	static const int max_token_size = 0x7FFF;

	// �t�@�C������ݒ�
	token_info info;
	memset(&info, 0, sizeof(token_info));

	// �S�s���[�v
	while (*raw_data)
	{
		// ���l�������ꍇ
		static const int divider_length = (int)(sizeof(_token_divider) / sizeof(_token_divider[0]));
		int divider_pos = 0;
		for (; divider_pos < divider_length; divider_pos++)
		{
			// ������ۑ�
			const auto origin_info = info;

			// divider�̎擾
			const auto& divider = _token_divider[divider_pos];

			// �g�[�N������
			const int length = divider.divider_func(raw_data, info);
			if (length == 0) { continue; }

			// �g�[�N���̒���
			if (length < 0)
			{
				setter(info.line_no, info.error, "");
				return std::vector<flight_token>();
			}

			// -------------------�����܂ł���ꍇ�̓g�[�N���̒�����1�ȏ�-------------------
			if (length >= max_token_size)
			{
				setter(origin_info.line_no, flight_errors::error_tootokenlength, "");
				return std::vector<flight_token>();
			}

			// �R�����g����Ȃ��ꍇ�̓g�[�N���ۑ�
			if (_token_divider[divider_pos].token_type != flight_token_type::FTK_NOCODE)
			{
				// ��������R�s�[
				char mem[max_token_size];
				memset(mem, 0, sizeof(mem));
				memcpy(mem, raw_data, length);

				// �g�[�N��
				flight_token token;
				token.type = divider.token_type;
				token.token_str = divider.modify_func(mem, info);
				token.line_no = info.line_no;

				// �G���[�����������ꍇ
				if (info.error != 0)
				{
					setter(info.line_no, info.error, "");
					return std::vector<flight_token>();
				}

				// �g�[�N���ǉ�
				ret.push_back(token);
			}

			// ��������Z
			raw_data += length;

			break;
		}

		// �Ō�܂Ń`�F�b�N���X���[�������ꍇ�A�������Ȃ�
		if (divider_pos == divider_length) { ++raw_data; }
	}

	// �g�[�N���̊K�w�ŃG���[���������Ă����ꍇ�A�G���[
	if (!flight_check_token_nest(ret, info) || !flight_check_function(ret, info))
	{
		setter(info.line_no, info.error, "");
		return std::vector<flight_token>();
	}

	return ret;
}
