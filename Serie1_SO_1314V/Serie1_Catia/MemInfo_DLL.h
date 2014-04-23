/*#ifndef MEMINFO_DLL_H
#define MEMINFO_DLL_H

#ifdef MEMINFO_FUNCS_DLL_EXPORTS
#define MEMINFO_DLL_API __declspec(dllexport);
#else
#define MEMINFO_DLL_API __declspec(dllimport);
#endif

#endif */

namespace MemInfoFuncs{

	typedef struct SysMemInfo {
		int thing;
	} SYSMEMINFO, *PSYSMEMINFO;

	typedef struct ProcMemInfo {
		int thing;
	} PROCMEMINFO, *PPROCMEMINFO;


	class MyMemInfoFuncs
	{
	public:
		static __declspec(dllexport) BOOL GetSystemMemInfo(_Out_  PSYSMEMINFO  pSysMemInfo);  //  Retorna informa��o da utiliza��o global de mem�ria do sistema.

		static __declspec(dllexport) VOID PrintProcVirtualAddress(_In_ DWORD dwProcId); // Apresenta na consola a lista de VAD�s do processo.

		static __declspec(dllexport) BOOL GetProcMemInfo(_In_  DWORD  dwProcId, _Out_  PPROCMEMINFO pProcMemInfo); // Retorna informa��o da utiliza��o de mem�ria do processo.
	};
}