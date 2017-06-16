#include <psc/process.h>

#include <psc/typedefs.h>
#if defined( _WIN32 )
	#include <windows.h>
#elif defined( _LINUX )
	#include <unistd.h>
	#include <errno.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/time.h>
	#include <sys/resource.h>
	#include <sys/wait.h>
	#include <sys/poll.h>
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

namespace psc {

#if defined (_WIN32)

static HANDLE CreateChildProcess (HANDLE hChildStdinRd, HANDLE hChildStdoutWr, HANDLE hChildStderrWr, char const * cmdline)
{ 
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;
	BOOL bFuncRetn = FALSE; 

	ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO); 
	siStartInfo.hStdError = hChildStderrWr;
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bFuncRetn = CreateProcess(NULL, 
		(char *)cmdline,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo    // receives PROCESS_INFORMATION 
	);

	if (bFuncRetn == 0) 
	{
		std::cout << "CreateProcess failed" << std::endl;
		return INVALID_HANDLE_VALUE;
	}
	else 
	{
		CloseHandle(piProcInfo.hThread);
		return piProcInfo.hProcess;
	}
}

int create_process (std::string const & cmd, std::string const & in, std::string & out)
{
	SECURITY_ATTRIBUTES saAttr; 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

	HANDLE hChildStdinRd, hChildStdinWr;
	CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0);
	SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

	HANDLE hChildStdoutRd, hChildStdoutWr;
	CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0);
	SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

	HANDLE hProcess = CreateChildProcess(hChildStdinRd, hChildStdoutWr, hChildStdoutWr, cmd.c_str());

	CloseHandle(hChildStdoutWr);
	CloseHandle(hChildStdinRd);

	DWORD written = 0;
	WriteFile(hChildStdinWr, in.c_str(), DWORD(in.length()), &written, 0);
	CloseHandle(hChildStdinWr);

	int ret = -1;
	if ( INVALID_HANDLE_VALUE != hProcess ) {
		out += '\n';
		DWORD nExitCode;
		DWORD readed;
		char buffer[4096];
		for ( ; ; ) {
			BOOL r = ReadFile(hChildStdoutRd, buffer, 4096, &readed, 0);
			if ( r ) {
				out.append(buffer, buffer + readed);
			} else {
				DWORD dwError = GetLastError();
				break;
			}
		}

		CloseHandle(hChildStdoutRd);
		WaitForSingleObject(hProcess, INFINITE);

		if ( GetExitCodeProcess(hProcess, &nExitCode) ) {
			ret = nExitCode;
			CloseHandle(hProcess);
		} else {
			std::cerr << "error while GetExitCode" << std::endl;
		}
	}
	return ret;
}

#elif defined( _LINUX )

int create_process (std::string const & cmd, std::string const & in, std::string & out)
{
//	printf("create_process_and_read_output, pid=%d, ppid=%d \n", getpid(), getppid());

    int outfd[2] = {-1};
    int rr = pipe(outfd);

	int infd[2] = {-1};
    rr = pipe(infd);

	//int exit_code = -1;
	int ret = -1;
    int pid = fork();

//	printf("fork, pid = %d, ppid = %d\n", pid, getppid());

	if ( pid != 0 ) {
		out += '\n';
		close(outfd[1]);
		close(infd[0]);
		int rcw1 = write(infd[1], in.c_str(), in.length());
		static_cast<void>(rcw1);
		close(infd[1]);

		char buffer[1024];
		int len;
		bool cycle = true;
		for ( ; cycle ; ) {
			len = read(outfd[0], buffer, 1024);
			if ( len > 0 ) {
				out.append(buffer, buffer + len);
			} else {
				cycle = false;
			}
		}
	
		int options;
		int p = waitpid(pid, &options, 0);
		if ( p == pid && WIFEXITED(options) ) {
			ret = WEXITSTATUS(options);
		} else {
			ret = -1;
		}

		close(outfd[0]);
	} else {
		// child process

		close(outfd[0]);
		close(infd[1]);
		
        int rc = dup2(outfd[1], 1);
        rc = dup2(outfd[1], 2);
        rc = dup2(infd[0], 0);
		if ( -1 == rc ) {
			std::cerr << "error while dup2, pid=" << getpid() << std::endl;
		}
		ret = system(cmd.c_str());

		if ( 0 != ret ) {
			ret = 2;
		}
		_exit(ret);
	}

	return ret;
}

#endif  // _LINUX

}
