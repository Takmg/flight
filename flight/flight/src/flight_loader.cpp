//=============================================================================
// include files
//=============================================================================
#include "stdafx.h"
#include "../../stdafx.h"


//=============================================================================
// <global functions> :: flight_loadfile
//=============================================================================
std::string flight_loadfile(const char* filename, const error_setter& setter)
{
	// �t�@�C���̓ǂݍ���
	std::ifstream ifs(filename);
	if (ifs.fail()) { setter(-1, flight_errors::error_notfoundfile, ""); return std::string(); }

	// �t�@�C������S�s�擾
	std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// �ǂݍ���
	return str;
}


//=============================================================================
// <global functions> :: flight_loadmemory
//=============================================================================
std::string flight_loadmemory(const char* memory, const error_setter& setter)
{
	if (memory == nullptr)
	{
		setter(-1, flight_errors::error_nullmemory, "");
		return std::string();
	}

	return std::string(memory);
}