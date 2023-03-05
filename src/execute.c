/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"
#include "command.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "quash.h"
#include "deque.h"


IMPLEMENT_DEQUE_STRUCT(pidQ, pid_t);
IMPLEMENT_DEQUE(pidQ, pid_t);
PROTOTYPE_DEQUE(pidQ, pid_t);
pidQ pidq;

typedef struct Job
{
    int jobId;
    char* command;
    bool isBackground;
    pidQ* pidq;
} Job;

IMPLEMENT_DEQUE_STRUCT(jobQueue, struct Job);
IMPLEMENT_DEQUE(jobQueue, struct Job);
jobQueue jq;
int currentJID = 1;
bool new_job = true;

int current_pipe = 0;
int next_pipe = 1;

//read is 0, write is 1
static int pipes[2][2];

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {
  // Get the current working directory. This will fix the prompt path.
  char* cwd = malloc(sizeof(char)*1024);
  getcwd(cwd, 1024);
  return cwd;
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly
  // (void) env_var; // Silence unused variable warning
  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status()
{
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.

  // TODO: Once jobs are implemented, uncomment and fill the following line
  // print_job_bg_complete(job_id, pid, cmd);

  int numOfJob = length_jobQueue(&jq);

  // iterate over the number of jobs
  for (int i = 0; i < numOfJob; i++)
  {
    Job currentJob = pop_front_jobQueue(&jq);
    int numOfPids = length_pidQ(&currentJob.pidq);
    pid_t atFront = peek_front_pidQ(&currentJob.pidq);
    for (int j = 0; j < numOfPids; j++)
    {
      pid_t currentPid = pop_front_pidQ(&currentJob.pidq);
      int theStatus;
      if (waitpid(currentPid, &theStatus, 1) == 0)
        push_back_pidQ(&currentJob.pidq, currentPid);
    }
    if (is_empty_pidQ(&currentJob.pidq))
    {
      print_job_bg_complete(currentJob.jobId, atFront, currentJob.command);
    }
    else
    {
      push_back_jobQueue(&jq, currentJob);
    }
  }
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char* exec = cmd.args[0];
  char** args = cmd.args;

  // (void) exec; // Silence unused variable warning
  // (void) args; // Silence unused variable warning

  execvp(exec, args);

  perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd) {
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** str = cmd.args;

  (void) str; // Silence unused variable warning

  int i = 0;
  while(str[i] != NULL){
    printf("%s", str[i]);
    i++;
  }
  printf("\n");

  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  // TODO: Remove warning silencers
  (void) env_var; // Silence unused variable warning
  (void) val;     // Silence unused variable warning

  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  char* curDir = get_current_directory(false);
  char* dir = cmd.dir;
  fflush(stdout);
  // Check if the directory is valid
  if (dir == NULL) {
    perror("ERROR: Failed to resolve path");
    return;
  }
  printf("HERE\n");
  fflush(stdout);
  
  // TODO: Change directory
  chdir(dir);
  // TODO: Update the PWD environment variable to be the new current working
  // directory and optionally update OLD_PWD environment variable to be the old
  // working directory.
  setenv("OLD_PWD", curDir, 1);
  setenv("PWD", dir, 1);
  fflush(stdout);
  free (curDir);
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
  int signal = cmd.sig;
  int job_id = cmd.job;

  // TODO: Remove warning silencers
  (void) signal; // Silence unused variable warning
  (void) job_id; // Silence unused variable warning

  // TODO: Kill all processes associated with a background job
  pidQ current_q;
  pid_t currentPID;
  Job j;

  for(int i = 0; i<length_jobQueue(&jq); i++){
    j = pop_front_jobQueue(&jq);
    if(j.jobId == job_id){
      current_q = *j.pidq;
      while(length_pidQ(&current_q) != 0){
        currentPID = pop_front_pidQ(&current_q);
        kill(currentPID, signal);
      }
      push_back_jobQueue(&jq, j);
    }
  }
}

// Prints the current working directory to stdout
void run_pwd() {
  // TODO: Print the current working directory
  // IMPLEMENT_ME();
  char * dir = get_current_directory(false);
  printf("%s\n", dir);
  free(dir);
  
  // Flush the buffer before returning
  fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {
  // TODO: Print background jobs

  Job current_job;
  printf("HERE\n");
  if(is_empty_jobQueue(&jq) == 0){
    printf("There are currently no jobs running\n");
    return;
  }
  printf("HERE1\n");
  for(int i = 0; i < length_jobQueue(&jq); i++){
    printf("LOOP ITEM\n");
    current_job = pop_front_jobQueue(&jq);
    print_job(current_job.jobId, peek_front_pidQ(current_job.pidq), current_job.command);
    push_back_jobQueue(&jq, current_job);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // TODO: Remove warning silencers
  // (void) p_in;  // Silence unused variable warning
  // (void) p_out; // Silence unused variable warning
  // (void) r_in;  // Silence unused variable warning
  // (void) r_out; // Silence unused variable warning
  // (void) r_app; // Silence unused variable warning

  // set the next pipe in the event that the
  // process has a pipe_output
  if (p_out){
    pipe(pipes[next_pipe]);
  }
  // write is 1
  // read is 0

  pid_t pid = fork();
  push_back_pidQ(&pidq, pid);

  if(pid == 0){
    //child process
    if(p_in){
      dup2(pipes[current_pipe][0], STDIN_FILENO);
      close(pipes[current_pipe][0]);
    }
    if(p_out){
      dup2(pipes[next_pipe][1], STDIN_FILENO);
      close(pipes[next_pipe][1]);
    }
    if(r_in){
      FILE* file = fopen(holder.redirect_out, "r");
      dup2(fileno(file), STDIN_FILENO);
      fclose(file);
    }
    if(r_out){
      if(r_app){
        FILE* file = fopen(holder.redirect_out, "a");
        dup2(fileno(file), STDOUT_FILENO);
        fclose(file);
      }
      else{
        FILE* file = fopen(holder.redirect_out, "w");
        dup2(fileno(file), STDOUT_FILENO);
        fclose(file);
      }
    }

    child_run_command(holder.cmd);
    exit(0);
  }
  else{
    //parent process
    if(p_out){
      close(pipes[next_pipe][1]);
    }
    
    parent_run_command(holder.cmd); // This should be done in the parent branch of
                                    // a fork

    //need to incriment the pipes
    current_pipe = (current_pipe + 1) % 2;
    next_pipe = (next_pipe + 1) % 2;
  }

  //parent_run_command(holder.cmd); // This should be done in the parent branch of
                                  // a fork
  //child_run_command(holder.cmd); // This should be done in the child branch of a fork
}

// Run a list of commands
void run_script(CommandHolder* holders) {
  if (holders == NULL)
    return;

  if (new_job){
     jq = new_jobQueue(0);
     new_job = false;
  }

  check_jobs_bg_status();

  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC) {
    end_main_loop();
    return;
  }
  CommandType type;
  pidq = new_pidQ(1);
  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i){
    create_process(holders[i]);
  }

  if (!(holders[0].flags & BACKGROUND)) {    
    // Not a background Job
    // TODO: Wait for all processes under the job to complete
    while(!is_empty_pidQ(&pidq)){
      pid_t tempPID = pop_front_pidQ(&pidq);
      int status;
      waitpid(tempPID, &status, 0);
    }
    destroy_pidQ(&pidq);
  }
  else {
    // A background job.
    // TODO: Push the new job to the job queue
    // TODO: Once jobs are implemented, uncomment and fill the following line.
    Job newJ;
    newJ.jobId = currentJID;
    currentJID++;
    newJ.pidq = &pidq;
    newJ.command = get_command_string();
    if(!is_empty_pidQ(&pidq)){
      newJ.jobId = peek_back_pidQ(&pidq);
    }
    else{
      fprintf(stderr, "No ID for the process.\n");
    }
    push_back_jobQueue(&jq, newJ);

    print_job_bg_start(newJ.jobId, peek_front_pidQ(newJ.pidq), newJ.command);
  }

}

