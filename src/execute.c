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
    pidQ pidq;
} Job;

IMPLEMENT_DEQUE_STRUCT(jobQueue, struct Job);
IMPLEMENT_DEQUE(jobQueue, struct Job);
jobQueue jq;
int currentJID = 1;
bool new_job = true;

// int current_pipe = 0;
// int next_pipe = 1;

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
  // Gets the current working directory.
  char* cwd = malloc(sizeof(char)*1024);
  *should_free = true;
  getcwd(cwd, 1024);
  return cwd;
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // Lookups environment variables.
  // (void) env_var; // Silence unused variable warning
  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status()
{
  // Checks on the statuses of all processes belonging to all background
  // jobs. This function removes jobs from the jobs queue once all
  // processes belonging to a job have completed.

  // print_job_bg_complete(job_id, pid, cmd);

  int numOfJob = length_jobQueue(&jq);

  // Iterate over the number of jobs
  for (int i = 0; i < numOfJob; i++)
  {
    // pops the next job in the queue
    Job currentJob = pop_front_jobQueue(&jq);
    int numOfPids = length_pidQ(&currentJob.pidq);
    pid_t atFront = peek_front_pidQ(&currentJob.pidq);
    // Iterates over each Pid at the front of the queue
    for (int j = 0; j < numOfPids; j++)
    {
      // Pop the next Pid from the front
      pid_t currentPid = pop_front_pidQ(&currentJob.pidq);
      int theStatus;
      // Wait for the process to complete
      if (waitpid(currentPid, &theStatus, 1) == 0)
        push_back_pidQ(&currentJob.pidq, currentPid);
    }
    // If all processes have completed 
    if (is_empty_pidQ(&currentJob.pidq))
    {
      // Print job complete
      print_job_bg_complete(currentJob.jobId, atFront, currentJob.command);
    }
    else
    {
      // Push the current job onto the queue
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

  (void) env_var; // Silence unused variable warning
  (void) val;     // Silence unused variable warning

  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  bool free_mem;
  free_mem = false;
  // Gets current working directory
  char* curDir = get_current_directory(&free_mem);
  char* dir = cmd.dir;
  // Clear the output buffer
  fflush(stdout);
  // Check if the directory is valid
  if (dir == NULL) {
    perror("ERROR: Failed to resolve path");
    return;
  }
  // Clear the output buffer
  fflush(stdout);
  
  // Changes directory
  chdir(dir);
  // Updates the PWD environment variable to be the new current working
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

  (void) signal; // Silence unused variable warning
  (void) job_id; // Silence unused variable warning

  // Kills all processes associated with a background job
  pid_t currentPID;
  Job j;
  // Iterates over all jobs in the job queue
  for(int i = 0; i<length_jobQueue(&jq); i++){
    // Gets next job from the front of queue
    j = pop_front_jobQueue(&jq);
    // If job id matches, then iterate over all the Pid's
    if(j.jobId == job_id){
      pidQ current_q = j.pidq;
      while(length_pidQ(&current_q) != 0){
        // Get the next Pid from the front of the queue
        currentPID = pop_front_pidQ(&current_q);
        kill(currentPID, signal);
      }
    }
    else{
      // Push back job onto the queue if the Id does not match
      push_back_jobQueue(&jq, j);
      // int len = length_jobQueue(&jq);
      // printf("PUSHBACK 3 : %i\n", len);
    }
  }
}

// Prints the current working directory to stdout
void run_pwd() {
  // Prints the current working directory
  bool free_mem;
  free_mem = false;
  // Gets current working directory
  char * dir = get_current_directory(&free_mem);
  // Prints the cwd to stdout
  printf("%s\n", dir);
  // Frees the memory allocated 
  free(dir);
  
  // Flush the buffer before returning
  fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {

  Job current_job;

  // int len = length_jobQueue(&jq);
  // printf("JB LENGTHS : %i\n", len);
  // Check if the job queue is empty
  if(is_empty_jobQueue(&jq)){
    printf("There are currently no jobs running\n");
    return;
  }
  // Iterate over all the jobs in the job queue
  for(int i = 0; i < length_jobQueue(&jq); i++){
    current_job = pop_front_jobQueue(&jq);
    // Print the job
    print_job(current_job.jobId, peek_front_pidQ(&current_job.pidq), current_job.command);
    // Push the job back to the job queue after printing 
    push_back_jobQueue(&jq, current_job);

    // int len = length_jobQueue(&jq);
    // printf("PUSHBACK 2 : %i\n", len);
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
void create_process(CommandHolder holder, int r) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // Remove warning silencers
  // (void) p_in;  // Silence unused variable warning
  // (void) p_out; // Silence unused variable warning
  // (void) r_in;  // Silence unused variable warning
  // (void) r_out; // Silence unused variable warning
  // (void) r_app; // Silence unused variable warning

  // set the next pipe in the event that the
  // process has a pipe_output
  int read = (r - 1) % 2;
  int write = r % 2;

  if (p_out){
    pipe(pipes[write]);
  }
  // write is 1
  // read is 0
  
  pid_t pid = fork();
  push_back_pidQ(&pidq, pid);

  if(pid == 0){
    //child process
    if(p_in){
      dup2(pipes[read][0], STDIN_FILENO);
      close(pipes[read][0]);
    }
    if(p_out){
      dup2(pipes[write][1], STDOUT_FILENO);
      close(pipes[write][1]);
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
      close(pipes[write][1]);
    }
    
    parent_run_command(holder.cmd); // This should be done in the parent branch of
                                    // a fork
  }
  //parent_run_command(holder.cmd); // This should be done in the parent branch of
                                  // a fork
  //child_run_command(holder.cmd); // This should be done in the child branch of a fork
}

// Run a list of commands
void run_script(CommandHolder* holders) {
  if (holders == NULL)
    return;

  // If new job, creates a new job queue
  if (new_job){
     jq = new_jobQueue(0);
     new_job = false;
  }

  // Checks status of background jobs
  check_jobs_bg_status();
  // Exit program is EXIT or EOC
  if (get_command_holder_type(holders[0]) == EXIT &&
    get_command_holder_type(holders[1]) == EOC) {
    end_main_loop();
    return;
  }
  // Create new Pid queue
  CommandType type;
  pidq = new_pidQ(0);
  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i){
    create_process(holders[i], i);
  }
  // If job is not a background job
  if (!(holders[0].flags & BACKGROUND)) {    
    // Not a background Job
    // Waits for all processes under the job to complete
    while(!is_empty_pidQ(&pidq)){
      pid_t tempPID = pop_front_pidQ(&pidq);
      int status;
      waitpid(tempPID, &status, 0);
    }
    destroy_pidQ(&pidq); // frees memory
  }
  else {
    // A background job.
    // Pushes the new job to the job queue
    Job newJ;
    newJ.jobId = currentJID; // assign a new job id
    currentJID++;
    newJ.pidq = pidq;
    newJ.command = get_command_string();
    // If no pids in the queue, set the job id to the last Pid
    if(!is_empty_pidQ(&pidq)){
      newJ.jobId = peek_back_pidQ(&pidq);
    }
    else{
      // If no Pid, then print error
      fprintf(stderr, "No ID for the process.\n");
    }
     
    push_back_jobQueue(&jq, newJ); // Push job onto the job queue

    // int len = length_jobQueue(&jq);
    // printf("PUSHBACK 1 : %i\n", len);
    // Print that job started
    print_job_bg_start(newJ.jobId, peek_front_pidQ(&newJ.pidq), newJ.command);
  }
}

