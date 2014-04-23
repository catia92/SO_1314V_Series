#include "stdafx.h"
#include <stdio.h>

#include "MemInfo_DLL.h"

using namespace std;

namespace MemInfoFuncs{

	BOOL MyMemInfoFuncs::GetSystemMemInfo(_Out_  PSYSMEMINFO  pSysMemInfo){  //  Retorna informa��o da utiliza��o global de mem�ria do sistema.
		printf("Get System Mem Info");
		return true;
	}

	VOID MyMemInfoFuncs::PrintProcVirtualAddress(_In_ DWORD dwProcId){ // Apresenta na consola a lista de VAD�s do processo.
		printf("Print Proc Virtual Address");

	}

	BOOL MyMemInfoFuncs::GetProcMemInfo(_In_  DWORD  dwProcId, _Out_  PPROCMEMINFO pProcMemInfo){ // Retorna informa��o da utiliza��o de mem�ria do processo.
		printf("Get Proc Mem Info");
		return true;
	}
}