
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>
#include "util.h"

#define LIMIT 2560
#define MAXLINE 1024

char lineBuffer[MAXLINE];
CP *childs = NULL;

void init(){
        GBSH_PID = getpid();
        GBSH_IS_INTERACTIVE = isatty(STDIN_FILENO);  

		if (GBSH_IS_INTERACTIVE) {

			while (tcgetpgrp(STDIN_FILENO) != (GBSH_PGID = getpgrp()))
					kill(GBSH_PID, SIGTTIN);             
	              
			act_child.sa_sigaction = *signalHandler_child;
			act_child.sa_flags = SA_SIGINFO;
			
			sigaction(SIGCHLD, &act_child, 0);

			setpgid(GBSH_PID, GBSH_PID);
			GBSH_PGID = getpgrp();
			if (GBSH_PID != GBSH_PGID) {
					printf("Error, the shell is not process group leader");
					exit(EXIT_FAILURE);
			}
			tcsetpgrp(STDIN_FILENO, GBSH_PGID);  

			tcgetattr(STDIN_FILENO, &GBSH_TMODES);

			currentDirectory = (char*) calloc(1024, sizeof(char));
        } else {
                printf("Could not make the shell interactive.\n");
                exit(EXIT_FAILURE);
        }
}

void signalHandler_child(int p, siginfo_t* info, void* vp){
	//printf("\nremove");
	removeChildProcessByPID(info -> si_pid);
	while (waitpid(-1, NULL, WNOHANG) > 0) {
	}
}
void signalHandler_int(int p){
	if (kill(pid, SIGTERM) == 0){
		no_reprint_prmpt = 1;			
	}
}
void signalHandler_tstp(int p){
	//printf("\nsigtstp\n");	
	setChildProcessStateByPID(pid, 0);
	if (kill(pid,SIGTSTP) == 0){
		no_reprint_prmpt = 0;			
	}
}
void shellPrompt(){
	char hostn[1204] = "";
	gethostname(hostn, sizeof(hostn));
	printf("%s@%s %s > ", getenv("LOGNAME"), hostn, getcwd(currentDirectory, 1024));
}

int changeDirectory(char* args[]){
	if (args[1] == NULL) {
		chdir(getenv("HOME")); 
		return 1;
	}
	else{

		if (chdir(args[1]) == -1) {
			printf(" %s: no such directory\n", args[1]);
            return -1;
		}
	}
	return 0;
}

