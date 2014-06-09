#include <Windows.h>
#include "BackupServer.h"

/*
Outros processos (clientes) obt�m refer�ncias para o servi�o (OpenBackupService), podendo
adicionar pedidos de backup (BackupFile), de reposi��o (RestoreFile) e de termina��o (StopBackupService).
*/

HBACKUPSERVICE OpenBackupService(TCHAR * serviceName);

BOOL BackupFile(HBACKUPSERVICE service, TCHAR * file);

BOOL RestoreFile(HBACKUPSERVICE service, TCHAR * file);

BOOL StopBackupService(TCHAR * serviceName);

BOOL CloseBackupService(HBACKUPSERVICE service);