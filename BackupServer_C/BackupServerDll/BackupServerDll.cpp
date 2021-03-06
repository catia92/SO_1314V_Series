#include "stdafx.h"
#include <iostream>
#include <tchar.h>
#include "BackupServerDll.h"
#include <cstdio>
#include <wchar.h>

HBACKUPSERVICE backupService;

// Instanciar o servi�o
HBACKUPSERVICE CreateBackupService(TCHAR * serviceName, TCHAR * repoPath){
	SIZE_T maxSize = sizeof(BACKUPSERVICE);
	DWORD i = 0;
	HANDLE hfMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, maxSize, serviceName);

	if (hfMap == NULL){
		printf("\nERRO: Nao foi possivel chamar CreateFileMapping! #%d", GetLastError());
		return NULL;
	}

	backupService = (HBACKUPSERVICE)MapViewOfFile(hfMap, FILE_MAP_ALL_ACCESS, 0, 0, maxSize);
	if (backupService == NULL){
		printf("\nERRO: Nao foi possivel mapear em memoria! #%d",GetLastError());
		return NULL;
	}
	
	wcsncpy_s(backupService->fileStoragePath, repoPath, wcslen(repoPath)+1);
	backupService->nRequests = 0;
	wcsncpy_s(backupService->serviceName, serviceName, wcslen(serviceName)+1);
	backupService->hServiceExclusion = CreateMutex(NULL, FALSE, (LPCWSTR)"serverMutex"); // null para usar os security attributes por omiss�o
	backupService->serverProcess = GetCurrentProcess();

	if (backupService->hServiceExclusion == NULL){
		printf("\nERRO: Nao foi possivel criar o mutex! #%d", GetLastError());
		return NULL;
	}
	return backupService;
}

// Manda realizar a operacao escolhida, sobre o ficheiro
BOOL ProcessNextEntry(HBACKUPSERVICE service, ProcessorFunc processor){
	HBACKUPENTRY pRequest;
	HANDLE hMutexDup;
	DWORD nRequests, reqIdx;
	BOOL requestSuccess = FALSE; 

	// Must duplicate the Mutex handle here
	if (DuplicateHandle(service->serverProcess,
		service->hServiceExclusion, GetCurrentProcess(), &hMutexDup, 0, FALSE, DUPLICATE_SAME_ACCESS) == NULL){
		printf("\nERRO: Nao foi possivel duplicar o handle para o mutex! (%d)", GetLastError());
		return FALSE;
	}

	WaitForSingleObject(hMutexDup, INFINITE);
	{
		nRequests = service->nRequests;
		// Ir buscar um pedido, para mandar processar
		if (nRequests > 0){
			reqIdx = nRequests-1;	// Ir buscar a entrada correspondente ao ultimo pedido
			pRequest = &service->requests[reqIdx];
			requestSuccess = processor(pRequest);
			// Se houve sucesso no processamento do pedido, decrementar n� de pedidos
			if (requestSuccess){
				service->nRequests --;
				printf("\nSucesso: Pedido processado!");
				SetEvent(pRequest->success);
			}
			else{
				printf("\nERRO: O pedido nao foi processado correctamente!");
				SetEvent(pRequest->unsuccess);
			}
		}
	}
	ReleaseMutex(hMutexDup);
	CloseHandle(hMutexDup);
	return requestSuccess;
}

// Espera o resultado de um pedido
BOOL WaitForOperationResult(HBACKUPENTRY pEntry){
	BOOL finished = FALSE, success = FALSE;
	do{
		if (WaitForSingleObject(pEntry->unsuccess, 1000) != WAIT_TIMEOUT){
			finished = TRUE;
			success = FALSE;
			ResetEvent(pEntry->unsuccess);
			printf("\n\nA OPERACAO FALHOU!\n");
		}
		else if (WaitForSingleObject(pEntry->success, 1000) != WAIT_TIMEOUT){
			finished = TRUE;
			success = TRUE;
			ResetEvent(pEntry->success);
			printf("\n\nA operacao foi concluida com sucesso.\n");
		}
	} while (!finished);
	return success;
}