int manageEnviron(char * args[], int option){
	char **env_aux;
	switch(option){
		case 0: 
			for(env_aux = environ; *env_aux != 0; env_aux ++){
				printf("%s\n", *env_aux);
			}
			break;
		case 1:			
			if(args[1] == NULL){
				printf("%s","Not enought input arguments\n");
				return -1;
			}
			char *delim = "=";
			char *variable = strtok(args[1], delim);
			char *value = strtok(NULL, delim);

			if(getenv(variable) != NULL){
				printf("%s", "variable overwritten\n");
			}
			else{
				printf("%s", "variable created\n");
			}
			if (value == NULL){
				setenv(variable, "", 1);
			}
			else{
				setenv(variable, value, 1);
			}
			break;
		case 2:
			if(args[1] == NULL){
				printf("%s","Not enought input arguments\n");
				return -1;
			}
			if(getenv(args[1]) != NULL){
				unsetenv(args[1]);
				printf("%s", "variable deleted\n");
			}
			else{
				printf("%s", "variable does'nt exist\n");
			}
			break;
	}
	return 0;
}
void addChilProcessToList(int background, pid_t pid){
	CP *cp = malloc(sizeof(CP));
	cp -> pid = pid;
	cp -> background = 0;
	cp -> state = 1;
	strcpy(cp -> prompt, lineBuffer); 
	cp -> next = NULL;
	int i;
	if(childs == NULL){
		i = 1;
		childs = cp;
		if(background)
			printf("[%d] %s\t%s", i, (cp->state == 1) ? "Runnung" : "Stopped", cp->prompt );
	}
	else{
		CP *c = childs;
		i = 2;
		while(c -> next != NULL){
			i++;
			c = c -> next;
		}
		c -> next = cp;
		if(background)
			printf("[%d] %s\t%s", i, (cp->state == 1) ? "Runnung" : "Stopped", cp->prompt );
	}

}
void removeChildProcessByPID(pid_t pid){
	CP *pre, *cur;
	pre = cur = childs;
	while(cur != NULL){
		if(cur -> pid == pid){
			if(pre != cur){
				pre -> next = cur -> next;
				free(cur);
			}
			else{
				free(cur);
				childs = NULL;
			}
			break;
		}
		else{
			pre = cur;
			cur = cur -> next;
		}
	}	
}
void setChildProcessStateByPID(pid_t pid, int state){
	CP *cur = childs;
	int i = 1;
	while(cur != NULL){
		if(cur -> pid == pid){
			cur -> state = state;
			cur -> background = 1;
			printf("[%d] %s\t%s",i , (cur->state == 1) ? "Runnung" : "Stopped", cur->prompt );
			break;
		}
		else{
			cur = cur -> next;
			i++;
		}
	}
}
void launchProg(char **args, int background){	 
	 int err = -1;
	 
	 if((pid=fork())==-1){
		 printf("Child process could not be created\n");
		 return;
	 }
	if(pid==0){
		setenv("parent",getcwd(currentDirectory, 1024),1);	
		if (execvp(args[0],args)!=0){
			printf("Command not found");
			kill(getpid(),SIGTERM);
		}
	 }
	 signal(SIGINT, signalHandler_int);
	 signal(SIGTSTP, signalHandler_tstp);

	 if (background == 0){	 
		 addChilProcessToList(background, pid);
		 pause();
	 }else{
		 addChilProcessToList(background, pid);
	 } 
}

void fileIO(char * args[], char* inputFile, char* outputFile, int option){
	 
	int err = -1;
	
	int fileDescriptor;
	
	if((pid=fork())==-1){
		printf("Child process could not be created\n");
		return;
	}
	if(pid==0){
		if (option == 0){
			fileDescriptor = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600); 
			dup2(fileDescriptor, STDOUT_FILENO); 
			close(fileDescriptor);
		}else if (option == 1){
			fileDescriptor = open(inputFile, O_RDONLY, 0600);  
			dup2(fileDescriptor, STDIN_FILENO);
			close(fileDescriptor);
			fileDescriptor = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
			dup2(fileDescriptor, STDOUT_FILENO);
			close(fileDescriptor);		 
		}
		 
		setenv("parent",getcwd(currentDirectory, 1024),1);
		
		if (execvp(args[0],args)==err){
			printf("err");
			kill(getpid(),SIGTERM);
		}		 
	}
	waitpid(pid,NULL,0);
}

