#include "stdafx.h"
#include "BackupClient.h"

// TODO Obter refer�ncia para o servi�o (inst�ncia de BackupServer)
HBACKUPSERVICE OpenBackupService(TCHAR * serviceName){

	return NULL;
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
	return FALSE;
}