// Faz a reposicao (Restore) ou guarda uma copia do ficheiro (Backup), conforme o bool recebido.
// backupNrestore = true -> Backup
// backupNrestore = false -> Restore
BOOL CopyFile(BOOL backupNrestore, HBACKUPENTRY copyRequest){
	
	FILE * origin; FILE * destiny;
	CHAR auxBuffer[MAX_PATH];
	DWORD i, ofnIdx; //ofnIdx serve para copiar todos os chars para um char* local, com o nome do ficheiro original
	CHAR* separator = "\\", *terminator = "\0";
	SIZE_T sz, filePathSize = strlen((CHAR*)backupService->fileStoragePath) + strlen(separator) + strlen((CHAR*)copyRequest->file) + 1;
	CHAR*repoFileName, *originalFileName;
	
	repoFileName = (CHAR*)malloc(filePathSize);
	originalFileName = (CHAR*)malloc(strlen((CHAR*)copyRequest->file) + 1);

	sz = strlen((CHAR*)backupService->fileStoragePath) + 1;
	strcpy_s(repoFileName, sz, (CHAR*)backupService->fileStoragePath);

	sz += strlen(separator);
	strcat_s(repoFileName, filePathSize, separator);

	ofnIdx = 0;
	for (i = 0; i < STR_MAX_SIZE; i++){
		CHAR * part = (CHAR*)(copyRequest->file + i*sizeof(CHAR));
		if (strcmp(part, terminator) == 0)
			break;
		sz++; ofnIdx++;
		strcat_s(repoFileName, sz, part);

		if (ofnIdx == 1)
			strcpy_s(originalFileName, ofnIdx + 1, part);
		else
			strcat_s(originalFileName, ofnIdx + 1, part);
	}

	if (fopen_s(&origin, backupNrestore?originalFileName : repoFileName, "rb") != 0){// abrir ficheiro origem para ler
		printf("\nERRO... O ficheiro %s nao existe!", backupNrestore ? originalFileName : repoFileName);
		return FALSE;
	}

	if (fopen_s(&destiny, backupNrestore ? repoFileName : originalFileName, "wb+") != 0){
		printf("\nERRO... Nao foi possivel criar o ficheiro %s ! Verifique se o caminho existe!", backupNrestore ? repoFileName : originalFileName);
		return FALSE;
	}

	while (fgets(auxBuffer, sizeof(auxBuffer), origin) != NULL){
		fprintf_s(destiny, auxBuffer);
	}

	fclose(origin);
	fclose(destiny);
	return TRUE;
}

// Repoe o ficheiro original (copia o ficheiro novamente, mas na ordem inversa)
BOOL RestoreFileFunction(HBACKUPENTRY pentry){
	return CopyFile(FALSE, pentry);
}

//Copia ficheiro
BOOL BackupFileFunction(HBACKUPENTRY pentry){
	return CopyFile(TRUE, pentry);
}