void pipeHandler(char * args[]){
	int filedes[2];
	int filedes2[2];
	
	int num_cmds = 0;
	
	char *command[256];
	
	pid_t pid;
	
	int err = -1;
	int end = 0;
	
	
	int i = 0;
	int j = 0;
	int k = 0;
	int l = 0;
	
	
	while (args[l] != NULL){
		if (strcmp(args[l],"|") == 0){
			num_cmds++;
		}
		l++;
	}
	num_cmds++;
	
	
	while (args[j] != NULL && end != 1){
		k = 0;
		
		while (strcmp(args[j],"|") != 0){
			command[k] = args[j];
			j++;	
			if (args[j] == NULL){
				end = 1;
				k++;
				break;
			}
			k++;
		}
		command[k] = NULL;
		j++;		
		

		if (i % 2 != 0){
			pipe(filedes);
		}else{
			pipe(filedes2);
		}
		
		pid=fork();
		if(pid==-1){			
			if (i != num_cmds - 1){
				if (i % 2 != 0){
					close(filedes[1]);
				}else{
					close(filedes2[1]);
				} 
			}			
			printf("Child process could not be created\n");
			return;
		}
		if(pid==0){
			if (i == 0){
				dup2(filedes2[1], STDOUT_FILENO);
			}
			else if (i == num_cmds - 1){
				if (num_cmds % 2 != 0){
					dup2(filedes[0],STDIN_FILENO);
				}else{
					dup2(filedes2[0],STDIN_FILENO);
				}
			}else{
				if (i % 2 != 0){
					dup2(filedes2[0],STDIN_FILENO); 
					dup2(filedes[1],STDOUT_FILENO);
				}else{
					dup2(filedes[0],STDIN_FILENO); 
					dup2(filedes[1],STDOUT_FILENO);					
				} 
			}
			
			if (execvp(command[0],command)==err){
				kill(getpid(),SIGTERM);
			}		
		}
		if (i == 0){
			close(filedes2[1]);
		}
		else if (i == num_cmds - 1){
			if (num_cmds % 2 != 0){					
				close(filedes[0]);
			}else{					
				close(filedes2[0]);
			}
		}else{
			if (i % 2 != 0){					
				close(filedes2[0]);
				close(filedes[1]);
			}else{					
				close(filedes[0]);
				close(filedes2[1]);
			}
		}
		//pause();
						
		i++;	
	}
	waitpid(pid,NULL,0);
}
			
