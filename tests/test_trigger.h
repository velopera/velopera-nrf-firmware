/*
 * Host-side test for trigger.c UART handling logic
 * 
 * This simulates the UART read loop and tests:
 * - Character accumulation in rx_buf
 * - Newline detection and payload transfer
 * - Buffer overflow handling
 * - JSON validation
 */

#ifndef TEST_TRIGGER_H
#define TEST_TRIGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define RX_BUF_SIZE 512

/* Payload structure (mirrors velopera_payload) */
struct test_payload {
    char string[700];
};

/* Test state - mirrors the trigger.c globals */
typedef struct {
    char rx_buf[RX_BUF_SIZE];
    int index;
    struct test_payload payload;
    int payload_len;
    bool overflow_detected;
    int lines_received;
    int valid_json_count;
    int invalid_json_count;
} test_state_t;

/* Function declarations */
void test_uart_handler(char byte, test_state_t *state);
bool test_is_valid_json(const char *json_str);
void test_trigger_task_logic(test_state_t *state);
void print_test_state(const test_state_t *state);

/* Test helper functions */
void test_simulate_uart_input(const char *input, test_state_t *state);
void test_reset_state(test_state_t *state);

#endif /* TEST_TRIGGER_H */