BOOL SendNewRequest(HBACKUPSERVICE service, DWORD clientProcId, BACKUP_OPERATION operation, TCHAR * file){
	DWORD nReq;
	HBACKUPENTRY pEntry;
	HANDLE hMutexDup;
	BOOL success = FALSE;

	if (operation != exit_operation && file==NULL){
		printf("\nERROR: No file was specified!");
		return FALSE;
	}

	// Must duplicate the Mutex handle here
	if (DuplicateHandle(service->serverProcess,
		service->hServiceExclusion, GetCurrentProcess(), &hMutexDup, 0, FALSE, DUPLICATE_SAME_ACCESS) == NULL){
			printf("\nERROR: Could not duplicate the server process' mutex handle! (%d)", GetLastError());
			return FALSE;
		}
	
	WaitForSingleObject(hMutexDup, INFINITE);
	nReq = service->nRequests;
	if (nReq >= MAX_REQUESTS_NR){ // se o n� m�ximo de pedidos foi alcan�ado, sair
		ReleaseMutex(hMutexDup);
		return FALSE;
	}
	service->requests[nReq].clientProcessId = clientProcId;
	service->requests[nReq].operation = operation;
	wcsncpy_s(service->requests[nReq].file, file, wcslen(file) + 1);
	service->nRequests++; 

	// Criar eventos manuais, que vao ser sinalizados caso a operacao tenha sucesso ou nao, respectivamente
	service->requests[nReq].success = CreateEvent(NULL, TRUE, FALSE, L"successEvent");
	service->requests[nReq].unsuccess = CreateEvent(NULL, TRUE, FALSE, L"unsuccessEvent");

	if (service->requests[nReq].success == NULL || service->requests[nReq].unsuccess == NULL){
		printf("\nERRO ao tentar criar os eventos! (%d)",GetLastError());
	}

	ReleaseMutex(hMutexDup);
	
	switch (operation)
	{
	case backup_operation:
		ProcessNextEntry(service, BackupFileFunction); break;
	case restore_operation:
		ProcessNextEntry(service, RestoreFileFunction); break;
	default: break;
	}

	CloseHandle(hMutexDup);
	return WaitForOperationResult(&service->requests[nReq]);
}

BOOL CloseBackupService(HBACKUPSERVICE service){
	
	DWORD i;

	WaitForSingleObject(service->hServiceExclusion, INFINITE);

	// Notificar todos os pedidos pendentes do insucesso
	for (i = service->nRequests; i>0; i--){
		SetEvent(service->requests[i].unsuccess);
	}
	
	ReleaseMutex(service->hServiceExclusion);

	if (!UnmapViewOfFile((LPVOID)service)){
		printf("\nUnmapViewOfFile failed = ERROR %d", GetLastError());
		return FALSE;
	}
	return TRUE;
}

//Cliente

// Obter refer�ncia para o servi�o (inst�ncia de BackupServer)
HBACKUPSERVICE OpenBackupService(TCHAR * serviceName){
	HBACKUPSERVICE pService;
	HANDLE hMapFile;

	hMapFile = (HBACKUPSERVICE)OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, serviceName);
	if (hMapFile == NULL)
	{
		printf("Could not open file mapping object (%d).\n",GetLastError());
		return NULL;
	}
	
	pService = (HBACKUPSERVICE) MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BACKUPSERVICE));
	if (pService == NULL)
	{
		printf("Could not map view of file (%d).\n", GetLastError());
		return NULL;
	}

	return pService;
}

// Enviar um pedido de Backup de um ficheiro.
BOOL BackupFile(HBACKUPSERVICE service, TCHAR * file){
	return SendNewRequest(service, GetCurrentProcessId(), backup_operation, file);
}

// Enviar um pedido de reposi��o de um ficheiro.
BOOL RestoreFile(HBACKUPSERVICE service, TCHAR * file){
	return SendNewRequest(service, GetCurrentProcessId(), restore_operation, file);
}

// Enviar pedido de termina��o do servi�o.
BOOL StopBackupService(TCHAR * serviceName){
	
	BOOL success = FALSE, finished = FALSE;
	HANDLE hService = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, serviceName);
	HBACKUPSERVICE pService = (HBACKUPSERVICE)MapViewOfFile(hService, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BACKUPSERVICE));

	if (pService == NULL){
		printf("\nERRO: Nao foi possivel chamar a funcao MapViewOfFile com sucesso! (%d)",GetLastError());
		return success = FALSE;
	}

	success = CloseBackupService(pService);// SendNewRequest(pService, GetCurrentProcessId(), exit_operation, NULL);
	
	UnmapViewOfFile(pService);
	CloseHandle(hService);

	return success;
}
