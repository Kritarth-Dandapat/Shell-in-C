#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_history(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "history"};

int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_history};

int lsh_num_builtins()
{
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

/**
   @brief Builtin command: display history of all commands used.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_history(char **args)
{
  FILE *fp;
  char *line = NULL;
  size_t len = 0;

  // If an argument is provided
  if (args[1] != NULL)
  {
    // Check if the argument is "-c" (case-insensitive)
    if (strcasecmp(args[1], "-c") == 0)
    {
      // Attempt to remove the history file
      if (remove("history.txt") != 0)
      {
        perror("Error clearing history");
      }
      else
      {
        printf("History cleared successfully.\n");
      }
      return 1;
    }
    else
    {
      fprintf(stderr, "Invalid option: %s\n", args[1]);
      fprintf(stderr, "Usage: history [-c]\n");
      return 1;
    }
  }

  // Default behavior: display history
  printf("History of commands used:\n");
  fp = fopen("history.txt", "r");

  if (fp == NULL)
  {
    fprintf(stderr, "No history found.\n");
    return 1;
  }

  while (getline(&line, &len, fp) != -1)
  {
    printf("%s", line);
  }

  fclose(fp);
  if (line)
    free(line);
  return 1;
}

/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
  if (args[1] == NULL)
  {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  }
  else
  {
    // Change the current working directory to the specified path.
    // If the directory does not exist, an error message is printed.
    if (chdir(args[1]) != 0) // system call to change directory
    {
      perror("lsh");
    }
  }
  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_help(char **args)
{
  int i;
  printf("Kritarth Dande's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++)
  {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int lsh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  /**
   * fork() creates a new process by duplicating the calling process.
   * The new process, known as the child, is an exact copy of the parent process
   * except for the returned value. In the child process, fork() returns 0,
   * while in the parent process, it returns the child's process ID.
   * This allows both processes to execute concurrently.
   */
  pid = fork();
  if (pid == 0)
  {
    // Child process
    /**
     * Executes a program, replacing the current process image.
     */
    if (execvp(args[0], args) == -1)
    {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  }
  else if (pid < 0)
  {
    // Error forking
    perror("lsh");
  }
  else
  {
    // Parent process
    do
    {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL)
  {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++)
  {
    if (strcmp(args[0], builtin_str[i]) == 0)
    {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

#define LSH_RL_BUFSIZE 1024
/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer)
  {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1)
  {
    // Read a character
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n')
    {
      buffer[position] = '\0';
      return buffer;
    }
    else
    {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize)
    {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer)
      {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char *));
  char *token;

  if (!tokens)
  {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL)
  {
    tokens[position] = token;
    position++;

    if (position >= bufsize)
    {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char *));
      if (!tokens)
      {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do
  {
    printf("> ");
    line = lsh_read_line();

    if (line[0] != '\0') // Only write non-empty lines
    {
      FILE *fp = fopen("history.txt", "a");
      if (fp != NULL)
      {
        // Get the current working directory
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
          perror("getcwd error");
          strncpy(cwd, "unknown", sizeof(cwd)); // Fallback if getcwd fails
        }

        // Get the current timestamp
        time_t now = time(NULL);
        char timestamp[20]; // Format: YYYY-MM-DD HH:MM:SS
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

        // Write the enhanced details to the file
        fprintf(fp, "[%s] [%s] %s\n", timestamp, cwd, line);
        fclose(fp);
      }
    }
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  lsh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}