int commandHandler(char * args[]){
	int i = 0;
	int j = 0;
	
	int fileDescriptor;
	int standardOut;
	
	int aux;
	int background = 0;

	char *args_aux[256];
	
	while ( args[j] != NULL){
		if ( (strcmp(args[j],">") == 0) || (strcmp(args[j],"<") == 0) || (strcmp(args[j],"&") == 0)){
			break;
		}
		args_aux[j] = args[j];
		j++;
	}
	args_aux[j] = NULL;

	if(strcmp(args[0],"exit") == 0) 
		exit(0);

 	else if (strcmp(args[0],"pwd") == 0){
		if (args[j] != NULL){
			if ( (strcmp(args[j],">") == 0) && (args[j+1] != NULL) ){
				fileDescriptor = open(args[j+1], O_CREAT | O_TRUNC | O_WRONLY, 0600); 
				standardOut = dup(STDOUT_FILENO);		
				dup2(fileDescriptor, STDOUT_FILENO); 
				close(fileDescriptor);
				printf("%s\n", getcwd(currentDirectory, 1024));
				dup2(standardOut, STDOUT_FILENO);
			}
		}
		else{
			printf("%s\n", getcwd(currentDirectory, 1024));
		}
	} 
 	
	else if (strcmp(args[0],"cd") == 0) 
		changeDirectory(args);

	else if (strcmp(args[0],"env") == 0){
		if (args[j] != NULL){
			if ( (strcmp(args[j],">") == 0) && (args[j+1] != NULL) ){
				fileDescriptor = open(args[j+1], O_CREAT | O_TRUNC | O_WRONLY, 0600); 
				standardOut = dup(STDOUT_FILENO); 													
				dup2(fileDescriptor, STDOUT_FILENO); 
				close(fileDescriptor);
				manageEnviron(args,0);
				dup2(standardOut, STDOUT_FILENO);
			}
		}
		else{
			manageEnviron(args,0);
		}
	}

	else if (strcmp(args[0],"export") == 0)
		manageEnviron(args,1);

	else if (strcmp(args[0],"unset") == 0) 
		manageEnviron(args,2);

	else if(strcmp(args[0],"jobs") == 0) {
		CP *c;
		int i = 1;
		for(c = childs; c != NULL; c = c -> next){
			printf("[%d] %s\t%s", i++, (c->state == 1) ? "Running" : "Stopped", c -> prompt);	
		}		
	}
	else if(strcmp(args[0], "fg") == 0){
		if(args[1] == NULL){
			if(childs == NULL){
				printf("fg: current: no such job\n");
				return 1;
			}
	 		signal(SIGINT, signalHandler_int);
	 		signal(SIGTSTP, signalHandler_tstp);

			CP *c = childs;
			while(c->next != NULL){
				c = c -> next;
			}
			if(c->prompt[strlen(c->prompt)-2] == '&'){
				c->prompt[strlen(c->prompt)-2] = '\n';
				c->prompt[strlen(c->prompt)-1] = '\0';
			}
			printf("%s",c->prompt);
			pid = c -> pid;
			c -> state = 1;
			c -> background = 0;
			kill(pid, SIGCONT);
			pause();
		}
		else{
			int i;
			sscanf(args[1],"%d", &i);
			CP *c = childs;
			int j = 0;
			while(c != NULL){
				j++;
				if(i == j){
					break;				
				}
				c = c -> next;
			}
			if(c == NULL){
				printf("fg: current: no such job\n");
				return 1;			
			}
			signal(SIGINT, signalHandler_int);
	 		signal(SIGTSTP, signalHandler_tstp);

			if(c->prompt[strlen(c->prompt)-2] == '&'){
				c->prompt[strlen(c->prompt)-2] = '\n';
				c->prompt[strlen(c->prompt)-1] = '\0';
			}
			printf("%s",c->prompt);
			pid = c -> pid;
			c -> state = 1;
			c -> background = 0;
			kill(pid, SIGCONT);
			pause();
		}
	}
	else if(strcmp(args[0], "bg") == 0){
		if(args[1] == NULL){
			if(childs == NULL){
				printf("bg: current: no such job\n");
				return 1;
			}
			CP *c = childs;
			while(c->next != NULL){
				c = c -> next;
			}
			if(c->prompt[strlen(c->prompt)-2] != '&'){
				int len = strlen(c->prompt);
				c->prompt[len-1] = '&';
				c->prompt[len] = '\n';
				c->prompt[len+1] = '\0';
			}
			pid = c -> pid;
			c -> state = 1;
			c -> background = 1;
			kill(pid, SIGCONT);
			printf("[%d] %s\t%s", i, (c->state == 1) ? "Running" : "Stopped", c -> prompt);
		}
		else{
			int i;
			sscanf(args[1],"%d", &i);
			CP *c = childs;
			int j = 0;
			while(c != NULL){
				j++;
				if(i == j){
					break;				
				}
				c = c -> next;
			}
			if(c == NULL){
				printf("bg: current: no such job\n");
				return 1;			
			}
			if(c->prompt[strlen(c->prompt)-2] != '&'){
				int len = strlen(c->prompt);
				c->prompt[len-1] = '&';
				c->prompt[len] = '\n';
				c->prompt[len+1] = '\0';
			}
			pid = c -> pid;
			c -> state = 1;
			c -> background = 1;
			kill(pid, SIGCONT);
			printf("[%d] %s\t%s", i, (c->state == 1) ? "Running" : "Stopped", c -> prompt);
		}
	}
	else{
		while (args[i] != NULL && background == 0){
			if (strcmp(args[i],"&") == 0){
				background = 1;
			}
			else if (strcmp(args[i],"|") == 0){
				pipeHandler(args);
				return 1;
			}
			/*else if (strcmp(args[i],"<") == 0){
				aux = i+1;
				if (args[aux] == NULL || args[aux+1] == NULL || args[aux+2] == NULL ){
					printf("Not enough input arguments\n");
					return -1;
				}
				else{
					if (strcmp(args[aux+1],">") != 0){
						printf("Usage: Expected '>' and found %s\n",args[aux+1]);
						return -2;
					}
				}
				fileIO(args_aux,args[i+1],args[i+3],1);
				return 1;
			}*/
			else if (strcmp(args[i],"<") == 0){
				if (args[i+1] == NULL){
					printf("Not enough input arguments\n");
					return -1;
				}
				fileIO(args_aux,args[i+1],NULL,1);
				return 1;
			}
			else if (strcmp(args[i],">") == 0){
				if (args[i+1] == NULL){
					printf("Not enough input arguments\n");
					return -1;
				}
				fileIO(args_aux,NULL,args[i+1],0);
				return 1;
			}
			i++;
		}
		launchProg(args_aux,background);
	}
	return 1;
}
void handleExpandOP(char ***files,int *f,char *token){
	DIR *d;
	struct dirent *dir;
	d = opendir(".");
	int i = 0;
	if (d){
		while ((dir = readdir(d)) != NULL){
			if(strcmp(dir->d_name,"..") == 0 || strcmp(dir->d_name,".") == 0)
				continue;
			(*files)[i++] = dir->d_name;
		}
		closedir(d);
	}
	if(strcmp(token, "*") == 0){
		token[0] = '.';
		token[1] = '*';
		token[2] = '\0';
	}
	*f = i;
	int reti;
	char **tmp = malloc(sizeof(char*)*1000);
	int len = 0;
	int j;
	for(j = 0; j < *f; j++){
		regex_t regex;
		regcomp(&regex, token, 0);
		reti = regexec(&regex, (*files)[j], 0, NULL, REG_EXTENDED);
		if (!reti) {
			tmp[len++] = (*files)[j];
		}
		regfree(&regex);
	}
	*f = len;
	*files = tmp;
}
char *trimwhitespace(char *str){
  char *end;
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  *(end+1) = 0;
  return str;
}

