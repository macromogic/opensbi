#include <sbi/ebi/monitor.h>
#include <sbi/ebi/enclave.h>
#include <sbi/sbi_console.h>

__attribute__((unused)) void dump_enclave_status()
{
	for (int i = 1; i <= NUM_ENCLAVE; i++) {
		if (enclaves[i].status == ENC_FREE) {
			continue;
		}

		switch (enclaves[i].status) {
		case ENC_LOAD:
			sbi_printf("[ INFO ] EID = %d, status: loaded\n", i);
			break;
		case ENC_RUN:
			sbi_printf("[ INFO ] EID = %d, status: running\n", i);
			break;
		case ENC_IDLE:
			sbi_printf("[ INFO ] EID = %d, status: idle\n", i);
			break;
		default:
			sbi_printf("[ ERROR ] something went wrong\n");
			break;
		}
	}
}