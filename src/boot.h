#ifndef BOOT_H
#define BOOT_H

void bootFromEmmc(int index);
int check_for_recovery_mode(void);
int check_for_ble_pin(void);
void bootIntoRecoveryMode(void);
void force_reset(void);

#endif