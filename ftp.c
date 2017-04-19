/*************************************************************************
	> File Name: ftp.c
	> Author: lfy
	> Mail: lingfuyi@126.com
	> Created Time: 2017年04月13日 星期四 17时03分00秒
 ************************************************************************/
#include<stdio.h>  
#include<sys/socket.h>  
#include<stdlib.h>  
#include<string.h>  
#include<strings.h>  
#include<unistd.h>  
#include<netinet/in.h>  
#include<netdb.h>  
#include<errno.h>  
#include<sys/types.h>  
#include<sys/stat.h>  
#include<fcntl.h> 
#include <netinet/in.h>
#include<arpa/inet.h> 
#include <sys/select.h>
char serverip[30];
int  serverport;
int  dataport=0;
char username[30]={0};
char keyword[30]={0};
char opcodeA[50]={0};
char opcodeB[50]={0};
char dstdir[50]={0};			//目标目录

char currentdir[50]={0};
int socket_fd ; 
int socket_datfd;

enum ReplyCode
{
	LOGINSUCCEED,
	LOGINFAILD
};
enum RecOlder
{
	OLDER_HELP,
	OLDER_CD,
	OLDER_PWD,
	OLDER_GET,
	OLDER_PUT,
	OLDER_EXIT,
	OLDER_LS,
	OLDER_RM,
	OLDER_MKD,
	OLDER_MV,
	OLDER_NULL,
	OLDER_INVALID
};


int my_gets(char* buf, int size)  
{  
    int c;  
    int len = 0;  
    while((c = getchar()) != EOF && len < size)
	{
		if('\n' == c)
			break;
        buf[len++] = c;  
    }  
    buf[len] = '\0';  
    return len;  
}  
/*

验证服务器IP格式是否合法
*/

static int check_ip(char *ip , char *port)
{
	 struct in_addr addr;  
    int ret;  
    volatile int local_errno;  
    errno = 0;  
    ret = inet_pton(AF_INET, ip, &addr);  
    local_errno = errno;  
    if (ret > 0)  
    {
		strcpy(serverip, ip);
		serverport = atoi(port);
		fprintf(stderr, "\"%s\" IP正确,端口%d,正在发起连接...\n", serverip,serverport);  
    }
    else if (ret < 0)  
        fprintf(stderr, "EAFNOSUPPORT: %s\n", strerror(local_errno));  
    else   
        fprintf(stderr, "\"%s\" 不是合法的IP地址\n", ip);  
    fflush(stdout);  
    return ret;  
}

static int get_serverip(int argc , char **argv)
{
	int ret;
	if(argc<2)		// ./ftp
		ret = check_ip("127.0.0.1" , "21");
	else if(argc<3)		// ./ftp 192.168.1.1
		ret = check_ip(argv[1] , "21");
	else
		ret = check_ip(argv[1] , argv[2]);

	return ret;
}
/*

显示帮助菜单
*/
static void ftp_help(void)
{
	printf("\n/------------------------------------------/\n");
	printf("FTP   Client\nAuthor:凌福义\nemail:lingfuyi@126.com\n");
	printf("\n基础命令:	\n");
	printf("ls:显示当前工作目录的所有文件  \n");
	printf("pwd:显示当前工作目录\n");
	printf("cd:切换工作目录\n");
	printf("get:复制服务器文件到本地\n");
	printf("put:复制本地文件到服务器\n");
	printf("rm:删除文件\n");
	printf("mv:重命名文件\n");
	printf("mkdir:创建目录\n");
	printf("quit/exit:退出\n");
	printf("\n/------------------------------------------/\n");
	fflush(stdout);  

}
/*

打开一个TCP连接
*/
static int open_tcpclient(char *host , int port)
{
	struct sockaddr_in servaddr;
	int control_sock;
	struct hostent *ht=NULL;
	control_sock = socket(AF_INET , SOCK_STREAM , 0);
	if(control_sock < 0)
	{
		printf("\nsocket error\n");
		return -1;
	}
	ht = gethostbyname(host);
    if(!ht)
    { 
		printf("\nhost error\n");
        return -1;
    }
	memset(&servaddr,0,sizeof(struct sockaddr_in));
    memcpy(&servaddr.sin_addr.s_addr,ht->h_addr,ht->h_length);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
	if(connect(control_sock , (struct sockaddr*)&servaddr , sizeof(struct sockaddr)) == -1)
	{
		printf("\n连接失败!\n");
		return -1;
	}
	else
	{	
		return control_sock;
	}
}

