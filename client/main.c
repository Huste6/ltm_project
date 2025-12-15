#include "client.h"
#include "ui/ui.h"
#include "handle/handle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            ui_print_menu_room();
            scanf("%d", &choice);
            getchar();

            switch (choice)
            {
            case 1:
                // handle_start_exam(&client);
                ui_show_info("Start Exam feature not implemented yet.");
                break;
            case 2:
                // handle_leave_room(&client);
                ui_show_info("Leave Room feature not implemented yet.");
                break;
            case 0:
                client.state = CLIENT_AUTHENTICATED;
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