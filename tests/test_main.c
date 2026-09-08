/*
 * Test runner for trigger.c UART handling logic
 * 
 * Tests:
 * - Regular JSON input
 * - Irregular/malformed lines
 * - Buffer overflow scenarios
 * - Empty lines
 * - Partial JSON lines
 * - Multiple consecutive lines
 */

#include "test_trigger.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START() do { tests_run++; printf("\n[Test %d] %s\n", tests_run, __func__); } while(0)
#define TEST_PASS() do { tests_passed++; printf("  PASSED\n"); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf("  FAILED: %s\n", msg); } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("  ASSERT FAILED: %d != %d\n", (a), (b)); return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp((a), (b)) != 0) { printf("  ASSERT FAILED: '%s' != '%s'\n", (a), (b)); return; } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { printf("  ASSERT FAILED: condition false\n"); return; } } while(0)
#define ASSERT_FALSE(cond) do { if ((cond)) { printf("  ASSERT FAILED: condition true\n"); return; } } while(0)

/* Test 1: Simple valid JSON line */
void test_simple_valid_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"sensor\":\"temperature\",\"value\":25.5}\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_STR_EQ(state.payload.string, "{\"sensor\":\"temperature\",\"value\":25.5}");
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 2: Multiple JSON lines */
void test_multiple_json_lines(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"id\":1}\n{\"id\":2}\n{\"id\":3}\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 3);
    ASSERT_STR_EQ(state.payload.string, "{\"id\":3}");
    
    TEST_PASS();
}

/* Test 3: Empty line */
void test_empty_line(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_TRUE(state.payload.string[0] == '\0');
    
    TEST_PASS();
}

/* Test 4: Invalid JSON */
void test_invalid_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "not json at all\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_FALSE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 5: Partial JSON (missing closing brace) */
void test_partial_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"sensor\":\"temperature\"\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_FALSE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 6: Buffer overflow */
void test_buffer_overflow(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    /* Create a string larger than RX_BUF_SIZE */
    char large_input[RX_BUF_SIZE + 10];
    memset(large_input, 'A', sizeof(large_input) - 1);
    large_input[sizeof(large_input) - 1] = '\n';
    
    test_simulate_uart_input(large_input, &state);
    
    ASSERT_TRUE(state.overflow_detected);
    ASSERT_EQ(state.lines_received, 1);
    
    TEST_PASS();
}

/* Test 7: Line without newline (should not trigger payload update) */
void test_line_without_newline(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"sensor\":\"test\"";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 0);
    ASSERT_TRUE(state.payload.string[0] == '\0');
    ASSERT_STR_EQ(state.rx_buf, "{\"sensor\":\"test\"");
    
    TEST_PASS();
}

/* Test 8: JSON with special characters */
void test_json_with_special_chars(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"message\":\"Hello, World! @#$%^&*()\"}\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 9: Trigger task logic with valid JSON */
void test_trigger_task_valid_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"sensor\":\"gps\",\"lat\":47.5,\"lon\":12.3}\n";
    test_simulate_uart_input(input, &state);
    
    test_trigger_task_logic(&state);
    
    ASSERT_EQ(state.valid_json_count, 1);
    ASSERT_TRUE(state.payload.string[0] == '\0'); /* Should be cleared */
    
    TEST_PASS();
}

/* Test 10: Trigger task logic with invalid JSON */
void test_trigger_task_invalid_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "random garbage data\n";
    test_simulate_uart_input(input, &state);
    
    test_trigger_task_logic(&state);
    
    ASSERT_EQ(state.invalid_json_count, 1);
    
    TEST_PASS();
}

