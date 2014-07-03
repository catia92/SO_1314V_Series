#include <string.h>
#include "BackupService.h"

/****************************** Server *********************************/
HBACKUPSERVICE CreateBackupService(TCHAR * serviceName, TCHAR * repoPath){
	HBACKUPSERVICE backupService;
	HANDLE hMapFile;
	LPCTSTR pBuf;

	/* Um processo (servidor) instancia o servi�o (CreateBackupService), ficando respons�vel
	por realizar as c�pias dos ficheiros � medida que os pedidos v�o aparecendo (ProcessNextEntry) */

	hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(BACKUPSERVICE),
		serviceName
	);

	if (hMapFile == NULL){
		printf("Could not create file mapping object (%d).\n", GetLastError());
		return NULL;
	}
	pBuf = (LPCTSTR)MapViewOfFile(
		hMapFile,				//_In_  HANDLE	hFileMappingObject,
		FILE_MAP_ALL_ACCESS,	//_In_  DWORD	dwDesiredAccess,
		0,						//_In_  DWORD	dwFileOffsetHigh,
		0,						//_In_  DWORD	dwFileOffsetLow,
		sizeof(BACKUPSERVICE)	//_In_  SIZE_T	dwNumberOfBytesToMap
	);

	if (pBuf == NULL){
		printf("Could not create file mapping object (%d).\n", GetLastError());
		CloseHandle(hMapFile);
		return NULL;
	}

	backupService = (HBACKUPSERVICE)pBuf;

	backupService->hMapFile = hMapFile;
	backupService->repoPath = repoPath;
	backupService->nRequests = 0;
	backupService->putRequest = 0;
	backupService->getRequest = 0;
	backupService->isAlive = TRUE;
	backupService->processID = GetCurrentProcessId();

	backupService->rqstsMutex = CreateMutex(NULL, FALSE, NULL);		// Mutex sem nome que trata de lidar com a lista de requestss
	backupService->hasWork = CreateEvent(NULL, FALSE, FALSE, NULL);	// Auto-Reset Event (FALSE), sem nome (NULL)
	backupService->isFull = CreateEvent(NULL, FALSE, FALSE, NULL);	// Auto-Reset Event (FALSE), sem nome (NULL)

	printf("\n++++ Server with Service: %s is online! ++++\n\n", serviceName);
	printf("\n++++ Repository Path: %s ++++\n\n", repoPath);

	return backupService;
}

BOOL ProcessNextEntry(HBACKUPSERVICE service, ProcessorFunc processor){
	BACKUPENTRY backupEntry;

	if (service->isAlive == FALSE){
		printf("\n++++ Server is no longer working! ++++\n\n ");
		return FALSE;
	}

	if (service->nRequests == 0){
		printf("\n++++ Server has nothing to do and it will sleep until there's work! ++++\n\n ");
		WaitForSingleObject(service->hasWork, INFINITE);
	}

	// If the server is online and has work to do, then it should process the next entry on requests
	printf("\n++++ Server is going to process the next entry! ++++\n\n ");
	WaitForSingleObject(service->rqstsMutex, INFINITE);
	{
		backupEntry = service->requests[service->getRequest];
		service->getRequest = service->getRequest == MAX_REQUESTS ? 0 : service->getRequest++;
	}
	ReleaseMutex(service->rqstsMutex);
	
	switch (backupEntry.operation){
		case Backup:
			if (!CopyFile(
				backupEntry.file,	// _In_  LPCTSTR lpExistingFileName -> If lpExistingFileName does not exist, CopyFile fails, and GetLastError returns ERROR_FILE_NOT_FOUND.
				service->repoPath,	// _In_  LPCTSTR lpNewFileName
				FALSE				// _In_  BOOL bFailIfExists -> If this parameter is TRUE and the new file specified by lpNewFileName already exists, the function fails. If this parameter is FALSE and the new file already exists, the function overwrites the existing file and succeeds.
				))
			{
				printf("Couldn't copy the file. Error %d", GetLastError());
				SetEvent(backupEntry.hArray[1]); // and return TRUE; ?
			}
			else{
				SetEvent(backupEntry.hArray[0]); // and return TRUE; ?
			}
			break;
		case Restore:
			if (!CopyFile(
				service->repoPath,	// _In_  LPCTSTR lpExistingFileName -> If lpExistingFileName does not exist, CopyFile fails, and GetLastError returns ERROR_FILE_NOT_FOUND.
				backupEntry.file,	// _In_  LPCTSTR lpNewFileName
				FALSE				// _In_  BOOL bFailIfExists -> If this parameter is TRUE and the new file specified by lpNewFileName already exists, the function fails. If this parameter is FALSE and the new file already exists, the function overwrites the existing file and succeeds.
				))
			{
				printf("Couldn't copy the file. Error %d", GetLastError());
				SetEvent(backupEntry.hArray[1]); // and return FALSE; ?
			}
			else{
				SetEvent(backupEntry.hArray[0]); // and return TRUE; ?
			}
			break;
		case Exit:
			// TODO
			break;
		default:
			printf("An error has occurred: %d", GetLastError());
			return FALSE;
	}

	ResetEvent(service->isFull); // If a request was replied, then the server has at least one slot available

	return TRUE;
}

BOOL CloseBackupService(HBACKUPSERVICE service){
	return FALSE;
}

