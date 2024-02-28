#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */
typedef struct bookmarkListNode{
    char *executableName ;
    struct bookmarkListNode *next;
}BookmarkListNode;

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */


} /* end of setup routine */

void foregroundProcess(char *path,char *args[]);
void backgroundProcess(char *path,char *args[]);
void stopRunningForegroundProcessAndItsChildren();
void insertNewBookmark(BookmarkListNode **root,BookmarkListNode **tail,char *executableName);
void printBookmarkList(BookmarkListNode **root);
void bookmarkCommandEntered(char *args[]);
void executeBookmarkIndex(BookmarkListNode **root,int index);
void deleteBookmarkAtIndex(BookmarkListNode **root, BookmarkListNode **tail, int deletedIndex);
void handleSIGCHLD(int signo);
void exitCommandEntered(char *args[]);
void exitCommand();
int checkIOOperations(char *args[]);
void handleIOOperations(char *args[]);

int foregroundPid;
int parentPid;
int backgroundGroupPid = 0;



BookmarkListNode *bookmarkRoot;
BookmarkListNode *bookmarkTail;
int main(void)
{
    bookmarkRoot=NULL;
    bookmarkTail=NULL;
    //bookmarkRoot = (BookmarkListNode*)malloc(sizeof (BookmarkListNode));
    //bookmarkTail = (BookmarkListNode*)malloc(sizeof (BookmarkListNode));



    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */

    signal(SIGTSTP,&stopRunningForegroundProcessAndItsChildren);
    signal(SIGCHLD, handleSIGCHLD); //waits for the background childs so that they are terminated successfully



    parentPid = getpid();
    while (1){
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);

        /** the steps are:
        (1) fork a child process using fork()
        (2) the child process will invoke execv()
        (3) if background == 0, the parent will wait,
        otherwise it will invoke the setup() function again. */

        char *path = (char*)malloc(10);


        strcat(path, "/bin/");
        strcat(path, args[0]);
        strcat(path, "\0");

        //printf("my path is %s",path);
        if(strcmp(args[0],"bookmark")==0){
            bookmarkCommandEntered(args);
            goto L;

        }else if(strcmp(args[0], "exit")==0){
            exitCommandEntered(args);
            goto L;
        }

        if(background==0){//foreground process
            foregroundProcess(path,args);
        }else if(background==1){//background process
            backgroundProcess(path,args);
            printf("background group id: %d\n", backgroundGroupPid );

        }

        //Fork a child process
        L:

    }//While(1) end

}

void exitCommandEntered(char *args[]){
    if(args[1] == NULL){
        exitCommand();
    } else{
        printf("Usage: only \"exit\" ");
    }
}

void exitCommand(){
    pid_t wpid;
    int status;
    wpid = waitpid(-backgroundGroupPid, &status, WNOHANG);



    if (wpid == -1) {
        exit(1);
    }else{
        printf("there are background process running.\n");
    }

}



void bookmarkCommandEntered(char *args[]){
    if(strcmp(args[1],"-l")==0){
        printBookmarkList(&bookmarkRoot);
    }else if(strcmp(args[1],"-i")==0){
        executeBookmarkIndex(&bookmarkRoot,atoi(args[2]));
    }
    else if(strcmp(args[1],"-d")==0){
        deleteBookmarkAtIndex(&bookmarkRoot,&bookmarkTail,atoi(args[2]));
    }
    else{
        char *executableName = (char*) malloc (100);
        //args[1] and concatenated with later string
        int i=1;
        while(args[i]!=NULL){
            executableName = strcat(executableName,args[i]);
            executableName = strcat(executableName," ");
            i++;
        }

        insertNewBookmark(&bookmarkRoot,&bookmarkTail,executableName);
        //printBookmarkList(&bookmarkRoot);
    }


}

void executeBookmarkIndex(BookmarkListNode **root,int index){
    BookmarkListNode *iterator = *root;
    if(iterator==NULL){
        printf("List of executables are empty!\n");
        return;
    }

    int i;
    for(i=0;i<index;i++){
        iterator=iterator->next;
        if(iterator==NULL){
            printf("Invalid index!\n");
            return;
        }
    }
    system(iterator->executableName);

}

void printBookmarkList(BookmarkListNode **root){
    BookmarkListNode *iterator = *root;

    if(iterator==NULL){
        printf("List is empty!\n");
    }
    int i = 0;
    while(iterator != NULL){
        printf("index : %d , executable : %s \n",i,iterator->executableName);
        i++;
        iterator = iterator->next;
    }
}

