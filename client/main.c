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
            // For participants: Continuously poll for broadcast
            if (!client.is_creator)
            {
                while (1)
                {
                    ui_print_menu_room(client.is_creator);

                    fd_set readfds;
                    struct timeval tv;
                    FD_ZERO(&readfds);
                    FD_SET(client.socket_fd, &readfds);
                    tv.tv_sec = 5; // 5 second timeout - less frequent polling
                    tv.tv_usec = 0;

                    int ready = select(client.socket_fd + 1, &readfds, NULL, NULL, &tv);
                    if (ready > 0)
                    {
                        // There's data available - might be a broadcast
                        char buffer[BUFFER_SIZE];
                        int bytes = recv(client.socket_fd, buffer, sizeof(buffer) - 1, MSG_PEEK);
                        if (bytes > 0)
                        {
                            buffer[bytes] = '\0';
                            // Check if it's a START_OK broadcast
                            if (strstr(buffer, "125 START_OK") != NULL)
                            {
                                // Consume the message
                                recv(client.socket_fd, buffer, sizeof(buffer) - 1, 0);
                                printf("\n🎉 Creator has started the exam!\n");
                                client.state = CLIENT_IN_EXAM;

                                // AUTO GET_EXAM - theo đặc tả bắt buộc
                                printf("📝 Loading exam questions...\n");
                                handle_get_exam(&client);

                                ui_wait_enter();
                                break; // Exit polling loop
                            }
                        }
                    }

                    // Check if stdin has input (non-blocking)
                    fd_set stdin_fds;
                    struct timeval stdin_tv;
                    FD_ZERO(&stdin_fds);
                    FD_SET(STDIN_FILENO, &stdin_fds);
                    stdin_tv.tv_sec = 0;
                    stdin_tv.tv_usec = 0;

                    if (select(STDIN_FILENO + 1, &stdin_fds, NULL, NULL, &stdin_tv) > 0)
                    {
                        // User pressed something, read it
                        scanf("%d", &choice);
                        getchar();

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
                            continue; // Continue polling loop
                        }
                        break; // Exit polling loop after processing choice
                    }

                    // Status update every 5 seconds (less intrusive)
                    printf("\r%-60s", "⏳ Waiting for creator to start... (Press 1 to leave, 0 to back)");
                    fflush(stdout);
                }
            }
            else
            {
                // Creator menu (normal behavior)
                ui_print_menu_room(client.is_creator);
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