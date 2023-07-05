#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define L_BOUND 1024

/*
* Smallsh Program Breakdown (Received Full Points For This Project):
* In this assignment you will write smallsh your own shell in C. smallsh will implement a command line interface similar to well-known shells, such as bash. Your program will:
* 1) Print an interactive input prompt
* 2) Parse command line input into semantic tokens
* 3) Implement parameter expansion
* - Shell special parameters $$, $?, and $!
* - Tilde (~) expansion
* 4) Implement two shell built-in commands: exit and cd
* 5) Execute non-built-in commands using the the appropriate EXEC(3) function.
* - Implement redirection operators ‘<’ and ‘>’
* - Implement the ‘&’ operator to run commands in the background
* 6_Implement custom behavior for SIGINT and SIGTSTP signals
*/

// struct to hold arguments
typedef struct ArgumentInfo{
  char command[2048];
  char **words; 
  int bkgrnd;  
  char *in_file;
  char *out_file;
}Arguments;

// function for expansion
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);

// function for parsing words
int parse_words(Arguments *args_ptr, ssize_t word_index, ssize_t comment_word);

// function for redirecting input and ouput
void redirections(Arguments *args_ptr);


char cash_question[100] = "0"; // represents $?
char cash_exclamation[100] = ""; // represents $!

// signal handler for SIGINT
void sigint_handle(int sig){
  
}


// an informative message shall be printed to stderr and the value of the “$?” 
int main(void) {
  
  char *line = NULL;
  size_t n = 0;

  int add_mem = 10;
  pid_t *bkgrnd_pids = (pid_t *)malloc(add_mem * sizeof(pid_t));
  int pids_count = 0;
  
  
  //signal handling for sigtstp
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = SIG_IGN;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP,&SIGTSTP_action,NULL);
  
  // signal handling for sigint
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = sigint_handle;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT,&SIGINT_action,NULL);
  

  for (;;) {
    errno = 0;
    // declarations
    ssize_t word_index = -1; // element counter for number of words in array, will also act as index
    pid_t smallsh_pid = getpid();
    char sm_pid[100];
    const char* ifs = getenv("IFS");
    const char* ps1 = getenv("PS1");
    const char* home = getenv("HOME");
    //int increase_mem = 2; // multiplier incase we need to increase memory
    char* token;  
    ssize_t comment_word = -1; // will serve as a check and store the index of "#"
    int cmd_flag = 0; // if incase there is a need to check if there is a command 

    // initialize struct
    Arguments args;
    args.words = (char **)malloc(L_BOUND * sizeof(char *)); 
    args.bkgrnd = 0;
    args.in_file = NULL;
    args.out_file = NULL;
    
    // check environment variables if they are unset
    if (ps1 == NULL){
      ps1 = "";
    }
    if (home == NULL){
      home = "";
    }
    // to convert smallsh pid to string
    sprintf(sm_pid, "%d", smallsh_pid);

    // Step 1: Prompt
    // check for any unwaited background processes
    // Here we check the statuses of background children processes
    int process_count = 0;
    while (process_count < pids_count){
      int waitStatus;
      pid_t bkgrnd_process;

      if (bkgrnd_pids[process_count] != 0){
        bkgrnd_process = waitpid(bkgrnd_pids[process_count],&waitStatus,WNOHANG | WUNTRACED);
        
        if (bkgrnd_process == 0){
          process_count ++;
           continue;
          
        }else{
          if(WIFEXITED(waitStatus)) {
            int exit_status = WEXITSTATUS(waitStatus);
            bkgrnd_pids[process_count] = 0;
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bkgrnd_process, exit_status);
          }
    
          if(WIFSIGNALED(waitStatus)) {
            int sig_status = WTERMSIG(waitStatus);
            bkgrnd_pids[process_count] = 0;
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) bkgrnd_process, sig_status);
            fflush(NULL);
          }
          if(WIFSTOPPED(waitStatus)) {
            kill(bkgrnd_pids[process_count],SIGCONT);
            fprintf(stderr,"Child process %jd stopped. Continuing.\n",(intmax_t) bkgrnd_process);
        }

        }
      }
      process_count ++;
    }
    
    // prompt user and grab input
    fprintf(stderr,"%s",ps1);
    getline(&line, &n, stdin); /* Reallocates line */
    
    if (errno == EINTR){
      continue;
    }

    if (feof(stdin)){ // jump to exit built in command
      clearerr(stdin);
      strcpy(args.command, "exit");
      goto exit;
    }
   
   
    if (strcmp(line,"\n")==0){ 
      continue;
    }
    
    // Step 2: Splitting (here we tokenize)
    if (ifs != NULL){ // if the environment variable IFS is not unset
      token = strtok(line, ifs);

      while (token != NULL){
        
        if (cmd_flag == 0){
          strcpy(args.command, token);
          cmd_flag = 1;
          
        }
          word_index++;
          args.words[word_index] = strdup(token); //strdup already allocates memory
          
          if (strcmp(args.words[word_index], "#") == 0){ // if the word is a comment symbol
            comment_word = word_index;
          }
        
        token = strtok(NULL, ifs);
      }
      
    } else{ // otherwsie IFS is unset
      token = strtok(line," \t\n");

      while (token != NULL){
        
        if (cmd_flag == 0){
          strcpy(args.command, token);
          cmd_flag = 1;
          
          }
          word_index++;
          args.words[word_index] = strdup(token);
          
          if (strcmp(args.words[word_index], "#") == 0){
            comment_word = word_index;
          }
        
        token = strtok(NULL, " \t\n");
      }
    }

    if (strcmp(args.words[word_index], "\n")==0){ // check if last index is a newline character
      free(args.words[word_index]);
      word_index--;
    }
   
    // realloc() for original malloc()
    args.words = (char **)realloc(args.words,(word_index+1) * sizeof(char *)); // + 2 or 1 for end in null
    
    // Step 3: Expansion
    size_t i = 0;
    // expand the symbols into new substrings 
    while (i <= word_index){
      
      if ((strncmp(args.words[i],"~/",2)) == 0){
        str_gsub(&args.words[i],"~",home);
      }
      
        str_gsub(&args.words[i],"$$", sm_pid);
        str_gsub(&args.words[i],"$?", cash_question);
        str_gsub(&args.words[i],"$!", cash_exclamation);
      i++;
    }
    
    // Step 4: Parsing 
    ssize_t before_count = word_index;
    // this function assigns many of the special tokens to different variables so we can get rid of them for execvp()
    word_index = parse_words(&args, word_index, comment_word);
     
    // free the commetns and special token we no longer need in the words array
    while (word_index < before_count){
   
      free(args.words[before_count]);
      before_count--;
    }
    // realloc() with new words_index value from the value returned from parse_words()
    args.words = (char **)realloc(args.words,(word_index+1) * sizeof(char *));
    
    //step 5: execution
    // built-in commands
    if (strcmp(args.command,"cd")==0){
      if(word_index == 0){ // meaning there's no arguments, just a command
        if(chdir(home) != 0){
          perror("chdir() failed\n");
        }
      } else if(word_index == 1){
        if(chdir(args.words[word_index]) != 0){
          perror("chdir() failed\n");
        }
      } else{
       fprintf(stderr,"Too many arguments for cd command"); 
      }
      goto restart;
    }
    