/* Test 11: JSON array */
void test_json_array(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "[1,2,3,4,5]\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 12: Nested JSON */
void test_nested_json(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"data\":{\"nested\":{\"value\":42}}}\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 13: Real-world sensor data */
void test_real_sensor_data(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = 
        "{\"Latitude\":\"47.523120\",\"Longitude\":\"12.345670\",\"Altitude\":\"500.00\"}\n"
        "{\"sensor\":\"compass\",\"heading\":\"180.5\"}\n"
        "{\"error\":\"timeout\"}\n";
    
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 3);
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 14: Consecutive empty lines */
void test_consecutive_empty_lines(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "\n\n\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 3);
    
    TEST_PASS();
}

/* Test 15: JSON with escaped characters */
void test_json_with_escapes(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"message\":\"line1\\nline2\"}\n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Test 16: Character-by-character accumulation */
void test_char_by_char_accumulation(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    /* Feed characters one by one */
    const char *json = "{\"test\":true}";
    for (size_t i = 0; json[i] != '\0'; i++) {
        test_uart_handler(json[i], &state);
    }
    
    /* Should not have triggered yet (no newline) */
    ASSERT_EQ(state.lines_received, 0);
    ASSERT_STR_EQ(state.rx_buf, "{\"test\":true}");
    
    /* Now send newline */
    test_uart_handler('\n', &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_STR_EQ(state.payload.string, "{\"test\":true}");
    
    TEST_PASS();
}

/* Test 17: Max buffer size */
void test_max_buffer_size(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    /* Fill buffer to exactly RX_BUF_SIZE - 1 */
    char input[RX_BUF_SIZE];
    memset(input, 'A', RX_BUF_SIZE - 2);
    input[RX_BUF_SIZE - 2] = '\n';
    input[RX_BUF_SIZE - 1] = '\0';
    
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    ASSERT_FALSE(state.overflow_detected);
    
    TEST_PASS();
}

/* Test 17b: Buffer overflow recovery */
void test_overflow_recovery(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    /* Fill buffer beyond capacity */
    char large_input[RX_BUF_SIZE + 10];
    memset(large_input, 'A', sizeof(large_input) - 1);
    large_input[sizeof(large_input) - 1] = '\n';
    
    test_simulate_uart_input(large_input, &state);
    
    ASSERT_TRUE(state.overflow_detected);
    /* After overflow recovery, index should be reset */
    ASSERT_EQ(state.index, 0);
    
    TEST_PASS();
}

/* Test 18: JSON validation edge cases */
void test_json_validation_edge_cases(void)
{
    TEST_START();
    
    /* Valid JSON */
    if (!test_is_valid_json("{}")) { printf("  ASSERT FAILED: {} should be valid\n"); return; }
    if (!test_is_valid_json("{\"a\":1}")) { printf("  ASSERT FAILED: {\"a\":1} should be valid\n"); return; }
    if (!test_is_valid_json("[]")) { printf("  ASSERT FAILED: [] should be valid\n"); return; }
    if (!test_is_valid_json("[1,2,3]")) { printf("  ASSERT FAILED: [1,2,3] should be valid\n"); return; }
    
    /* Invalid JSON */
    if (test_is_valid_json("{")) { printf("  ASSERT FAILED: { should be invalid\n"); return; }
    if (test_is_valid_json("}")) { printf("  ASSERT FAILED: } should be invalid\n"); return; }
    if (test_is_valid_json("hello")) { printf("  ASSERT FAILED: hello should be invalid\n"); return; }
    if (test_is_valid_json("{invalid}")) { printf("  ASSERT FAILED: {invalid} should be invalid\n"); return; }
    
    /* Empty string should be invalid */
    if (test_is_valid_json("")) { printf("  ASSERT FAILED: empty string should be invalid\n"); return; }
    if (test_is_valid_json(NULL)) { printf("  ASSERT FAILED: NULL should be invalid\n"); return; }
    
    TEST_PASS();
}

/* Test 19: Mixed valid and invalid lines */
void test_mixed_valid_invalid(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    /* Process each line individually to simulate trigger task behavior */
    const char *lines[] = {
        "{\"valid\":true}\n",
        "invalid line\n",
        "{\"also\":true}\n",
        "another invalid\n"
    };
    
    for (int i = 0; i < 4; i++) {
        /* Don't reset state - we want to accumulate counts */
        test_simulate_uart_input(lines[i], &state);
        test_trigger_task_logic(&state);
    }
    
    ASSERT_EQ(state.valid_json_count, 2);
    ASSERT_EQ(state.invalid_json_count, 2);
    
    TEST_PASS();
}

/* Test 20: Trailing whitespace in JSON */
void test_json_with_trailing_whitespace(void)
{
    TEST_START();
    test_state_t state;
    test_reset_state(&state);
    
    const char *input = "{\"value\":42}   \n";
    test_simulate_uart_input(input, &state);
    
    ASSERT_EQ(state.lines_received, 1);
    /* JSON with trailing whitespace should still be valid */
    ASSERT_TRUE(test_is_valid_json(state.payload.string));
    
    TEST_PASS();
}

/* Run all tests */
int main(void)
{
    printf("=== Trigger Module Host Tests ===\n\n");
    
    /* Run all tests */
    test_simple_valid_json();
    test_multiple_json_lines();
    test_empty_line();
    test_invalid_json();
    test_partial_json();
    test_buffer_overflow();
    test_line_without_newline();
    test_json_with_special_chars();
    test_trigger_task_valid_json();
    test_trigger_task_invalid_json();
    test_json_array();
    test_nested_json();
    test_real_sensor_data();
    test_consecutive_empty_lines();
    test_json_with_escapes();
    test_char_by_char_accumulation();
    test_max_buffer_size();
    test_overflow_recovery();
    test_json_validation_edge_cases();
    test_mixed_valid_invalid();
    test_json_with_trailing_whitespace();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    return (tests_failed == 0) ? 0 : 1;
}
