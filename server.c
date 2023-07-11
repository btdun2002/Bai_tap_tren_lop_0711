#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

struct client_info
{
    int fd;
    char client_name[32];
    int is_set_info;
};

struct client_info clients[16] = {0};
int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *);

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        if (client == -1)
        {
            perror("accept() failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        printf("New client connected: %d\n", client);
        clients[num_clients].fd = client;
        num_clients++;

        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, &client);
        pthread_detach(thread_id);
    }

    close(listener);

    return 0;
}

void *client_thread(void *param)
{
    int client = *(int *)param;
    char *client_name;
    char buf[256];
    char mess[512];

    int i = 0;
    for (int j = 0; j < 16; j++)
    {
        if (clients[j].fd == client)
        {
            i = j;
            break;
        }
        if (j == 15)
        {
            printf("Failed at line 88");
            exit(0);
        }
    }

    while (1)
    {
        int ret = recv(clients[i].fd, buf, sizeof(buf), 0);
        buf[strcspn(buf, "\n")] = '\0';
        char *flag = strtok(buf, " ");
        char *name = strtok(NULL, " ");
        if (strcmp(flag, "JOIN") == 0)
        {
            int invalid_str = 0;
            for (int i = 0; i < strlen(name); i++)
            {
                if (name[i] >= 'a' && name[i] <= 'z' || name[i] >= '0' && name[i] <= '9')
                {
                }
                else
                {
                    invalid_str = 1;
                    send(clients[i].fd, "201 INVALID NICK NAME", strlen("201 INVALID NICK NAME"), 0);
                    break;
                }
            }

            if (invalid_str == 0)
            {
                FILE *f = fopen("database.txt", "r");
                char line[16];
                char db_name[256];
                while (fgets(line, sizeof(line), f) != NULL)
                {
                    line[strcspn(line, "\n")] = '\0';
                    sscanf(line, "%s", db_name);
                    if (strcmp(db_name, name) == 0)
                    {
                        send(clients[i].fd, "200 NICKNAME IN USE", strlen("200 NICKNAME IN USE"), 0);
                    }
                }
                send(clients[i].fd, "100 OK", strlen("100 OK"), 0);

                strcpy(clients[i].client_name, buf);
                clients[i].is_set_info = 1;

                snprintf(mess, sizeof(mess), "JOIN %s\n", buf);

                for (int j = 0; j < 16; j++)
                    send(clients[j].fd, mess, strlen(mess), 0);
            }
        }
        else
        {
            while (1)
            {
                ret = recv(clients[i].fd, buf, sizeof(buf), 0);
                buf[strcspn(buf, "\n")] = '\0';
                puts(buf);
                if (strncmp(buf, "@", 1) == 0)
                {
                    char *recipient = strtok(buf + 1, ":");
                    char *content = strtok(NULL, ":");

                    snprintf(mess, sizeof(mess), "PMSG %s\n", clients[i].client_name);

                    for (int j = 0; j < 16; j++)
                        send(clients[j].fd, mess, strlen(mess), 0);

                    for (int j = 0; j < 16; j++)
                    {
                        if (strcmp(clients[j].client_name, recipient) == 0)
                        {
                            {
                                snprintf(mess, sizeof(mess), "%s: %s", clients[i].client_name, content);
                                send(clients[j].fd, mess, strlen(mess), 0);
                            }
                        }
                    }
                }
                else
                {
                    snprintf(mess, sizeof(mess), "MSG %s %s\n", clients[i].client_name, buf);

                    for (int j = 0; j < 16; j++)
                        send(clients[j].fd, mess, strlen(mess), 0);
                }
            }
        }

        close(client);
    }
}