void insertNewBookmark(BookmarkListNode **root, BookmarkListNode **tail, char *executableName) {
    BookmarkListNode *newNodePtr = (BookmarkListNode *)malloc(sizeof(BookmarkListNode));
    if (newNodePtr == NULL) {
        fprintf(stderr, "%s", "Memory allocation failed.\n");
        return;
    }

    newNodePtr->executableName = (char *)malloc(strlen(executableName) + 1);
    if (newNodePtr->executableName == NULL) {
        fprintf(stderr, "%s", "Memory allocation failed.\n");
        free(newNodePtr);
        return;
    }
    strcpy(newNodePtr->executableName, executableName);

    newNodePtr->next = NULL;

    if (*tail != NULL) {
        (*tail)->next = newNodePtr;
        *tail = newNodePtr;
    } else if (*root == NULL) {
        *root = newNodePtr;
        *tail = newNodePtr;
    } else {
        fprintf(stderr, "%s", "Error occurred while inserting the bookmark.\n");
        free(newNodePtr->executableName);
        free(newNodePtr);
    }
}

void deleteBookmarkAtIndex(BookmarkListNode **root, BookmarkListNode **tail, int deletedIndex) {
    BookmarkListNode *current = *root;
    BookmarkListNode *prev = NULL;
    int currentIndex = 0;

    // If list is empty
    if (current == NULL) {
        fprintf(stderr, "List is empty. Cannot delete.\n");
        return;
    }

    // Handle deletion at the head of the list
    if (deletedIndex == 0) {
        *root = current->next;

        // Update the tail if the deleted node is the only node in the list
        if (*root == NULL) {
            *tail = NULL;
        }

        free(current->executableName);
        free(current);
        return;
    }

    // Find the node to delete
    while (current != NULL && currentIndex != deletedIndex) {
        prev = current;
        current = current->next;
        currentIndex++;
    }

    // Node not found at the specified index
    if (current == NULL) {
        fprintf(stderr, "Index out of range. Cannot delete.\n");
        return;
    }

    // Perform deletion
    prev->next = current->next;

    // Update the tail pointer if the last node is deleted
    if (current->next == NULL) {
        *tail = prev;
    }

    free(current->executableName);
    free(current);
}


void handleIOOperations(char *args[]){

    char *executableName = (char*) malloc (100);
    //args[1] and concatenated with later string
    int i=0;
    while(args[i]!=NULL){
        executableName = strcat(executableName,args[i]);
        executableName = strcat(executableName," ");
        i++;
    }

    system(executableName);
}

//Return 1 if the args exists > or <
int checkIOOperations(char *args[]){
    int i = 0 ;

    while(args[i] != NULL){
        if(!strcmp(args[i],">") || !strcmp(args[i],"<") || !strcmp(args[i],">>") || !strcmp(args[i],"2>")){
            return 1;
        }
        i++;
    }
    return 0;
}

void foregroundProcess(char *path,char *args[]){
    pid_t pid;
    pid = fork();
    if(pid<0){//Error occured
        fprintf(stderr,"Fork Failed");
        return 1;
    }

    else if(pid==0){//Child Process
    //    int i = 0;
    //    while(args[i] != NULL){
    //        printf("%s ",args[i]);
    //        i++;
    //    }
        if(checkIOOperations(args)){
            printf("IO tarafı foreground\n");
            handleIOOperations(args);
        }else{
            printf("IO olmayan taraf foreground\n");

            execv(path,args);
        }

    }
    else{//Parent process
        foregroundPid = pid;
        setpgid(pid,pid);
        waitpid(foregroundPid, NULL,WUNTRACED);
        printf("Child Completed\n", foregroundPid);
    }
}

void backgroundProcess(char *path,char *args[]){
    printf("\nbackground\n");
    int i;
    for(i=0; i < 32; i ++){
        if(args[i]==NULL) break;

        if(strcmp(args[i],"&")==0){
            args[i]=NULL;
        }
    }

    pid_t pid;
    pid = fork();
    if(pid<0){//Error occured
        fprintf(stderr,"Fork Failed");
        return 1;
    }
    else if(pid==0){//Child Process

        if(checkIOOperations(args)){
            printf("IO tarafı background\n");
            handleIOOperations(args);
        }else{
            printf("IO olmayan taraf background\n");

            execv(path,args);
        }

        fflush(stdout);
        printf("\nmyshell:");
    }
    else{ //parent
        if(backgroundGroupPid == 0){ // if there is no previous background process created then create a new process group for background processes
            setpgid(pid,pid);
            backgroundGroupPid = getpgid(pid);
        }else{ // if already a background process group created before then assign new background process to this group
            setpgid(pid,backgroundGroupPid);
        }
    }
}

void stopRunningForegroundProcessAndItsChildren(){


    if(foregroundPid == 0 || waitpid(foregroundPid,NULL,WNOHANG) == -1){
        fflush(stdout);
        printf("\nmyshell: ");
        killpg(backgroundGroupPid,SIGCONT);
        return;
    }
    kill(foregroundPid, SIGKILL);
    foregroundPid = 0;
    killpg(backgroundGroupPid,SIGCONT);

}

void handleSIGCHLD(int signo) {
    (void) signo;
    int status;
    pid_t child_pid;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {

    }
}