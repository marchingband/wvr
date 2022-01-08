#ifndef BOOT_H
#define BOOT_H

void bootFromEmmc(int index);
int check_for_recovery_mode();
void bootIntoRecoveryMode(void);
void force_reset(void);

#endif