exit:
    // here we check if the command is "exit" and perform certain operations based on the arguments given.
    if (strcmp(args.command,"exit")==0 ){
      int result;

      if(word_index == 0|| word_index == -1){ // meaning there's no arguments, or eof stdin
          fprintf(stderr, "\nexit\n");
          result = atoi(cash_question);
        
          int p_count1 = 0;
          while (p_count1 < pids_count){
            if (bkgrnd_pids[p_count1] != 0){
              kill(bkgrnd_pids[p_count1],SIGINT);
            }
            p_count1++;
          }
        
          exit(result);
        
      } else if(word_index == 1){
          int length = strlen(args.words[word_index]);
          for (i=0;i<length;i++){
            if (!isdigit(args.words[word_index][i])){
              fprintf(stderr,"error for exit() command, invalid input");
              goto restart;
            }
          }
          fprintf(stderr, "\nexit\n");
          result = atoi(args.words[word_index]);
        
          int p_count2 = 0;
          while (p_count2 < pids_count){
            if (bkgrnd_pids[p_count2] != 0){
              kill(bkgrnd_pids[p_count2],SIGINT);
            }
            p_count2++;
          }
        
          exit(result);
        
      } else{
       fprintf(stderr,"Too many arguments for exit command"); 
      }
      goto restart;
    }
    
    // non-built ins:
    int childStatus;
    args.words = (char **)realloc(args.words,(word_index+2) * sizeof(char *));
    args.words[word_index+1] = NULL;
  
  	// Fork a new process
  	pid_t spawnPid = fork();
    
  	switch(spawnPid){
  	case -1:
  		perror("fork()\n");
  		exit(1);
  		break;
  	case 0:
      redirections(&args);
  		int error = execvp(args.command, args.words);
      if (error == -1){
        fprintf(stderr, "Failed program execution for execvp()");
        sprintf(cash_question,"%d",getpid());
        exit(1);
      }
  		break;
      
  	default: 
      // parent branch, we check the exit,signal, or stop status of the last child process. Otherwise we add the new bkgrnd pid to our global variables.
      if (args.bkgrnd == 0){
  		  spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);

        if(WIFEXITED(childStatus)) {
          int exit_status = WEXITSTATUS(childStatus);
          sprintf(cash_question,"%d",exit_status);
        }
  
        if(WIFSIGNALED(childStatus)) {
          int sig_status = WTERMSIG(childStatus);
          sig_status += 128;
          sprintf(cash_question,"%d",sig_status);
        }

       if(WIFSTOPPED(childStatus)) {
         kill(spawnPid,SIGCONT);
         fprintf(stderr,"Child process %jd stopped. Continuing.\n",(intmax_t) spawnPid);
         sprintf(cash_question,"%d",spawnPid);
        }
 
        
      }else{ 
        sprintf(cash_exclamation,"%jd",(intmax_t)spawnPid);
        bkgrnd_pids[pids_count] = spawnPid;
        pids_count++;
        if (pids_count >= add_mem){
          add_mem = add_mem * 2;
          bkgrnd_pids = (pid_t *)malloc(add_mem * sizeof(pid_t));
        }
      }
      
  		break;
  	} 
    
  
  restart:
    // clean dynamic memory and continue to next iteration
   for (size_t i =0; i <= word_index; i++){
     free(args.words[i]);
   }
   free(args.words);

   if (args.in_file != NULL){
    free(args.in_file);
   }
   if (args.out_file != NULL){ 
   free(args.out_file);
   }
   fflush(NULL);
  }

  return 0;
}