/****************************** Client *********************************/
HBACKUPSERVICE OpenBackupService(TCHAR * serviceName){
	HBACKUPSERVICE backupService;
	HANDLE hMapFile;
	LPTSTR pBuf;

	/*
	Os clientes, que n�o poder�o ser mais do que MAX_CLIENTS em simult�neo, encontram o espa�o de mem�ria
	partilhada pelo nome atribu�do. O espa�o de mem�ria partilhada cont�m um array de MAX_CLIENTS posi��es,
	em que cada entrada cont�m uma inst�ncia de PBACKUPENTRY e os elementos necess�rios para a sincroniza��o 
	entre o servidor e um cliente espec�fico.
	*/

	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		serviceName);

	if (hMapFile == NULL){
		printf("Could not create file mapping object (%d).\n", GetLastError());
		return NULL;
	}

	pBuf = (LPTSTR)MapViewOfFile(hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(BACKUPSERVICE));

	if (pBuf == NULL)
	{
		printf("Could not create file mapping object (%d).\n", GetLastError());
		CloseHandle(hMapFile);
		return NULL;
	}

	backupService = (HBACKUPSERVICE)pBuf;

	backupService->hMapFile = hMapFile;

	printf("\n++++ Found the Server with service name: %s ++++\n", serviceName);

	return backupService;
}

BOOL FileOperation(HBACKUPSERVICE service, TCHAR * file, enum OPERATION operation){
	HANDLE rqstsMutexDup, hasWorkDup, isFullDup;
	PBACKUPENTRY backupEntry;
	DWORD wfmoRet;
	TCHAR cenas[200];

	if (service->isAlive == FALSE){
		printf("\n+++++ Server is offline, declining this request! +++++\n");
		return FALSE;
	}

	// Is it better to put WaitForSingleObject(rqstsMutexDup, INFINITE) here?

	if (!DuplicateHandle(
		OpenProcess(PROCESS_ALL_ACCESS, FALSE, service->processID),
		service->rqstsMutex,
		GetCurrentProcess(),
		&rqstsMutexDup,
		0,		// This parameter is ignored if the dwOptions parameter specifies the DUPLICATE_SAME_ACCESS flag.
		FALSE,
		DUPLICATE_SAME_ACCESS))
	{
		printf("\n1. Could not duplicate the request Mutex Handle! (%d).\n"), GetLastError();
		return FALSE;
	}

	if (!DuplicateHandle(
		OpenProcess(PROCESS_ALL_ACCESS, FALSE, service->processID),
		service->hasWork,
		GetCurrentProcess(),
		&hasWorkDup,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS))
	{
		printf("\n2. Could not duplicate the hasWork Mutex Handle! (%d).\n"), GetLastError();
		return FALSE;
	}

	// I need to do this here because of error handling
	if (!DuplicateHandle(
		OpenProcess(PROCESS_ALL_ACCESS, FALSE, service->processID),
		service->isFull,
		GetCurrentProcess(),
		&isFullDup,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS))
	{
		printf("\n3. Could not duplicate the isFull Mutex Handle (%d).\n"), GetLastError();
		return FALSE;
	}

	if (service->nRequests == MAX_CLIENTS){
		printf("\n+++++ Too many clients, sorry :/ Wait a bit... +++++\n");
		WaitForSingleObject(service->isFull, INFINITE);
	}

	WaitForSingleObject(rqstsMutexDup, INFINITE);
	{
		backupEntry = &service->requests[service->putRequest];
		service->putRequest = service->putRequest == MAX_REQUESTS ? 0 : service->putRequest++;

		backupEntry->clientProcID = GetCurrentProcessId();
		backupEntry->operation = operation;
		strcpy_s(backupEntry->file, strlen(file) + 1, file);

		backupEntry->hArray[0] = CreateEvent(NULL, FALSE, FALSE, "success");	// Auto-Reset Event (FALSE), com nome ("success")
		backupEntry->hArray[1] = CreateEvent(NULL, FALSE, FALSE, "failure");	// Auto-Reset Event (FALSE), com nome ("failure")

		if (service->putRequest == MAX_CLIENTS)
			SetEvent(isFullDup);

		SetEvent(hasWorkDup);
	}
	ReleaseMutex(rqstsMutexDup);

	wfmoRet = WaitForMultipleObjects(
		2,						// number of objects in array
		backupEntry->hArray,	// array of objects
		FALSE,					// wait for any object
		INFINITE				// Infinite wait
	);

	switch (wfmoRet)
	{
		// hArray[0] was signaled
		case WAIT_OBJECT_0 + 0:
			// TODO: Perform tasks required by this event
			printf("\nOperation returned success.\n");
			break;

		// hArray[1] was signaled
		case WAIT_OBJECT_0 + 1:
			printf("\nOperation returned failure.\n");
			break;

		case WAIT_TIMEOUT:
			printf("\nWait timed out.\n");
			return FALSE;

		// Return value is invalid.
		default:
		printf("\nWait error: %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL BackupFile(HBACKUPSERVICE service, TCHAR * file){
	return FileOperation(service, file, Backup);
}

BOOL RestoreFile(HBACKUPSERVICE service, TCHAR * file){
	return FileOperation(service, file, Restore);
}

BOOL StopBackupService(TCHAR * serviceName){
	return FALSE;
}