static void get_data_port(char *rev)
{
	char dath[5],datl[5];
	int i=0,j=0,pos=0;
	while(*rev)
	{
		if(*rev == ',')
		{
			pos++;
		}
		if(4 == pos && *rev!=',')		
			dath[i++] = *rev;		
		if(5 == pos && *rev!=',')
		{
			if(*rev == ')')
				break;
			datl[j++] = *rev;
		}
		rev++;
	}
	dath[i] = '\0';
	datl[j] = '\0';
	dataport = atoi(dath)*256 + atoi(datl);
}

static int ftp_pasv(void)
{
	int len;
	char sendbuf[50]={0};
	char readbuf[50]={0};
	if(write(socket_fd,"PASV\r\n",strlen("PASV\r\n")) != strlen("PASV\r\n"))
	{
		printf("PASV Mode Err\n");
		fflush(stdout);  
		 return -1;
	}
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"227",3))		//pasv mode succeed
	{
		printf("PASV Mode Err\n");
		fflush(stdout);  
		return -1;

	}
	get_data_port(readbuf);
	socket_datfd= open_tcpclient(serverip, dataport);
	return 0;
}

static void ftp_exit(void)
{
	char readbuf[50]={0};
	write(socket_fd,"QUIT\r\n",strlen("QUIT\r\n")) != strlen("QUIT\r\n");
	printf("Good Bye.....^_^\n");
	fflush(stdout); 
	close(socket_fd);
	close(socket_datfd);
}
static void ftp_rm(void)
{
	int len;
	char readbuf[50]={0};
	char sendbuf[50]={0};
	sprintf(sendbuf,"RMD %s\r\n",opcodeA);   
	write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf);
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"550",3)==0) 
	{
		sprintf(sendbuf,"DELE %s\r\n",opcodeA);   
		write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf);
		memset(readbuf , 0 , sizeof(readbuf));
		len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
		if(strncmp(readbuf,"550",3)==0) 
		{
			printf("删除出错:550\n");
			fflush(stdout); 
			return;
		}
		else if(strncmp(readbuf,"250",3)==0)
		{
			printf("删除文件成功\n");
			fflush(stdout); 
			return;
		}
		else
			printf("%s",readbuf);
			return;
	}
	else if(strncmp(readbuf,"250",3)==0)
	{
		printf("删除文件夹成功\n");
		fflush(stdout); 
		return;
	}
	else
	{
		printf("rm=%s",readbuf);
		fflush(stdout); 
	}
}
static void ftp_pwd(void)
{
	int i=0;
	int len;
	char readbuf[50]={0};
	if(write(socket_fd,"PWD\r\n",strlen("PWD\r\n")) != strlen("PWD\r\n"))
	{
		 return;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"257",3)==0)  
	{
		char *ptr=readbuf+5;
		while(*ptr != '"')
			currentdir[i++]=*ptr++;
		currentdir[i]='\0';
		printf("%s\n",currentdir);
		fflush(stdout); 
	}
}
static void ftp_mkd(void)
{
	int len;
	char readbuf[50]={0};
	char sendbuf[50]={0};
	sprintf(sendbuf,"MKD %s\r\n",opcodeA);              
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		 return;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"257",3))  
	{
		printf("目录创建失败 550\n");
	}
	else		
		printf("目录创建成功\n");
	fflush(stdout); 
}

static void ftp_mv(void)
{
	int len;
	char readbuf[50]={0};
	char sendbuf[50]={0};
	sprintf(sendbuf,"RNFR %s\r\n",opcodeA);              
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		 return;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"350",3))  
	{
		printf("没有该文件\n");
		fflush(stdout); 
		return;
	}
	sprintf(sendbuf,"RNTO %s\r\n",opcodeB);              
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		 return;
	}
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"250",3))  
	{
		printf("重命名失败\n");
		fflush(stdout); 
		return;
	}
	printf("重命名成功\n");
	fflush(stdout); 
}
static void ftp_cd(void)
{
	int len;
	char readbuf[50]={0};
	char sendbuf[50]={0};
	sprintf(sendbuf,"CWD %s\r\n",opcodeA);              
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		 return;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"250",3))  
	{
		printf("目录错误\n");
		fflush(stdout); 
	}
	else
	{
		strcpy(currentdir , opcodeA);
	}
}

static void ftp_type(char type)
{
	int len;
	char readbuf[1024]={0};
	char sendbuf[50]={0};
	sprintf(sendbuf,"TYPE %c\r\n",type);  
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		 return;
	}
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
}
static void ftp_get(void)
{
	int len;
	FILE *fd=NULL;
	char readbuf[1024]={0};
	char sendbuf[50]={0};
	ftp_pasv();
	ftp_type('I');
	sprintf(sendbuf,"RETR %s\r\n",opcodeA); 
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		printf("远端文件不存在\n");
		fflush(stdout);
		return;
	}
