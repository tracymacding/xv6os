#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);
void   runcmd(struct cmd*);

char*
mkcmdpath(char* dir, char* file) 
{
  char* cmdpath = NULL;
  cmdpath = malloc(strlen(dir) + strlen(file) + 1);
  assert(cmdpath);
  strncpy(cmdpath, dir, strlen(dir));
  strncat(cmdpath, file, strlen(file));
  return cmdpath;
}

char*
cmdfullpath(char *oricmdpath)
{
  char* PATH1 = "/bin/";
  char* PATH2 = "/usr/bin/";
  char* cmdpath = NULL;

  if(oricmdpath[0] == '/') {
	return oricmdpath;
  }

  cmdpath = mkcmdpath(PATH1, oricmdpath);
  if(access(cmdpath, F_OK) != 0) {
  	cmdpath = mkcmdpath(PATH2, oricmdpath);
  }
  return cmdpath;
}

char*
cmdcontent(char* cmdpath, char* cmdargv[])
{
  int i = 0;
  char* cmdcontent = NULL;
  int contentsize = strlen(cmdpath) + 1;
  for (i = 0; cmdargv[i] != NULL; i++) {
  	contentsize += strlen(cmdargv[i] + 1);
  }
  cmdcontent = malloc(contentsize);
  assert(cmdcontent);
  memset(cmdcontent, 0, contentsize);
  strncpy(cmdcontent, cmdpath, strlen(cmdpath));
  strncat(cmdcontent, " ", 1);

  for (i = 0; cmdargv[i] != NULL; i++) {
  	strncat(cmdcontent, cmdargv[i], strlen(cmdargv[i]));
  	strncat(cmdcontent, " ", 1);
  }
  return cmdcontent;
}

void
execsyscmd(struct execcmd* ecmd)
{
  int ret = 0;
  char* cmdpath = cmdfullpath(ecmd->argv[0]);
  char** cmdargv = ecmd->argv + 1;
  char* cmd = cmdcontent(cmdpath, cmdargv);

  // when execv("ls", "-al"), can't show file detail info
  // so use system()
  // ret = execv(cmdpath, cmdargv);
  ret = system(cmd);
  if (ret < 0) {
   	fprintf(stderr, "%s error: %s\n", cmdpath, strerror(errno));
  }
}

void
execmdpipe(struct cmd *first, struct cmd *second) 
{
  int fd[2];
  pipe(fd);

  // child process
  if(fork() == 0) {
	close(fd[0]);
    if(dup2(fd[1], STDOUT_FILENO) < 0 ) {
      fprintf(stderr, "dup2 error:%s\n", strerror(errno));
	  exit(-1);
	}
    runcmd(first);
  } else {
	close(fd[1]);
	if(dup2(fd[0], STDIN_FILENO) < 0 ) {
      fprintf(stderr, "dup2 error:%s\n", strerror(errno));
	  exit(-1);
	}
    runcmd(second);
  }
}

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2], r;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);

	execsyscmd(ecmd);
    break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
	// open file: rcmd->file
	// redirect rcmd->fd to file
	int newfd = open(rcmd->file, rcmd->mode);
	if(newfd < 0) {
    	fprintf(stderr, "open %s error:%s\n", rcmd->file, strerror(errno));
		return;
	}
	if(dup2(newfd, rcmd->fd) < 0 ) {
    	fprintf(stderr, "dup2 error:%s\n", strerror(errno));
		return;
	}
    runcmd(rcmd->cmd);
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
	execmdpipe(pcmd->left, pcmd->right);
    break;
  }    
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  
  if (isatty(fileno(stdin)))
    fprintf(stdout, "6.828$ ");
  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd, r;

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
	// exit if cmd contains "exit"
	if(strncmp(buf, "exit", 4) == 0) {
		exit(0);
	}

    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(&r);
  }
  exit(0);
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
