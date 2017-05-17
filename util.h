
#define TRUE 1
#define FALSE !TRUE

// Shell pid, pgid, terminal modes
static pid_t GBSH_PID;
static pid_t GBSH_PGID;
static int GBSH_IS_INTERACTIVE;
static struct termios GBSH_TMODES;

static char* currentDirectory;
extern char** environ;

struct sigaction act_child;
struct sigaction act_int;
struct sigaction act_tstp;

int no_reprint_prmpt;

typedef struct child_process{
	pid_t pid;
	int background;
	char prompt[1024];
	struct child_process *next;
	int state; // 1 Running, 0 Stopped 
}CP;

pid_t pid;


/**
 * SIGNAL HANDLERS
 */
// signal handler for SIGCHLD
void signalHandler_child(int p, siginfo_t* info, void* vp);
// signal handler for SIGINT
void signalHandler_int(int p);
// signal handler for SIGTSTP
void signalHandler_tstp(int p);

int changeDirectory(char * args[]);
void addChilProcessToList(int background, pid_t pid);
void removeChildProcessByPID(pid_t pid);
void setChildProcessStateByPID(pid_t pid, int state);
