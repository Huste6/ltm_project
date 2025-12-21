#include "client.h"
#include "ui/ui.h"
#include "handle/handle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    Client client;
    int running = 1;

    ui_clear_screen();
    ui_print_banner();

    // Connect to server
    if (client_connect(&client, SERVER_IP, SERVER_PORT) < 0)
    {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }

    while (running)
    {
        int choice;

        switch (client.state)
        {
        case CLIENT_CONNECTED:
            ui_print_menu_main();
            scanf("%d", &choice);
            getchar(); // consume newline

            switch (choice)
            {
            case 1:
                handle_register(&client);
                break;
            case 2:
                handle_login(&client);
                break;
            case 0:
                running = 0;
                break;
            default:
                ui_show_error("Invalid choice!");
            }
            break;
        case CLIENT_AUTHENTICATED:
            ui_print_menu_authenticated();
            scanf("%d", &choice);
            getchar();

            switch (choice)
            {
            case 1:
                handle_create_room(&client);
                break;
            case 2:
                handle_join_room(&client);
                break;
            case 3:
                handle_list_rooms(&client);
                break;
            case 4:
                handle_view_result(&client);
                break;
            case 5:
                handle_logout(&client);
                break;
            case 0:
                running = 0;
                break;
            default:
                ui_show_error("Invalid choice!");
            }
            break;
        case CLIENT_IN_ROOM:
            fd_set readfds; // select descriptor set
            int maxfd;
            char buffer[BUFFER_SIZE];

            // PARTICIPANT (NOT CREATOR)
            if (!client.is_creator)
            {
                // Print room menu once
                ui_print_menu_room(0);
                printf("⏳ Waiting for creator to start the exam...\n");

                while (client.state == CLIENT_IN_ROOM)
                {
                    FD_ZERO(&readfds);                  // clear fd_set
                    FD_SET(client.socket_fd, &readfds); // socket from server
                    FD_SET(0, &readfds);                // stdin (keyboard)

                    maxfd = client.socket_fd; // socket fd is always > 0

                    // BLOCK until an event occurs
                    if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0)
                    {
                        perror("select");
                        break;
                    }

                    // ===== EVENT: SERVER MESSAGE =====
                    if (FD_ISSET(client.socket_fd, &readfds))
                    {
                        int bytes = recv(client.socket_fd, buffer, sizeof(buffer) - 1, 0);
                        if (bytes <= 0)
                        {
                            ui_show_error("Server disconnected");
                            client.state = CLIENT_DISCONNECTED;
                            break;
                        }

                        buffer[bytes] = '\0';

                        if (strstr(buffer, "125 START_OK") != NULL)
                        {
                            printf("\n🎉 Creator has started the exam!\n");
                            printf("📝 Loading exam questions...\n");

                            client.state = CLIENT_IN_EXAM;
                            handle_get_exam(&client);
                            break;
                        }
                    }

                    // ===== EVENT: USER INPUT =====
                    if (FD_ISSET(0, &readfds))
                    {
                        int choice;
                        scanf("%d", &choice);
                        getchar(); // consume '\n'

                        switch (choice)
                        {
                        case 1:
                            handle_leave_room(&client);
                            break;
                        case 0:
                            client.state = CLIENT_AUTHENTICATED;
                            break;
                        default:
                            ui_show_error("Invalid choice!");
                            break;
                        }
                    }
                }
            }
            else
            {
                ui_print_menu_room(1);
                scanf("%d", &choice);
                getchar();

                switch (choice)
                {
                case 1:
                    handle_start_exam(&client);
                    break;
                case 2:
                    handle_leave_room(&client);
                    break;
                case 0:
                    client.state = CLIENT_AUTHENTICATED;
                    break;
                default:
                    ui_show_error("Invalid choice!");
                    break;
                }
            }
            break;
        case CLIENT_IN_EXAM:
            printf("\n=== EXAM IN PROGRESS ===\n");
            printf("1. Get Exam Questions\n");
            printf("2. Submit Exam\n");
            printf("0. Exit Exam\n");
            printf("Choice: ");
            scanf("%d", &choice);
            getchar();

            switch (choice)
            {
            case 1:
                handle_get_exam(&client);
                break;
            case 2:
                handle_submit_exam(&client);
                break;
            case 0:
                client.state = CLIENT_AUTHENTICATED;
                memset(client.current_room, 0, sizeof(client.current_room));
                client.is_creator = 0;
                break;
            default:
                ui_show_error("Invalid choice!");
                break;
            }
            break;
        default:
            ui_show_error("Unknown client state!");
            break;
        }

        if (running)
        {
            ui_wait_enter();
        }
    }

    client_disconnect(&client);
    printf("\nGoodbye!\n");

    return 0;
}