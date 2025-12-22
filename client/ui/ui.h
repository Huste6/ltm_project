#ifndef UI_H
#define UI_H

#include "../client.h"

/**
 * @brief Clear screen
 */
void ui_clear_screen();

/**
 * @brief Print banner
 */
void ui_print_banner();

/**
 * @brief Print main menu (not logged in)
 */
void ui_print_menu_main();

/**
 * @brief Print authenticated menu
 */
void ui_print_menu_authenticated();

/**
 * @brief Print room menu
 * @param is_creator Whether the user is the room creator
 */
void ui_print_menu_room(int is_creator);

void ui_print_menu_exam();

void ui_print_list_room_filter();

/**
 * @brief Display error message
 * @param message Error message
 */
void ui_show_error(const char *message);

/**
 * @brief Display success message
 * @param message Success message
 */
void ui_show_success(const char *message);

/**
 * @brief Display info message
 * @param message Info message
 */
void ui_show_info(const char *message);

/**
 * @brief Wait for user to press Enter
 */
void ui_wait_enter();

/**
 * @brief Get user input
 * @param prompt Prompt message
 * @param buffer Buffer to store input
 * @param size Buffer size
 */
void ui_get_input(const char *prompt, char *buffer, size_t size);

#endif // UI_H
