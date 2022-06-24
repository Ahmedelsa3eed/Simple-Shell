#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/resource.h>
#define SIZE 100

char input[SIZE], *cmd, *parm[SIZE], key[10]={'\0'}, value[10]={'\0'}, *new_echo_key;

/*extract the cmd and the parameters*/
void parse_input(){
    int i;
    char str[100];
    strcpy(str, input);

    char* token = strtok(str, " ");
    cmd = token;
    cmd[strlen(cmd)] = '\0';
    parm[0] = token;
    i = 1;
    while( token != NULL ){
        if( strcmp(cmd, "export") == 0 ){//with quotes or without: key=parm[1], val=parm[2]
            token = strtok(NULL, "=");
        }
        else if( strcmp(cmd, "echo") == 0 ){
            token = input+5;
            parm[1] = token;
            break;
        }
        else{
            token = strtok(NULL, " ");
        }
        if( token == NULL){
            parm[i] = token;
            break; 
        }
        parm[i] = token;
        if(parm[i][0] == '\"') parm[i]++;
        if( parm[i][strlen(parm[i])-1] == '\"'){
                parm[i][strlen(parm[i])-1] = '\0';
        }
        i++;
    }
    
}

/*extract the stored value into the parameters array*/
void evaluate(){
    int i,j,p;
    for(i = 0; input[i] != '\0'; i++){
        if(input[i] == '$'){

            if(value[0] != '\0' ){
                
                char *token = strtok(value, " ");
                p = 1;
                parm[p++] = token;

                while(token != NULL){
                    token = strtok(NULL, " ");
                    parm[p++] = token;
                }
            }
        }
    }
}

/*return true if the input type is built-in ,false otherwise*/
int input_type(){
    if((strcmp(cmd, "cd") == 0) ||
     (strcmp(cmd, "echo") == 0) ||
     (strcmp(cmd, "export") == 0) ||
     (strcmp(cmd, "pwd") == 0) ){
        return 1;
    }else{
        return 0;
    }
}

/*return 1 for "cd", 2 for "echo" , 3 for "export" and 4 for pwd*/
int builtin_type(){
    if(strcmp(cmd, "cd") == 0) return 1;
    else if(strcmp(cmd, "echo") == 0) return 2;
    else if(strcmp(cmd, "export") == 0) return 3;
    else if(strcmp(cmd, "pwd") == 0) return 4;
}

/*change the directory to the current working directory*/
void setup_environment(){
    chdir(getcwd(NULL, 0));
}

/*Accept input of two forms,
 either a string without spaces, or a full string inside double quotations*/
void evaluate_export(){
    strcpy(key, parm[1]);
    strcpy(value, parm[2]);
}

/*input to echo must be within double quotations */
void evalute_echo(){
    int i, k=0, flag = 0;
    char tmp[100] = {'\0'};
    for(i = 0; parm[1][i] != '\0'; i++){
        if(parm[1][i] == '\"')
            continue;
        if(parm[1][i] == '$'){
            int j;
            char tmpKey[10] = {'\0'};
            for(j=0,i++; parm[1][i] != ' ' && parm[1][i] != '\"'; j++,i++)
                tmpKey[j] = parm[1][i];
            if(key[0] == '\0' || !(strcmp(key, tmpKey) == 0) ){
                flag = 1;
                printf("Error: Variable does not exist!");
                break;
            }
            if(value[0] != '\0')
                i = i + strlen(key);
        }else{
            tmp[k++] = parm[1][i];
        }
    }
    if(!flag){
        printf("%s", tmp);
        if(value[0] != '\0')
            printf("%s", value);
    }
    printf("\n");
}

void execute_shell_builtin(){
    switch (builtin_type()){
    case 1:{//cd
        if(parm[1] == NULL ){
            parm[1] = "/home";
        }
        if(strcmp(parm[1], "~") == 0){
            parm[1] = "/";
        }
        if( chdir(parm[1]) != 0 )
            perror("cd failed");
        break;
    }case 2:{//echo
        evalute_echo();
        break;
    }case 3:{//export 
        evaluate_export();
        break;
    }case 4:{//pwd
        printf("%s\n", getcwd(NULL, 0));
        break;
    }default:
        break;
    }
}

void write_to_log_file(){
    FILE *file;
    file = fopen("log.txt", "a");
    if (file == NULL){
        printf("Error opening file!\n");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "Child process was terminated\n");
    fclose(file);
}

void reap_child_zombie(){
    while(1){
        pid_t id = wait(NULL);
        if(id == -1){
            break;
        }else if(id > 0){
            write_to_log_file();
        }
    }
}

void on_child_exit(){
    reap_child_zombie();
    write_to_log_file();
}

void register_child_signal(){
    signal (SIGCHLD, on_child_exit);
}

void execute_command(){
    evaluate();
    int statusPtr;
    pid_t child_id, end_id;
    if( (child_id = fork()) == -1){//Error
        perror("fork error");
        exit(EXIT_FAILURE);
    }  
    else if(child_id  == 0){//child 
        execvp(cmd, parm); 
        printf("Error in execvp!\n");
        exit(EXIT_FAILURE);
    }
    else{//parent
        if((input[strlen(input) - 1] == '&')){//should not wait (backgroud)
            return;
        }
        else{//foregroud
            end_id = waitpid(child_id, &statusPtr, 0);
            if (end_id == -1) {            
                perror("waitpid error");
            } 
        }
            
    }
}

void shell(){
    do{
        int i;
        for(i = 0; i<100;  i++)
            parm[i] = '\0';
        printf(">> ");
        fgets(input, 100, stdin);
        input[strlen(input) - 1] = '\0';
        parse_input();
        if(strcmp(cmd, "exit") == 0) break;
        if(input_type()){//shell built_in
            
            execute_shell_builtin();
        }
        else{//executable or error
            execute_command();
        }
    }
    while( cmd == '\0' || !(strcmp(cmd, "exit") == 0) );
    
}

int main(int argc, char* argv[]){
    register_child_signal();
    setup_environment();
    shell();
    return 0;
}