/*
 * This function is from Professor Gambord. It is a function that replaces and resizes substrings within strings.
 * We use this function for the the variables we need to expand.
 */
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){

  // dereferenced haystack pointer for convenience
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle), sub_len = strlen(sub);
  

  for (; (str = strstr(str, needle));) {
    //offset of where string is in relation to haystack
    ptrdiff_t offset = str - *haystack;
  
    if (sub_len > needle_len) {
      
      // don't need str anymore can use it for different purposes
      // realloc can return null if it fails, losing our reference if we assigned this to *haystack
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      //
      
      //if str is null, haystack would stay the same
      if(!str) return NULL;
      
      // otherwise can reassign haystack 
      *haystack = str;
      str = *haystack + offset;
    
    }
    
    // move memory
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - offset - needle_len);
    // copy memory
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len;
  }
  // for shrinking
  str = *haystack;
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) return NULL;
    *haystack = str;
  }
  
  return str;
}

/*
 * This function parses the words we possibly need for input and output files as well as background flags.
 * This function returns the new last index of the words array since we no longer need the special tokens and 
 * comments near the end of the words array.
 */
int parse_words(Arguments *args_ptr, ssize_t word_index, ssize_t comment_word){
  
  int pop_count = 0; // the space we deallocate to words array according to special tokens found
  ssize_t current_index = word_index; // this index will be used for searching key token

  // if we have a comment we set the current_index to the comment index
  if (comment_word != -1) {
    current_index = comment_word-1;
    word_index = comment_word-1;
  }

  // we decrement current_index and set the background flag to 1 if "&"" is near end
  if (strcmp(args_ptr -> words[current_index],"&")==0){
    args_ptr -> bkgrnd = 1;
    current_index -= 1; 
    pop_count++;
  }

  if (current_index >= 2){ // want to make sure there are enough arguments in the array to index
    
    // locate and store input and output files if found
    if (strcmp(args_ptr -> words[current_index -1],">")==0){
      args_ptr -> out_file = strdup(args_ptr -> words[current_index]);
      pop_count += 2;
    }
  
    if (strcmp(args_ptr -> words[current_index - 1],"<")==0){
      args_ptr -> in_file = strdup(args_ptr -> words[current_index]);
      pop_count += 2;
    }
  }

  if (current_index >= 4){
    if (strcmp(args_ptr -> words[current_index - 3],">")==0){
      args_ptr -> out_file = strdup(args_ptr -> words[current_index-2]);
      pop_count += 2;
    }
  
    if (strcmp(args_ptr -> words[current_index - 3],"<")==0){
      args_ptr -> in_file = strdup(args_ptr -> words[current_index-2]);
      pop_count += 2;
    }
  }
  
  return word_index - pop_count;
}

/*
 * This function handles the redirection from the special token < and > inputted by the user
 */
void redirections(Arguments *args_ptr){
  int newSFD;
  int newTFD;
  
  // dup2() for stdin 
  if (args_ptr -> in_file != NULL) {
    int sourceFD = open(args_ptr -> in_file, O_RDONLY);

    if (sourceFD == -1){
      perror("source open()");
      exit(1);
      
    }else{
      newSFD = dup2(sourceFD, 0);
      if (newSFD == -1){
        perror("source dup2()\n");
        exit(2);
      }
    }
  }

  // dup2() for stdout
  if (args_ptr -> out_file != NULL) {
    int targetFD = open(args_ptr -> out_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    if (targetFD == -1){
      perror("target open()");
      exit(1);
      
    }else{
      newTFD = dup2(targetFD, 1);
      if (newTFD == -1){
        perror("target dup2()\n");
        exit(2);
      }
    }
  }
  
}