int main(int argc, char *argv[], char ** envp) {
	char * tokens[LIMIT];
	char line[MAXLINE];
	int numTokens;
		
	no_reprint_prmpt = 0;
	pid = -10;
	
	init();
    
	environ = envp;
	
	setenv("shell",getcwd(currentDirectory, 1024),1);
	
	while(TRUE){	
		signal(SIGINT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		if (no_reprint_prmpt == 0) shellPrompt();
		no_reprint_prmpt = 0;
		
		memset ( line, '\0', MAXLINE );

		if(fgets(line, MAXLINE, stdin) == NULL){
			no_reprint_prmpt = 1;
			continue;
			//printf("\n");
		}

		strcpy(lineBuffer, line);
		lineBuffer[strlen(lineBuffer)-1] = '\0';
		strcpy(lineBuffer,trimwhitespace(lineBuffer));
		int len = strlen(lineBuffer);
		lineBuffer[len] = '\n';
		lineBuffer[len+1] = '\0';

		if((tokens[0] = strtok(line," \n\t")) == NULL) continue;
		
		numTokens = 1;
		while((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++;
		if(tokens[numTokens-1][strlen(tokens[numTokens-1])-1] == '&' && strlen(tokens[numTokens-1]) != 1){
			tokens[numTokens-1][strlen(tokens[numTokens-1])-1] = '\0';		
			tokens[numTokens] = "&";
			numTokens++;
		}	    
		int i, j, k=1;
		char **allTokens = malloc(sizeof(char*)*1000);
		allTokens[0] = tokens[0];
		for(j = 1; j < numTokens; j++){	
			char **files = malloc(sizeof(char*)*1000);
			int len;
			int reg = 0;	
			char *c;
			for(c = tokens[j]; (*c) != '\0'; c++){
				if(*c == '*' || *c == '?'){
					if(*c == '?')
						*c = '*';
					reg = 1;
				}
			}
			if(reg){
				handleExpandOP(&files, &len, tokens[j]); 
				for(i = 0; i < len; i++){
					allTokens[k++] = files[i];
				}
			}
			else{
				allTokens[k++] = tokens[j];
			}
		}
		allTokens[k] = NULL;
		commandHandler(allTokens);
	}          

	exit(0);
}
