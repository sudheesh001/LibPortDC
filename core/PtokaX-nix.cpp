/*
 * PtokaX - hub server for Direct Connect peer to peer network.

 * Copyright (C) 2002-2005  Ptaczek, Ptaczek at PtokaX dot org
 * Copyright (C) 2004-2012  Petr Kozelka, PPK at PtokaX dot org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#include "stdinc.h"
//---------------------------------------------------------------------------
#include "eventqueue.h"
#include "LanguageManager.h"
#include "ServerManager.h"
#include "serviceLoop.h"
#include "SettingManager.h"
#include "utility.h"
//---------------------------------------------------------------------------
#include "regtmrinc.h"
#include "scrtmrinc.h"
//---------------------------------------------------------------------------
#ifdef TIXML_USE_STL
	#undef TIXML_USE_STL
#endif
//---------------------------------------------------------------------------

static void SigHandler(int sig) {
	string str = "Received signal ";
	
	if(sig == SIGINT) {
	    str += "SIGINT";
	} else if(sig == SIGTERM) {
	    str += "SIGTERM";
	} else if(sig == SIGQUIT) {
	    str += "SIGQUIT";
	} else if(sig == SIGHUP) {
	    str += "SIGHUP";
	} else {
	    str += string(sig);
	}
	
	str += " ending...";
	
    eventqueue->AddThread(eventq::EVENT_SHUTDOWN, str.c_str(), NULL);

	// restore to default...
	struct sigaction sigact;
	sigact.sa_handler = SIG_DFL;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	    
	sigaction(sig, &sigact, NULL);
}
//---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
	sTitle = "e-Library DC Hub " + string(PtokaXVersionString);
	
#ifdef _DEBUG
	sTitle += " [debug]";
#endif
	
	for(int i = 0; i < argc; i++) {
	    if(strcasecmp(argv[i], "-d") == 0) {
	    	bDaemon = true;
	    } else if(strcasecmp(argv[i], "-c") == 0) {
	    	if(++i == argc) {
	            printf("Missing config directory!\n");
	            return EXIT_FAILURE;
	    	}
	
			if(argv[i][0] != '/') {
	            printf("Config directory must be absolute path!\n");
	            return EXIT_FAILURE;
			}
	
	        size_t szLen = strlen(argv[i]);
			if(argv[i][szLen - 1] == '/') {
	            PATH = string(argv[i], szLen - 1);
			} else {
	            PATH = string(argv[i], szLen);
	        }
	
	        if(DirExist(PATH.c_str()) == false) {
	        	if(mkdir(PATH.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
	                if(bDaemon == true) {
	                    syslog(LOG_USER | LOG_ERR, "Config directory not exist and can't be created!\n");
	                } else {
	                    printf("Config directory not exist and can't be created!");
	                }
	            }
            }
	    } else if(strcasecmp(argv[i], "-v") == 0) {
	        printf("%s built on %s %s\n", sTitle.c_str(), __DATE__, __TIME__);
	        return EXIT_SUCCESS;
	    } else if(strcasecmp(argv[i], "-h") == 0) {
	        printf("PtokaX [-d] [-c <configdir>] [-v]\n");
	        return EXIT_SUCCESS;
	    } else if(strcasecmp(argv[i], "/nokeycheck") == 0) {
	        bCmdNoKeyCheck = true;
	    } else if(strcasecmp(argv[i], "/generatexmllanguage") == 0) {
	        LangMan::GenerateXmlExample();
	        return EXIT_SUCCESS;
	    }
	}
	
	if(PATH.size() == 0) {
	    char* home;
	    char curdir[PATH_MAX];
	    if(bDaemon == true && (home = getenv("HOME")) != NULL) {
	        PATH = string(home) + "/.PtokaX";
	            
	        if(DirExist(PATH.c_str()) == false) {
	            if(mkdir(PATH.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
	                syslog(LOG_USER | LOG_ERR, "Config directory not exist and can't be created!\n");
	            }
	        }
	    } else if(getcwd(curdir, PATH_MAX) != NULL) {
	        PATH = curdir;
	    } else {
	        PATH = ".";
	    }
	}
	
	if(bDaemon == true) {
	    printf("Starting %s as daemon using %s as config directory.\n", sTitle.c_str(), PATH.c_str());
	
	    pid_t pid1 = fork();
	    if(pid1 == -1) {
	        syslog(LOG_USER | LOG_ERR, "First fork failed!\n");
	        return EXIT_FAILURE;
	    } else if(pid1 > 0) {
	        return EXIT_SUCCESS;
	    }
	
	    if(setsid() == -1) {
	        syslog(LOG_USER | LOG_ERR, "Setsid failed!\n");
	        return EXIT_FAILURE;
	    }
	
	    pid_t pid2 = fork();
	    if(pid2 == -1) {
	        syslog(LOG_USER | LOG_ERR, "Second fork failed!\n");
	        return EXIT_FAILURE;
	    } else if(pid2 > 0) {
            return EXIT_SUCCESS;
	    }
	
	    chdir("/");
	
	    close(STDIN_FILENO);
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);
	
	    umask(117);
	
	    if(open("/dev/null", O_RDWR) == -1) {
	        syslog(LOG_USER | LOG_ERR, "Failed to open /dev/null!\n");
	        return EXIT_FAILURE;
	    }
	
	    dup(0);
	    dup(0);
	}
	
	sigset_t sst;
	sigemptyset(&sst);
	sigaddset(&sst, SIGPIPE);
	sigaddset(&sst, SIGURG);
	sigaddset(&sst, SIGALRM);
	sigaddset(&sst, SIGSCRTMR);
	sigaddset(&sst, SIGREGTMR);
	
	if(bDaemon == true) {
	    sigaddset(&sst, SIGHUP);
	}
	
	pthread_sigmask(SIG_BLOCK, &sst, NULL);
	
	struct sigaction sigact;
	sigact.sa_handler = SigHandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	
	if(sigaction(SIGINT, &sigact, NULL) == -1) {
	    AppendDebugLog("%s - [ERR] Cannot create sigaction SIGINT in main\n", 0);
	    exit(EXIT_FAILURE);
	}
	
	if(sigaction(SIGTERM, &sigact, NULL) == -1) {
	    AppendDebugLog("%s - [ERR] Cannot create sigaction SIGTERM in main\n", 0);
	    exit(EXIT_FAILURE);
	}
	
	if(sigaction(SIGQUIT, &sigact, NULL) == -1) {
	    AppendDebugLog("%s - [ERR] Cannot create sigaction SIGQUIT in main\n", 0);
	    exit(EXIT_FAILURE);
	}
	
	if(bDaemon == false && sigaction(SIGHUP, &sigact, NULL) == -1) {
	    AppendDebugLog("%s - [ERR] Cannot create sigaction SIGHUP in main\n", 0);
	    exit(EXIT_FAILURE);
	}

	ServerInitialize();

	if(ServerStart() == false) {
	    if(bDaemon == false) {
	        printf("Server start failed!\n");
	    } else {
	        syslog(LOG_USER | LOG_ERR, "Server start failed!\n");
	    }
	    return EXIT_FAILURE;
	} else if(bDaemon == false) {
	    printf("%s running...\n", sTitle.c_str());
	}

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 100000000;

	while(true) {
	    srvLoop->Looper();
	
	    if(bServerTerminated == true) {
	        break;
	    }
	
        nanosleep(&sleeptime, NULL);
	}

	if(bDaemon == false) {
	    printf("%s ending...\n", sTitle.c_str());
	}

    return EXIT_SUCCESS;
}
//---------------------------------------------------------------------------