waitrecive:
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"550",3)==0)
	{
		printf("%s",readbuf);		
		fflush(stdout);
		close(socket_datfd);
		return;
	}
	if(strncmp(readbuf,"226",3)==0)
	{		
		//printf("dir=%s\n",dstdir);
		//fflush(stdout);
		fd = fopen(dstdir, "w+");
		if(fd == NULL)
		{
			printf("本地文件创建失败\n");		
			fflush(stdout);
			close(socket_datfd);
			return;
		}
		while(len = recv(socket_datfd , readbuf , sizeof(readbuf) , 0)>0)
		{
			fwrite(readbuf,1,strlen(readbuf),fd);
			memset(readbuf , 0 , sizeof(readbuf));
		}
	}
	else
		goto waitrecive;
	fclose(fd);
	close(socket_datfd);}

static void ftp_put(void)
{
	int len , error;
	FILE *fd=NULL;
	char readbuf[50]={0};
	char sendbuf[1024]={0};
	ftp_pasv();
	ftp_type('I');
	sprintf(sendbuf,"STOR %s\r\n",dstdir); 
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		printf("远端文件不存在\n");
		fflush(stdout);
		return;
	}
	fd = fopen(opcodeA , "r");
	if(fd == NULL)
	{
		printf("本地文件打开失败\n");		
		fflush(stdout);
		close(socket_datfd);
		return;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	while(!feof(fd))
	{
		memset(sendbuf , 0 , sizeof(sendbuf));
		len = fread(sendbuf , 1 , sizeof(sendbuf) , fd);
		error=ferror(fd);
		if(error)
        {
            printf("本地文件读取错误: %d\n",error);	
			fflush(stdout);
			close(socket_datfd);
			return;
        }
        else if(write(socket_datfd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
        {			
			printf("传输错误\n");	
			fflush(stdout);
			close(socket_datfd);
			return;
        }
		memset(readbuf , 0 , sizeof(readbuf));
	}
	close(socket_datfd);

	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"226",3)==0)
	{
		printf("传输完成\n");	
		fflush(stdout);
		return;
	}

}
static void ftp_ls(void)
{
	int len;
	char readbuf[1024]={0};
	char sendbuf[50]={0};
	ftp_pasv();
	ftp_type('I');

	sprintf(sendbuf,"LIST %s\r\n",opcodeA);  
	if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
	{
		printf("err");
		fflush(stdout);
		return;
	}
waitrecive:
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"226",3)==0)
	{
		while(len = recv(socket_datfd , readbuf , sizeof(readbuf) , 0)>0)
		{
			printf("%s",readbuf);		
			fflush(stdout);
			memset(readbuf , 0 , sizeof(readbuf));
		}
	}
	else
		goto waitrecive;
	close(socket_datfd);
}

/*获取几个操作码*/
static int get_opcode(char *older ,int number)
{
	int i=0 , j=0;
	memset(opcodeA , 0 , sizeof(opcodeA));
	while(*older != ' '&&*older)
		older++;
	while(*older == ' '&&*older)
		older++;
	while(*older != ' ' &&*older)
		opcodeA[i++] = *older++;
	opcodeA[i]='\0';
	if(0 == strlen(opcodeA))
	{
		strcpy(opcodeA , "./");
	}
	if(2 == number)
	{
		while(*older != ' '&&*older)
			older++;
		while(*older == ' '&&*older)
			older++;
		while(*older != ' ' &&*older)
			opcodeB[j++] = *older++;
		opcodeB[j]='\0';
		if(0 == strlen(opcodeB))
		{
			strcpy(opcodeB , "./");
		}
	}
	//printf("a=%s b=%s\n",opcodeA,opcodeB);
	//fflush(stdout);

}

/*
组件新的目录项
get /data/a.txt ./
则src=/dtat/a.txt dst=./
新的目录项为./a.txt
*/
static int get_dstdir(char *src , char *dst)
{
	int i=0;
	char buf[50];
	while(*src)
	{
		buf[i++]=*src;
		if(*src == '/')
		{
			i=0;
		}
		src++;
	}
	buf[i]='\0';
	if(dst[strlen(dst)-1]!='/')
		sprintf(dstdir , "%s/%s" , dst , buf);
	else
		sprintf(dstdir , "%s%s" , dst , buf);
	//printf("dir=%s\n",dstdir);
	//fflush(stdout);
}

