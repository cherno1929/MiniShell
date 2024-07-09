#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "parser.h"

typedef struct {
	int pidBg;
	char lineBg[1024];
}linBg; // Lineas del proceso Background

typedef struct {
	linBg *arr;
	int cont;
}arrBg; // Array de procesos Background

int status;

arrBg *arBg;

mode_t num;

//SIGNAL FUNCTIONS

void cntrC(){ // Funcion para evitar un SIGKILL
	printf("\n");
}

//SIGNAL FUNCTIONS

void ex1Com(tline *line1){ // Ejecutar comando en proceso hijo
	pid_t id;
	int status;
	id = fork();
	if (id == 0) { // Hijo	
		if(execv(line1->commands[0].filename, line1->commands[0].argv) == -1){
			fprintf(stderr, "No existe el comando : %s\n", line1->commands[0].argv[0]);
			exit(1);
		}
		exit(0);
	}else { // Padre
		waitpid(id, &status, 0);
	}
}

int exeNCom(tline *lineM){ // Ejecutar más de un comando (Pipe)

	int nCom = lineM->ncommands;
	int pipes[nCom - 1][2], pids[nCom];

	for (int i = 0; i < nCom - 1; i++){
		pipe(pipes[i]);
	}

	for (int i = 0; i < nCom; i++){
		pids[i] = fork();
		if (pids[i] == 0){ //Hijo
			for(int j = 0; j < nCom - 1; j++){ // Pipes Managment
				if (i == 0 && j == 0){ // 1º Hijo
					close(pipes[j][0]);
				}else if (i == nCom - 1 && j == nCom - 2){ // Ultimo hijo
					close(pipes[j][1]);
				}else{
					if (i - 1 != j){
						close(pipes[j][0]);
					}
					if(i != j){
						close(pipes[j][1]);
					}
				}
			}
			if (i == 0){ //1º Hijo
				dup2(pipes[i][1], STDOUT_FILENO);
				close(pipes[i][1]);
				if(execvp(lineM->commands[i].filename, lineM->commands[i].argv) == -1){
					fprintf(stderr, "No existe el comando : %s\n", lineM->commands[i].argv[0]);
					exit(1);
				}
			}else if(i == nCom - 1){ // Ultimo hijo
				dup2(pipes[i-1][0], STDIN_FILENO);
				close(pipes[i-1][0]);
				if(execvp(lineM->commands[i].filename, lineM->commands[i].argv) == -1){
					fprintf(stderr, "No existe el comando : %s\n", lineM->commands[i].argv[0]);
					exit(1);
				}
			}else{ //Demás casos
				dup2(pipes[i-1][0], STDIN_FILENO);
				dup2(pipes[i][1], STDOUT_FILENO);
				close(pipes[i-1][0]);
				close(pipes[i][1]);
				if(execvp(lineM->commands[i].filename, lineM->commands[i].argv) == -1){
					fprintf(stderr, "No existe el comando : %s\n", lineM->commands[i].argv[0]);
					exit(1);
				}
			}
			return 0;
		}
	}

	//Main

	for (int i = 0; i < nCom - 1; i++){ // Cerrar Pipes
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	for (int i = 0; i < nCom; i++){ //Liberar a los hijos
		wait(NULL);
	}

	return 0;
}

void execCom(tline *line, int in, int out, int error){
	int cpyIn, cpyOut,cpyErr;
	cpyIn = dup(STDIN_FILENO);
	cpyOut = dup(STDOUT_FILENO);
	cpyErr = dup(STDERR_FILENO);

	//Preparar las entradas y salidas

	int fd;
	num = umask(0);
	umask(num);

	if (in == 1){
		fd = open(line->redirect_input, O_RDONLY);
		if (fd == -1 || dup2(fd, STDIN_FILENO) == -1){
			fprintf(stderr, "No se puedo redirigir la entrada\n");
			exit(2);
		}
		close(fd);
	}
	if (out == 1){
		fd = open(line->redirect_output, O_WRONLY | O_CREAT, 0666 - num);
		if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1){
			fprintf(stderr, "No se puedo redirigir la salida\n");
			exit(2);
		}
		close(fd);
	}
	if (error == 1){
		fd = open(line->redirect_error, O_WRONLY | O_CREAT, 0666 - num);
		if (fd == -1 || dup2(fd, STDERR_FILENO) == -1){
			fprintf(stderr, "No se puedo redirigir la salida de error\n");
			exit(2);
		}
		close(fd);
	}

	// COMMAND EXECUTION
	if (line->ncommands == 1){
		ex1Com(line);
	}else{
		exeNCom(line);
	}

	// Devolver a las entradas y salidas a la normalidad
	if (in == 1){
		if(dup2(cpyIn, STDIN_FILENO) == -1){
			fprintf(stderr, "No se ha podido reedirigir la entrada a su normalidad\n");
			exit(2);
		}
	}
	if (out == 1){
		if(dup2(cpyOut, STDOUT_FILENO) == -1){
			fprintf(stderr, "No se ha podido reedirigir la salida a su normalidad\n");
			exit(2);
		}
	}
	if (error == 1){
		if(dup2(cpyErr, STDERR_FILENO) == -1){
			fprintf(stderr, "No se ha podido reedirigir la salida a su normalidad\n");
			exit(2);
		}
	}
}

