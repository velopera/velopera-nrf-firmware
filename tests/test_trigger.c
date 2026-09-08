/*
 * Host-side test for trigger.c UART handling logic
 * 
 * Simulates UART input and verifies the trigger_task behavior
 */

#include "test_trigger.h"

/* Forward declaration of the UART handler logic (extracted from trigger.c) */
static void uart_handler_sim(char byte, test_state_t *state);

/* Public wrapper for UART handler simulation */
void test_uart_handler(char byte, test_state_t *state)
{
    uart_handler_sim(byte, state);
}

/* UART handler logic (extracted from trigger.c) */
static void uart_handler_sim(char byte, test_state_t *state)
{
    if (byte == '\n')
    {
        /* Copy rx_buf to payload.string */
        memset(state->payload.string, 0, sizeof(state->payload.string));
        state->payload_len = snprintf(state->payload.string, sizeof(state->payload.string), "%s", state->rx_buf);
        
        /* Reset rx_buf */
        memset(state->rx_buf, 0, sizeof(state->rx_buf));
        state->index = 0;
        
        state->lines_received++;
    }
    else if (state->index < RX_BUF_SIZE - 1)
    {
        state->rx_buf[state->index] = byte;
        state->index++;
    }
    else
    {
        /* Buffer overflow - reset buffer to recover */
        state->overflow_detected = true;
        memset(state->rx_buf, 0, sizeof(state->rx_buf));
        state->index = 0;
    }
}

/* Simple JSON validation - checks for basic JSON structure */
bool test_is_valid_json(const char *json_str)
{
    if (json_str == NULL || strlen(json_str) == 0) {
        return false;
    }
    
    /* Trim leading whitespace */
    const char *start = json_str;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    
    /* Check for valid JSON start/end */
    bool is_object = (*start == '{');
    bool is_array = (*start == '[');
    
    if (!is_object && !is_array) {
        return false;
    }
    
    /* Find the end character (ignoring trailing whitespace) */
    size_t len = strlen(start);
    const char *end = start + len - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    /* Check for matching end character */
    if (is_object && *end != '}') {
        return false;
    }
    if (is_array && *end != ']') {
        return false;
    }
    
    /* Check for balanced braces/brackets */
    int depth = 0;
    bool in_string = false;
    char open_char = is_object ? '{' : '[';
    char close_char = is_object ? '}' : ']';
    
    for (const char *p = start; p <= end; p++) {
        if (*p == '"' && !in_string) {
            in_string = true;
        } else if (*p == '"' && in_string) {
            in_string = false;
        } else if (!in_string) {
            if (*p == open_char) {
                depth++;
            } else if (*p == close_char) {
                depth--;
            }
        }
        
        if (depth < 0) {
            return false;
        }
    }
    
    if (depth != 0) {
        return false;
    }
    
    /* For objects, check that there's at least one key-value pair (indicated by ":") or is empty */
    if (is_object) {
        /* Check if it's an empty object */
        if (strlen(start) == 2 && start[0] == '{' && start[1] == '}') {
            return true;
        }
        /* Check for at least one colon (key-value separator) */
        bool has_colon = false;
        for (const char *p = start; p <= end; p++) {
            if (*p == ':') {
                has_colon = true;
                break;
            }
        }
        if (!has_colon) {
            return false;
        }
    }
    
    return true;
}

/* Simulate UART input character by character */
void test_simulate_uart_input(const char *input, test_state_t *state)
{
    const char *p = input;
    while (*p) {
        uart_handler_sim(*p, state);
        p++;
    }
}

/* Reset test state */
void test_reset_state(test_state_t *state)
{
    memset(state, 0, sizeof(test_state_t));
}

/* Print current test state for debugging */
void print_test_state(const test_state_t *state)
{
    printf("  Lines received: %d\n", state->lines_received);
    printf("  Valid JSON count: %d\n", state->valid_json_count);
    printf("  Invalid JSON count: %d\n", state->invalid_json_count);
    printf("  Overflow detected: %s\n", state->overflow_detected ? "YES" : "NO");
    printf("  Current rx_buf index: %d\n", state->index);
    printf("  Current rx_buf: '%s'\n", state->rx_buf);
    printf("  Payload: '%s'\n", state->payload.string);
}

/* Trigger task logic simulation */
void test_trigger_task_logic(test_state_t *state)
{
    /* Check if payload has content */
    if (state->payload.string[0] != '\0' && strlen(state->payload.string) > 0)
    {
        /* Validate JSON */
        bool is_json = test_is_valid_json(state->payload.string);
        
        if (is_json) {
            state->valid_json_count++;
            printf("  [VALID JSON] %s\n", state->payload.string);
        } else {
            state->invalid_json_count++;
            printf("  [INVALID JSON] %s\n", state->payload.string);
        }
        
        /* Clear payload for next iteration */
        memset(state->payload.string, 0, sizeof(state->payload.string));
        state->payload_len = 0;
    }
}
