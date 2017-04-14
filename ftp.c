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
char username[30];
char keyword[30];

enum ReplyCode
{
	USERNAME,
	KEYWORD
};
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
void disphelp(void)
{
	printf("\n/------------------------------------------/\n");
	printf("FTP   Client\nAuthor:凌福义\nemail:lingfuyi@126.com\n");
	printf("\n基础命令:	\n");
	printf("ls:显示当前工作目录的所有文件  \n");
	printf("pwd:显示当前工作目录\n");
	printf("cd:切换工作目录\n");
	printf("cp:复制文件\n");
	printf("rm:删除文件\n");
	printf("\n/------------------------------------------/\n");
	fflush(stdout);  

}

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
	{	printf("\n连接成功 %d!\n",control_sock);
		return control_sock;
	}
}

static int login_ftp(int controlsock)
{
	int len;
	char sendbuf[50]={0};
	char readbuf[50]={0};
	while(1)
	{
		printf("\n用户名: ");
		scanf("%s" , username);
		printf("密码: ");
		scanf("%s" , keyword);
		fflush(stdout);
		sprintf(sendbuf,"USER %s\r\n",username);
		read(controlsock , readbuf , sizeof(readbuf));
		if(strlen(sendbuf) != write(controlsock , sendbuf , strlen(sendbuf)))
		{
			printf("\n 用户名错误");
		}
		else
		{
			read(controlsock , readbuf , sizeof(readbuf));
		}
		printf("%s" , readbuf);
		fflush(stdout);  
	}
	return 0;
}
int main(int argc , char **argv)
{
	char sendbuf[50]={0};
	char stdrevbuf[50]={0} ;
	char socketrevbuf[50]={0};
	fd_set rset;
	int socket_fd , maxfd;
	unsigned char  code=0xff;
	if(get_serverip(argc , argv) <=0)
	{
		return -1;
	}
	socket_fd=open_tcpclient(serverip, serverport);
	if(-1 == socket_fd)
	{
		return -1;
	}

	FD_ZERO(&rset);
	(socket_fd > STDIN_FILENO)?(maxfd=socket_fd):(maxfd=STDIN_FILENO);
	maxfd+=1;
	for( ; ; )
	{
		FD_SET(STDIN_FILENO , &rset);
		FD_SET(socket_fd , &rset);
		if(select(maxfd , &rset , NULL , NULL , NULL)<0)
		{
			printf("select error\n");
			return -1;
		}

		if(FD_ISSET(STDIN_FILENO , &rset))			//has dat in keyboard std
		{
			if((read(STDIN_FILENO,stdrevbuf,sizeof(stdrevbuf))) <=0)
			{
				printf("read error\n");
				break;
			}

			if(USERNAME == code)
			{
				sprintf(sendbuf,"USER %s",stdrevbuf);              
				if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
				{
					 printf("write error\n");
				}
				continue;
			}
			if(KEYWORD == code)
			{
				sprintf(sendbuf,"PASS %s",stdrevbuf);              
				if(write(socket_fd,sendbuf,strlen(sendbuf)) != strlen(sendbuf))
				{
					 printf("write error\n");
				}
				continue;
			}
			
		}
		if(FD_ISSET(socket_fd,&rset))				//has dat in socket fd
		{
			if(recv(socket_fd,socketrevbuf,sizeof(socketrevbuf),0)<=0)
            {
				printf("recv error\n");
				break;
			}
			if(strncmp(socketrevbuf,"220",3) ==0 || strncmp(socketrevbuf,"530",3)==0)		//connect succeed
			{
				fprintf(stdout, "\n%s , 请输入用户名:", socketrevbuf);
				//printf("\n%s , 请输入用户名:",socketrevbuf);
				code = USERNAME;
				continue;
			}
			if(strncmp(socketrevbuf,"331",3) ==0 )		//username succeed
			{
				printf("\n%s:",socketrevbuf);
				code = KEYWORD;
				continue;
			}
			if(strncmp(socketrevbuf,"230",3) ==0 )		//keyword succeed
			{
				printf("\n%s:",socketrevbuf);
				code=0xff;
				continue;
			}
		}
	}
}
















#if 0