int showJobs(){ // Comando jobs
	if(arBg->cont <= 0){
		printf("No jobs por ahora\n");
	}else{
		for (int i = 0; i < arBg->cont; i++){
			printf("[%d] [%d]+ In Background		%s\n",i ,arBg->arr[i].pidBg, arBg->arr[i].lineBg);
		}
	}
	return 0;
}

void forgroundPr(char *data, int numArg){ // Comando Foreground
	int id;
	if (numArg != 2){
		fprintf(stderr, "Error en el número de parametros\n");
	}else{
		id = atoi(data);
		if (id >= 0 && id < arBg->cont){
			kill(arBg->arr[id].pidBg, SIGUSR2);
			kill(getpid(), SIGSTOP);
		}else{
			fprintf(stderr, "Error en el valor del job\n");
		}
	}
}

void forgroundPr2(){
	while (1){
		waitpid(getpid(), &status, WNOHANG); // Sigue con tu vida, no esperes pero estate atento del progreso
		if (WIFEXITED(status) == 1){
			// El proceso ha acabado, pero cual en el array?
			for (int i = 0; i < arBg->cont; i++){ 
				//Voy a buscarlo
				if (arBg->arr[i].pidBg == getpid()){
					printf("[%d] [%d]+ Finished		%s\n", i, arBg->arr[i].pidBg, arBg->arr[i].lineBg);
					//Ahora queda reestructurar el array
					for (int j = i; j < arBg->cont - 1; j++){
						memcpy(&arBg->arr[j], &arBg->arr[j + 1], sizeof(linBg));
					}
					break;
				}
			}
			arBg->cont--;
			break;
		}
	}
	kill(getppid(), SIGCONT);
}

int execBg(tline *line ,arrBg *arBg, int inp, int outp, int errorp, char *buf){ // Ejecutar comando Background
	int pid;
    pid = fork();
    if (pid == 0){ // Hijo
		signal(SIGINT,cntrC);
		signal(SIGUSR2, forgroundPr2);
		// Bloquear la entrada
		int fd1 = open("/dev/null", O_RDONLY);
		dup2(fd1, STDIN_FILENO);
		close(fd1);
		
        execCom(line, inp, outp, errorp);
		kill(getppid(), SIGUSR1);
        exit(0);
    }else if (pid >0){ // Padre
		int posArr = arBg->cont;
		arBg->cont++;
		arBg->arr[posArr].pidBg = pid;
		strcpy(arBg->arr[posArr].lineBg, buf);
		fprintf(stdout, "[%d] [%d]+ Running		%s\n", posArr, pid, buf);
    }else{
        fprintf(stderr, "Error en el fork");
        exit(-1);
    }
    return 0;
}

int killChildren(arrBg *arBg){ // Acabar con los procesos hijo
	for (int i = 0; i < arBg->cont; i++){
		kill(arBg->arr[i].pidBg, SIGKILL);
		waitpid(arBg->arr[i].pidBg, NULL, 0);
	}
	return 0;
}

