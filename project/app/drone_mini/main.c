#include <stdio.h>
#include "imu.h"

int main() {
    printf("Drone Mini starting...\n");
    init_imu();
    read_imu_data();
    printf("Drone Mini running\n");
    return 0;
}