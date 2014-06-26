#include "BackupClient.h"

HBACKUPSERVICE OpenBackupService(TCHAR * serviceName){
	HBACKUPSERVICE backupService;
	LPTSTR pBuf;
	DWORD clientId;

	backupService = *((PHBACKUPSERVICE)malloc(sizeof(HBACKUPSERVICE)));
	//reqSession.reqService = (PHBACKUPSERVICE)malloc(sizeof(HBACKUPSERVICE));

	/*
	Os clientes, que n�o poder�o ser mais do que MAX_CLIENTS em simult�neo, encontram o espa�o de mem�ria
	partilhada pelo nome atribu�do. O espa�o de mem�ria partilhada cont�m um array de MAX_CLIENTS posi��es,
	em que cada entrada cont�m uma inst�ncia de REQ e os elementos necess�rios para a sincroniza��o entre o
	servidor e um cliente espec�fico. Cada entrada desse array � do tipo SLOT.
	*/

	backupService.hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		serviceName);

	if (backupService.hMapFile == NULL){
		printf("1. Could not create file mapping object (%d).\n"), GetLastError();
		return;
	}

	pBuf = (LPTSTR)MapViewOfFile(backupService.hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(backupService));

	if (pBuf == NULL)
	{
		printf("2. Could not create file mapping object (%d).\n"), GetLastError();
		CloseHandle(backupService.hMapFile);
		return;
	}

	//backupService->reqService = *(PHBACKUPSERVICE)pBuf;

	return backupService;
}

BOOL BackupFile(HBACKUPSERVICE service, TCHAR * file){
	return FALSE;
}

BOOL RestoreFile(HBACKUPSERVICE service, TCHAR * file){
	return FALSE;
}

BOOL StopBackupService(TCHAR * serviceName){
	return FALSE;
}

BOOL CloseBackupService(HBACKUPSERVICE service){
	return FALSE;
}