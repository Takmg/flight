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
	FTK_NONE = 0,				// ����
	FTK_INSTRUCTION_RESERVED,	// �\���
	FTK_SEPARATOR,				// ��؂�(,)
	FTK_OPERATION,				// ���Z�q
	FTK_VARIABLE,				// �ϐ�
	FTK_VALUE_RESERVED,			// �\�񂳂ꂽ�l
	FTK_VALUE_CHAR,				// ����
	FTK_VALUE_STR,				// ������
	FTK_VALUE_NUMBER,			// ���l
	FTK_PSES,					// (
	FTK_ENDPSES,				// )
	FTK_BLOCK,					// �u���b�N
	FTK_ENDBLOCK,				// �u���b�N�I��
	FTK_ENDINSTRUCTION,			// ���ߏI��(;)
	FTK_NOCODE,					// �R�����g��X�y�[�X
	FTK_LF,						// ���s�R�[�h
	FTK_UNKNOWN,				// ��̃g�[�N��

	// �ȉ��̓g�[�N�������Ŏg�p���Ȃ�
	FTK_FUNCTION,				// �֐�
	FTK_GOTOLABEL,				// ���x���փW�����v
	FTK_GOTOBACKLABEL,			// ���̃��x���փW�����v
	FTK_LABEL,					// �W�����v�����ꏊ
	FTK_ACCUMULATOR,			// �A�L�������[�^
} flight_token_type;


//=============================================================================
// <enum> :: __flight_vm_status
//=============================================================================
typedef enum __flight_vm_status
{
	VM_EXEC = 0,		// ���s
	VM_SUSPEND,			// ���f
	VM_ERROR,			// �G���[
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
	int					line_no;	// �g�[�N���̍s
	std::string			token_str;	// �g�[�N�����e
	flight_token_type	type;		// �g�[�N���^�C�v
};


//=============================================================================
// <struct> :: flight_token_node
//=============================================================================
struct flight_token_node
{
	fl_ptr left;			// �\��
	fl_ptr right;			// �\��
	flight_token token;		// �g�[�N��
};


//=============================================================================
// <struct> :: flight_instruction
//=============================================================================
struct flight_instruction
{
	int address;	// �A�h���X
	fl_ptr root;	// �\���؂̃��[�g
};


//=============================================================================
// <struct> :: flight_generate_data
//=============================================================================
struct flight_generate_data
{
	fi_container container;		// ���߃R���e�i
	int sigunature;				// if��for�̎��ʎq
	int error_line_no;			// ���������G���[�s
	flight_errors error;		// ���������G���[
	flight_token error_token;	// �G���[�g�[�N��
};


//=============================================================================
// <struct> :: flight_stack_variable
//=============================================================================
struct flight_stack_variable : public flight_variable
{
	int block_level;				// �u���b�N���x�� ... �ϐ��̃u���b�N���x��
	int stack_level;				// �X�^�b�N���x�� ... �ϐ��̃X�^�b�N���x���B
};


//=============================================================================
// <struct> :: flight_arg
//=============================================================================
struct flight_arg
{
	std::string variable_name;		// ������
};


//=============================================================================
// <struct> :: flight_function_data
//=============================================================================
struct flight_function_data
{
	std::string function_name;			// �֐���
	int start_address;					// �J�n�A�h���X
	std::vector<flight_arg> args;		// �������
};


//=============================================================================
// <struct> :: flight_vm_data
//=============================================================================
struct flight_vm_data
{
	int program_counter;			// ���݂̎��s�A�h���X
	int stack_pointer;				// �X�^�b�N�|�C���^�[

	int now_block_level;			// �u���b�N���x�� ... �傩�����̓x�Ƀu���b�N���x����������B
									//                    �傩�����I�����Ƀu���b�N���̕ϐ�������
	int now_stack_level;			// �X�^�b�N���x�� ... �֐��Ăяo���̓x�ɃX�^�b�N���x����������B
									//                    �֐�����߂�x�ɊY���X�^�b�N�̕ϐ����폜����B

	bool exit_flg;					// �I���t���O

	flight_variable accumulator;	// �A�L�������[�^
	flight_variable exit_var;		// �I�����ɕۑ����ꂽ�f�[�^(yield�����l)

	std::vector<flight_function_data> functions;		// �֐��ꗗ
	std::vector<flight_variable> global_variables;		// �O���[�o���ϐ�(�����Ȃ� �v���O�����I���܂Ŏc�葱����)
	std::vector<flight_stack_variable> stack_variables;	// ��ʓI�ȕϐ�(stack_pointer�ȉ��̕ϐ����g����ϐ�)

	std::shared_ptr<fi_container> instructions;
	std::map<std::string, std::shared_ptr<flight_function_holder>>* phf_holders;	// �z�X�g�֐��ꗗ

	// �G���[�֘A
	int now_line_no;			// ���݂̍s�ԍ�(�G���[�o�͗p)
	int error_line_no;			// ���������G���[�s
	flight_errors error;		// ���������G���[
	flight_token error_token;	// �G���[�g�[�N��

};