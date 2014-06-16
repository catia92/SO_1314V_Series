#include "stdafx.h"
#include "BackupClient.h"


// TODO Obter refer�ncia para o servi�o (inst�ncia de BackupServer)
HBACKUPSERVICE OpenBackupService(TCHAR * serviceName){
	HBACKUPSERVICE pService;
	// Como a inst�ncia j� est� mapeada, basta usar OpenFileMapping
	pService = (HBACKUPSERVICE) OpenFileMapping(PAGE_READWRITE, TRUE, serviceName);
	return pService;
}

//TODO Adicionar um pedido de Backup de um ficheiro.
BOOL BackupFile(HBACKUPSERVICE service, TCHAR * file){
	return FALSE;
}


//TODO Adicionar um pedido de reposi��o de um ficheiro.
BOOL RestoreFile(HBACKUPSERVICE service, TCHAR * file){
	return FALSE;
}


//TODO Pedido de termina��o do servi�o.
BOOL StopBackupService(TCHAR * serviceName){
	HBACKUPSERVICE service;
	if ((service = (HBACKUPSERVICE)OpenFileMapping(PAGE_READWRITE, FALSE, serviceName)) != NULL){
		WaitForSingleObject(service->hServiceExclusion, INFINITE);
		ReleaseMutex(service->hServiceExclusion);
		UnmapViewOfFile(service);
		CloseHandle(service);
		return TRUE;
	}
	fprintf(stderr, "Unable to open memory mapping: ERROR %d\n", GetLastError());
	return FALSE;
}
