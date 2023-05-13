#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip.h>

char    read_buff[1001];
char    write_buff[42];
int     count = 0;
int     max_fd = 0;
int     ids[65536];
char    *msgs[65536];

fd_set  rfds, wfds, afds;

void    fatal_error(void)
{
    write(2, "Fatal error\n", 12);
    exit(1);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
                return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
        return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void    notify_clients(int author, char *msg)
{
    for (int fd = 0; fd <= max_fd; fd++)
    {
        if (FD_ISSET(fd, &wfds) && fd != author)
            send(fd, msg, strlen(msg), 0);
    }
}

void receive_client(int fd)
{
    max_fd = fd > max_fd ? fd : max_fd;
    ids[fd] = count++;
    msgs[fd] = NULL;
    FD_SET(fd, &afds);
    sprintf(write_buff, "server: client %d just arrived\n", ids[fd]);
    notify_clients(fd, write_buff);
}

void remove_client(int fd)
{
    sprintf(write_buff, "server: client %d just left\n", ids[fd]);
    notify_clients(fd, write_buff);
    free(msgs[fd]);
    FD_CLR(fd, &afds);
    close(fd);
}

void    send_msg(int fd)
{
    char *msg = NULL;

    while (extract_message(&msgs[fd], &msg))
    {
        sprintf(write_buff, "client %d: ", ids[fd]);
        notify_clients(fd, write_buff);
        notify_clients(fd, msg);
        free(msg);
    }
}

int create_socket(void)
{
	max_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (max_fd < 0)
        fatal_error();
    FD_SET(max_fd, &afds);
    return (max_fd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

    FD_ZERO(&afds);
    int socket_fd = create_socket();

    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 

    if (bind(socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
        fatal_error();
    if (listen(socket_fd, SOMAXCONN))
        fatal_error();
    while(1)
    {
        rfds = wfds = afds;
        if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
            fatal_error();
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (!FD_ISSET(fd, &rfds))
                continue;
            if (fd == socket_fd)
            {
                socklen_t addr_len = sizeof(servaddr);
                int client_fd = accept(socket_fd, (struct sockaddr *)&servaddr, &addr_len);
                if (client_fd >= 0)
                {
                    receive_client(client_fd);
                    break ;
                }
            }
            else
            {
                int bytes_read = recv(fd, read_buff, 1000, 0);
                if (bytes_read <= 0)
                {
                    remove_client(fd);
                    break ;
                }
                read_buff[bytes_read] = '\0';
                msgs[fd] = str_join(msgs[fd], read_buff);
                send_msg(fd);
            }
        }
    }
    return (0);
}