void sig_FreePr(){ // Liberar proceso
	for (int i = 0; i < arBg->cont; i++){
		waitpid(arBg->arr[i].pidBg, &status, WNOHANG);
		if (WIFEXITED(status) == 1){ // En el momento en el que salga...
			printf("[%d] [%d]+ Finished		%s\n", i, arBg->arr[i].pidBg, arBg->arr[i].lineBg); // Se muestra en pantalla
			for (int j = i; j < arBg->cont - 1; j++){
				memcpy(&arBg->arr[j], &arBg->arr[j + 1], sizeof(linBg)); // Y se reajustan los contenidos de la memoria
			}
			arBg->cont--;
		}
	}
}

int changeMask(tline *line){ // Comando umask
	if (line->commands[0].argc == 2){
		num = strtol(line->commands[0].argv[1], NULL, 8);
		umask(num);
	}else if (line->commands[0].argc == 1){
		num = umask(0);
		printf("%04o\n", num);
		umask(num);
	}else{
		fprintf(stderr, "Error en el numero de argumentos\n");
	}
	return 0;
}

int chandeDir(tline *line){ // Comando cd
	char dir[1024];
	if (line->commands[0].argc == 2){
		if (strcmp(line->commands[0].argv[1], "HOME") == 0){
			chdir(getenv("HOME"));
		}else{
			chdir(line->commands[0].argv[1]);
		}
	}else if (line->commands[0].argc == 1){
		getcwd(dir, sizeof(dir));
		printf("Direcotorio actual : %s\n", dir);
	}else{
		fprintf(stderr, "Error en el numero de argumentos\n");
	}
	return 0;
}

int main() {
	char buf[1024];
	tline * line;
	
	// A partir de estas 2 siguientes lineas, las señales de interrupcion y la de usuario 1 tienen funciones personalizadas. En el caso de interrupcion, ahora no interrumpe, y el de usuario 1 ha recibido una funcion que puede usar
	signal(SIGINT,cntrC);
	signal(SIGUSR1, sig_FreePr);

	int inp, outp, errorp;

	// Reservar memoria compartida
	arBg = mmap( NULL, sizeof(arrBg), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );

	arBg->arr = (linBg *) malloc(sizeof(linBg) * 30); // Asignamos memoria de array para procesos background

	arBg->cont = 0; // Contador a 0

	printf("msh> "); // Prompt	
	while (1) {
		fgets(buf, 1024, stdin);
		line = tokenize(buf);
		if (line==NULL || strcmp(buf, "\n") == 0) { // Si el input es un enter o no se puso nada, se reintroduce el prompt
			printf("msh> ");
			continue;
		}
		if (strcmp(line->commands[0].argv[0],"exit") == 0) { // Si es un exit...
			killChildren(arBg); // Matamos todos los procesos hijos en background
			// Liberar la memoria compartida
			munmap(arBg, sizeof(arrBg));
			kill(getpid(), SIGTERM); // Matamos la consola
		}else if(strcmp(line->commands[0].argv[0],"jobs") == 0){ // Si pedimos los procesos en progreso
			showJobs();
		}else if (strcmp(line->commands[0].argv[0], "fg") == 0){ // Si queremos traer un comando a foreground
			forgroundPr(line->commands[0].argv[1], line->commands[0].argc);
		}else if (strcmp(line->commands[0].argv[0], "umask") == 0) { // Si queremos cambiar la mascara
			changeMask(line);
		}else if (strcmp(line->commands[0].argv[0], "cd") == 0){ // Si queremos cambiar de directorio
			chandeDir(line);
		}else{ // Si no es ninguno de los comandos considerados especiales, se ejecutan los comandos de forma anodina
			inp = line->redirect_input != NULL;
			outp = line->redirect_output != NULL;
			errorp = line->redirect_error != NULL;

			if (line->background){ // En caso de que se pida que el proceso sea en background, se ejecuta en un proceso hijo
				execBg(line ,arBg , inp, outp, errorp, buf);
			}else{ // Si no se pide en background, lo ejecuta la propia minishell
				execCom(line, inp, outp, errorp);
			}	
		}

		printf("msh> ");
		
	}

	return 0;
}