static int analysis_older(char *older)
{
	while(*older == ' ')
		older++;
	if(0 == strcmp("quit" , older) || 0 == strcmp("exit" , older))
		return OLDER_EXIT;
	else if(0 == strcmp("help" , older))
		return OLDER_HELP;
	else if(0 == strcmp("pwd" , older))
		return OLDER_PWD;
	else if(0 == strncmp("cd" , older , 2))
	{
		get_opcode(older ,1);
		return OLDER_CD;
	}
	else if(0 == strncmp("get" , older , 3))
	{
		get_opcode(older ,2);
		if(access(opcodeB, W_OK))
		{
			printf("本地路径不存在或不可写\n");
			fflush(stdout);
			return OLDER_NULL;
		}
		get_dstdir(opcodeA , opcodeB);
		
		return OLDER_GET;
	}
	else if(0 == strncmp("put" , older , 3))
	{
		get_opcode(older ,2);
		if(access(opcodeA, R_OK))
		{
			printf("本地路径不存在或不可读\n");
			fflush(stdout);
			return OLDER_NULL;
		}
		get_dstdir(opcodeA , opcodeB);
		return OLDER_PUT;
	}
	else if(0 == strncmp("ls" , older , 2))
	{
		get_opcode(older ,1);
		return OLDER_LS;
	}
	else if(0 == strncmp("rm" , older , 2))
	{
		get_opcode(older ,1);
		return OLDER_RM;
	}
	else if(0 == strncmp("mkdir" , older , 5))
	{
		get_opcode(older ,1);
		return OLDER_MKD;
	}
	else if(0 == strncmp("mv" , older , 2))
	{
		get_opcode(older ,2);
		return OLDER_MV;
	}	
	else
		return OLDER_INVALID;
}


/*

登录ftp
*/
static int ftp_login(void)
{
	int len;
	char sendbuf[50]={0};
	char readbuf[50]={0};
	fflush(stdout);  
	printf("ftp>用户名:");
	scanf("%s" , username);
	sprintf(sendbuf,"USER %s\r\n",username);
	if(strlen(sendbuf) != send(socket_fd , sendbuf , strlen(sendbuf) , 0))
	{
		return LOGINFAILD;
	}
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"331",3))		//username succeed
	{
		printf("用户名错误");
		return LOGINFAILD;
	}
	printf("ftp>密码:");
	fflush(stdout);  
	scanf("%s" , keyword);		
	memset(sendbuf , 0 , sizeof(sendbuf));
	sprintf(sendbuf,"PASS %s\r\n",keyword);              
	if(strlen(sendbuf) != send(socket_fd,sendbuf,strlen(sendbuf) , 0))
	{
		 return LOGINFAILD;
	}
	memset(readbuf , 0 , sizeof(readbuf));
	len = recv(socket_fd , readbuf , sizeof(readbuf) , 0);
	if(strncmp(readbuf,"230",3))		//username succeed
	{
		printf("密码错误\n");
		fflush(stdout);  
		return LOGINFAILD;
	}
	ftp_pasv();
	printf("登录成功\n");
	fflush(stdout);  
	setbuf(stdin, NULL);
	return LOGINSUCCEED;
}

int main(int argc , char **argv)
{
	int cmd;
	//char sendbuf[500]={0};
	char stdrevbuf[500]={0} ;
	char socketrevbuf[500]={0};
	fd_set rset;
	int maxfd;
	unsigned char  code=0xff ,older;
	if(get_serverip(argc , argv) <=0)
	{
		return -1;
	}
	socket_fd=open_tcpclient(serverip, serverport);
	if(-1 == socket_fd)
	{
		return -1;
	}
	recv(socket_fd, socketrevbuf , sizeof(socketrevbuf) , 0);
	printf("%s" , socketrevbuf);

	for( ; ; )
	{

		if(code != LOGINSUCCEED)
		{
			code = ftp_login();
			continue;
		}
		printf("ftp>:");
		fflush(stdout);  
		if(0 == my_gets(stdrevbuf , sizeof(stdrevbuf)))
			continue;
		setbuf(stdin , NULL);
		cmd=analysis_older(stdrevbuf);
		switch(cmd)
		{
			case OLDER_EXIT:
				ftp_exit();
				return 0;
			case OLDER_HELP:
				ftp_help();
				break;
			case OLDER_PWD:
				ftp_pwd();
				break;
			case OLDER_CD:
				ftp_cd();
				break;
			case OLDER_GET:
				ftp_get();
				break;
			case OLDER_PUT:
				ftp_put();
				break;
			case OLDER_LS:				
				ftp_ls();
				break;
			case OLDER_RM:				
				ftp_rm();
				break;
			case OLDER_MKD:				
				ftp_mkd();
				break;		
			case OLDER_MV:				
				ftp_mv();
				break;								
			case OLDER_NULL:
				break;
			default:
				printf("Invalid cmd\n");
				fflush(stdout);
				break;				
		}
	}
}

