void cmd_tcp(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    char *pathname;
    fd_set rset;
    FD_ZERO(&rset);
    maxfdp1 = sockfd + 1;
    for(;;)
    {
         FD_SET(STDIN_FILENO,&rset);
         FD_SET(sockfd,&rset);
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         }
         if(FD_ISSET(STDIN_FILENO,&rset))                       //判断是从键盘输入的，还是从服务器端返回的
         {
              //nwrite = 0;
              //nread = 0;
              bzero(wbuf,MAXBUF);          //zero               //当然要注意的是，每次在重新读取缓冲区里的内容时，需要将原来的缓冲区清空，用到的是bzero函数，或者也可以用memset函数
              bzero(rbuf1,MAXBUF);
              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;
              //printf("%d\n",nread);      
              if(replycode == USERNAME)
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 //printf("%s\n",wbuf);
                 //memset(rbuf1,0,sizeof(rbuf1));
                 //memset(wbuf,0,sizeof(wbuf));
                 //printf("1:%s\n",wbuf);
              }


              if(replycode == PASSWORD)
              {
                   //printf("%s\n",rbuf1);
                   sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
                   //bzero(rbuf,sizeof(rbuf));
                   //printf("%s\n",wbuf);
                   //printf("2:%s\n",wbuf);
              }
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
          if(strncmp(rbuf1,"pwd",3) == 0)
              {   
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     //close(sockfd);
                    if(close(sockfd) <0)
                       printf("close error\n");
                    break;
                 }
                 if(strncmp(rbuf1,"cwd",3) == 0)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sprintf(wbuf,"%s",rbuf1);
                     write(sockfd,wbuf,nread);
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PASV\n");
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,5);
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;    
                 }
                 if(strncmp(rbuf1,"get",3) == 0)
                 {
                     tag = 1;
                     sprintf(wbuf,"%s","PASV\n");                   
                     //printf("%s\n",s(rbuf1));
                     //char filename[100];
                     s(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }
                 if(strncmp(rbuf1,"put",3) == 0)
                 {
                     tag = 3;
                     sprintf(wbuf,"%s","PASV\n");
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }
              } 
                    /*if(close(sockfd) <0)
                       printf("close error\n");*/
         }
         if(FD_ISSET(sockfd,&rset))
         {
             bzero(rbuf,strlen(rbuf));
             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;
           
             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                 strcat(rbuf,"your name:");
                 //printf("%s\n",rbuf);
                 nread += 12;
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n")*/;
                strcat(rbuf,"your password:");
                nread += 16;
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                replycode = PASSWORD;
             }
             if(strncmp(rbuf,"230",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                replycode = LOGIN;
             }
             if(strncmp(rbuf,"257",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                replycode = PATHNAME;  
             }
             if(strncmp(rbuf,"226",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                replycode = CLOSEDATA;
             }
             if(strncmp(rbuf,"250",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/
                replycode = ACTIONOK;
             }
             if(strncmp(rbuf,"550",3) == 0)
             {
                replycode = 550;
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");
             }*/    
             //fprintf(stderr,"%d\n",1);
             if(strncmp(rbuf,"227",3) == 0)
             {
                //printf("%d\n",1);
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                   printf("write error to stdout\n");*/
                int port1 = strtosrv(rbuf);
                printf("%d\n",port1);
                printf("%s\n",host);
                data_sock = cliopen(host,port1);
        


//bzero(rbuf,sizeof(rbuf));
                //printf("%d\n",fd);
                //if(strncmp(rbuf1,"ls",2) == 0)
                if(tag == 2)
                {
                   write(sockfd,"list\n",strlen("list\n"));
                   ftp_list(data_sock);
                   /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                       printf("write error to stdout\n");*/
                   
                }
                //else if(strncmp(rbuf1,"get",3) == 0)
                else if(tag == 1)
                {
                    //sprintf(wbuf,"%s","RETR\n");
                    //printf("%s\n",wbuf);
                    //int str = strlen(filename);
                    //printf("%d\n",str);
                    sprintf(wbuf,"RETR %s\n",filename);
                    printf("%s\n",wbuf);
                    //int p = 5 + str + 1;


                    printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);
                    ftp_get(data_sock,filename);
                }
                else if(tag == 3)
                {
                    sprintf(wbuf,"STOR %s\n",filename);
                    printf("%s\n",wbuf);
                    write(sockfd,wbuf,strlen(wbuf));
                    ftp_put(data_sock,filename);
                }
                nwrite = 0;     
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                 if(write(STDOUT_FILENO,rbuf,nread) != nread)
                     printf("write error to stdout\n");
             }*/
             //printf("%s\n",rbuf);
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
             /*else 
                 printf("%d\n",-1);*/            
         }
    }
}

#endif
