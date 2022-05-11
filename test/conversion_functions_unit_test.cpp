#include <Arduino.h>
#include <unity.h>
#include <command_ADC.h>





void test_negative_inputs(uint32_t data) {
    int x = (0.00133 * (2.4/3.3) * (data)) - 267.146; 
    TEST_ASSERT_EQUAL(x, convert_internal_temp(data));
}

void setup() {

    UNITY_BEGIN();    // IMPORTANT LINE!

}

void loop() {

    for (uint32_t test_data = -1000; test_data < 0; test_data += 50) {
        test_negative_inputs(test_data);
    }
}