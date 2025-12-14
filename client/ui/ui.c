#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/**
 * @brief Clear screen
 */
void ui_clear_screen()
{
    printf("\033[2J\033[H");
}

/**
 * @brief Print banner
 */
void ui_print_banner()
{
    printf("*********************************\n");
    printf("*       Welcome to Quiz App     *\n");
    printf("*********************************\n");
}

/**
 * @brief Print main menu (not logged in)
 */
void ui_print_menu_main()
{
    printf("\n=== MAIN MENU ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("0. Exit\n");
    printf("Choice: ");
}

/**
 * @brief Print authenticated menu
 */
void ui_print_menu_authenticated()
{
    printf("\n=== MAIN MENU (Logged in) ===\n");
    printf("1. Create Room\n");
    printf("2. Join Room\n");
    printf("3. List Rooms\n");
    printf("4. Logout\n");
    printf("0. Exit\n");
    printf("Choice: ");
}

/**
 * @brief Print room menu
 */
void ui_print_menu_room()
{
    // implement room menu UI
}

/**
 * @brief Show error message
 */
void ui_show_error(const char *message)
{
    printf("Error: %s\n", message);
}

/**
 * @brief Show success message
 */
void ui_show_success(const char *message)
{
    printf("Success: %s\n", message);
}

/**
 * @brief Show info message
 */
void ui_show_info(const char *message)
{
    printf("Info: %s\n", message);
}

/**
 * @brief Wait for user to press Enter
 */
void ui_wait_enter()
{
    printf("Press Enter to continue...");
    while (getchar() != '\n')
        ;
}

/**
 * @brief Get user input
 */
void ui_get_input(const char *prompt, char *buffer, size_t size)
{
    printf("%s", prompt);
    fgets(buffer, size, stdin);

    // remove newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }
}
