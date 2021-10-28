#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#define MAX_LINE 80
#define MAX_ARGS 40
#define HISTORY_PATH ".history"


//Variables
int p_wait;
int in_file, out_file;
int saved_in, saved_out;
int in, out;
int save_c;


/** Parse Input Algorithm
 * Takes in pointer to command array, pointer to args array
 * 1.First find the length of the commmand
 * 2. Begin incrementing in the array
 * 3. If command array signals the existance of characters/ blank spaces input into the args array
 * 4. If ampersand is used then initiate the wait process
 **/ 
void parseInput(char *command, char **args)
{
  int args_count = 0;
  int command_len = strlen(command);
  int arg_start = -1;
  for(int i = 0; i < command_len; i++)
  {
    if(command[i] == ' ' || command[i] == '\t' || command[i] == '\n')
    {
      if(arg_start != -1)
      {
        args[args_count++] = &command[arg_start];
        arg_start = -1;
      }
      command[i] = '\0';
    }
    else
    {
      if(command[i] == '&')
      {
        p_wait = 0;
        i = command_len;
      }
      if(arg_start == -1) arg_start = i;
    }
  }
  args[args_count] = NULL;
}
/**Flag  Checker Algorithm
 * Passed parameters [Pointer to the Args array]
 * 1. Iterate through the args array until you reach the redirection operators
 * 2. Check to see if operator has anything on the other side
 * 3. Initiate flag values
 * */


void checkFlags(char **args)
{
  for(int i = 0; args[i] != NULL; i++)
  {
    if(!strcmp(args[i], ">"))
    {
      printf("found >");
      if(args[i+1] == NULL)
        printf("Invalid command format\n");
      else
        out_file = i + 1;
    }
    if(!strcmp(args[i], "<"))
    {
      printf("found < ");
      if(args[i+1] == NULL)
        printf("Invalid command format\n");
      else
        in_file = i + 1;
    }
    
  }
}
/** Manage History Algorithm
 * Compared to my last attempt I now initialize a direct history path to access previous commands
 * 1. Open the history path and check to see if it is null
 * 2. Manage the contents of the history
 **/ 
void manageHistory(char **args)
{
  FILE* h = fopen(HISTORY_PATH, "r");
  if(h == NULL)
  {
    printf("The history is empty\n");
  }
  else
  {
    if(args[1] == NULL)
    {
      char c = fgetc(h);
      while(c != EOF)
      {
        printf ("%c", c);
        c = fgetc(h);
      }
    }
    else if(!strcmp(args[1], "-c"))
    {
      save_c = 0;
      remove(HISTORY_PATH);
    }
    else
    {
      printf("[!] Invalid syntax");
    }
    fclose(h);
  }
}
/** Execution algorithm
 * Takes in pointer to the args array
 * check to see if execvp returns -1
 **/ 
void execute(char **args)
{

  if(execvp(args[0], args) < 0)
  {
    printf("Command not found\n");
    exit(1);
  }
}
//Saves command to history
void saveCommand(char *command)
{
  FILE* h = fopen(HISTORY_PATH, "a+");
  fprintf(h, "%s", command);
  rewind(h);
}

int main(void)
{
  char command[MAX_LINE];
  char last_command[MAX_LINE];
  char parse_command[MAX_LINE];
  char *args[MAX_ARGS];
  int should_run = 1, history = 0;
  int alert;
  
  
  while(should_run)
  {
    printf("osh> ");
    fflush(stdout);
    fgets(command, MAX_LINE, stdin);
    
    p_wait = 1;
    alert = 0;
    out_file = in_file = -1;
   
    save_c = 1;

    strcpy(parse_command, command);
    parseInput(parse_command, args);

    if(args[0] == NULL || !strcmp(args[0], "\0") || !strcmp(args[0], "\n")) continue;

    if(!strcmp(args[0], "exit"))
    {
      should_run = 0;
      continue;
    }
    //Call the history function using !! then copy the command and re run it
    //if there exists no history then remind the user
    if(!strcmp(args[0], "!!"))
    {
      if(history)
      {
        printf("%s", last_command);
        strcpy(command, last_command);
        strcpy(parse_command, command);
        parseInput(parse_command, args);
      }
      else
      {
        printf("No commands in history \n");
        continue;
      }
    }
//File I/O Section
/**File I/O Algorithm
 * 1. call checkFlags in order to set flag values properly
 * 2. If infile or outfile returns the position of the file execute File I/O operations
 * 3. Else return an error per case and raise the alert flags
 **/ 
    checkFlags(args);

    if(in_file != -1)
    {
      in = open(args[in_file], O_RDONLY);
      if(in < 0)
      {
        printf("Failed to open file \'%s\'\n", args[in_file]);
        alert = 1;
      }
      else
      {
        saved_in = dup(0);
        dup2(in, 0);
        close(in);
        args[in_file - 1] = NULL;
      }
    }
    //NOTE: These flags allow for the writing and creation of user input file names
    if(out_file != -1)
    {
      out = open(args[out_file], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
      if(out < 0)
      {
        printf("Failed to open file \'%s\'\n", args[out_file]);
        alert = 1;
      }
      else
      {
        saved_out = dup(1);
        dup2(out, 1);
        close(out);
        args[out_file - 1] = NULL;
      }
    }
    //Program Execution Block

    /**
     * Execution Block Algorithm
     * 1. Create the processs thread pid for fork()
     * 2. First check to see if history is being called,  if it is then call manageHistory
     * 3. Then check for stop or continue. 
     * 4. If pid goes through then execute the command/else wait
     * 5. Then save the command entered
     **/ 
    if(!alert && should_run)
    {
      pid_t pid=fork();
      if(!strcmp(args[0], "history")) manageHistory(args);
      else
      {
        if(!strcmp(args[0], "stop") || !strcmp(args[0], "continue"))
        {
          args[2] = args[1];
          args[3] = NULL;
        }
        if(pid== 0)
        {          
            execute(args);
        }
        else
        {
          if(p_wait) wait(NULL);
        }
      }
      strcpy(last_command, command);
      if(save_c) saveCommand(command);
      history = 1;
    }
    //after the Program execution block dup2() the filenames in and out
    dup2(saved_out, 1);
    dup2(saved_in, 0);
  }
  return 0;
}