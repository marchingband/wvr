#ifndef BOOT_H
#define BOOT_H

void bootFromEmmc(int index);
int check_for_recovery_mode(void);
void bootIntoRecoveryMode(void);

#endif