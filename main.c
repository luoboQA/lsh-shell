#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>   

#define LSH_RL_BUFSIZE 1024 //读取行的缓冲区大小
#define LSH_TOK_BUFSIZE 64  //// 初始指针数组大小：可以存64个参数
#define LSH_TOK_DELIM " \t\r\n\a" // 分隔符：空格、制表符、回车、换行、响铃

void lsh_loop(void);
char *lsh_read_line(void);
char **lsh_split_line(char *line);
int lsh_execute(char **args);
int lsh_launch(char **args);
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_pwd(char **args);


int main(int argc, char **argv){

    //运行命令循环
    lsh_loop();
    
    //执行清理
    return EXIT_SUCCESS;
}

void lsh_loop(void){
    char *line;     //存储用户输入的原始命令行字符串
    char **args;    //存储解析后的命令和参数的数组
    int status;     //命令执行状态，控制循环是否继续    

    do {
        printf("> ");  //终端提示符
        line = lsh_read_line(); //读取用户输入的命令行
        args = lsh_split_line(line); //解析命令行字符串成命令和参数的数组
        status = lsh_execute(args);  //执行命令并获取状态

        free(line); //释放读取的命令行字符串内存
        free(args); //释放解析后的命令和参数数组内存
    } while (status);
}


char *lsh_read_line(void){
    int bufsize = LSH_RL_BUFSIZE; //buffer初始大小
    int position = 0; //当前输入位置
    char *buffer = malloc(sizeof(char) * bufsize); //分配输入缓冲区
    int c;  //读取的字符

    if (!buffer) { //malloc返回null，表示分配失败
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        //读取一个字符
        c = getchar();

        //如果我们到达了行的末尾，或者遇到了 EOF，返回我们已经读入的字符串
        if (c == EOF || c == '\n') {
            buffer[position] = '\0'; //在字符串末尾添加 null 终止符
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        //如果我们超过了缓冲区，重新分配。
        if (position >= bufsize) {
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}


char **lsh_split_line(char *line){
    int bufsize = LSH_TOK_BUFSIZE, position = 0; // 当前指针数组大小和位置
    char **tokens = malloc(bufsize * sizeof(char*)); // 分配指针数组
    char *token;// 临时存储每个分割出的token

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM); // 第一次调用strtok，strtok()的工作原理是将输入字符串按照分隔符分割成一个个token，每次调用返回下一个token，直到没有更多token可返回时返回NULL,找到第一个token "ls"，将后面的空格变成\0,input:"ls -l /home",output:"ls\0-l /home"
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);//从第二次调用开始，strtok()会继续从上一次结束的位置继续分割，stroke内部静态指针记录上次结束位置，直到返回NULL
    }
    tokens[position] = NULL;
    return tokens;
}

int lsh_launch(char **args){
    pid_t pid, wpid; // pid: 进程ID, wpid: waitpid的返回值
    int status; // 存储子进程的退出状态

    pid = fork(); //创建一个新进程，返回值为0表示子进程，返回值为正数表示父进程，返回值为负数表示错误
    if (pid == 0) {
        //子进程，execvp 是exec函数族的一员，用新程序替换当前进程的映像
        if (execvp(args[0], args) == -1) {
            perror("lsh");// 如果exec失败，打印错误
        }
        exit(EXIT_FAILURE);// 如果exec失败，退出子进程
    } else if (pid < 0) {
        //错误
        perror("lsh");
    } else {
        //父进程，pid是要等待的子进程ID，&status: 存储子进程状态信息的指针，WUNTRACED: 控制返回条件
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }             //子进程还没结束        //子进程没被信号杀死  

    return 1;
}

char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "pwd"  // 新增
};

int (*builtin_func[]) (char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_pwd  // 新增
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);//动态计算内建命令的数量
}

int lsh_cd(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}


int lsh_help(char **args){
    int i;
    printf("Stephen Brennan's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;       
}

int lsh_exit(char **args){
    return 0;
}

//寻找内建命令并执行，非内建命令则调用lsh_launch执行外部命令
int lsh_execute(char **args){
    int i;

    if (args[0] == NULL) {
        //空命令
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

int lsh_pwd(char **args) {
    char cwd[1024];  // 缓冲区存储路径
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("lsh_Pwd_%s\n", cwd);  // 打印当前目录
    } else {
        perror("lsh");  // getcwd失败（比如权限问题）
    }
    
    return 1;  // pwd不会退出shell
}