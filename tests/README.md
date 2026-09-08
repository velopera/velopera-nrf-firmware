# Trigger Module Host Tests

## Overview

Host-side tests for the UART handling logic in `src/modules/trigger/trigger.c`.

These tests simulate UART input and verify the behavior of the trigger task, including:
- Character accumulation in the RX buffer
- Newline detection and payload transfer
- JSON validation
- Buffer overflow handling

## Test Coverage

### UART Handler Tests
- Simple valid JSON input
- Multiple JSON lines
- Empty lines
- Invalid JSON detection
- Partial JSON (missing closing brace)
- Buffer overflow scenarios
- Lines without newline (no payload update)
- JSON with special characters
- Character-by-character accumulation
- Maximum buffer size handling

### JSON Validation Tests
- Valid JSON objects and arrays
- Nested JSON structures
- JSON with escaped characters
- JSON with trailing whitespace
- Edge cases (empty strings, malformed JSON)

### Trigger Task Logic Tests
- Valid JSON processing
- Invalid JSON detection
- Mixed valid/invalid lines
- Payload clearing after processing

## Building and Running

```bash
cd tests
make clean
make test
```

Or simply:
```bash
make -C tests test
```

## Test Results

The test runner outputs detailed results for each test case and provides a summary at the end.

## Integration with Zephyr

These tests are designed to run on the host (x86) and can be used to verify the logic before deploying to the nRF target. The test code mirrors the logic from `trigger.c` to ensure consistency.

## Files

- `test_main.c` - Test runner with all test cases
- `test_trigger.c` - UART handler simulation and helper functions
- `test_trigger.h` - Header file with declarations
- `Makefile` - Build configuration
