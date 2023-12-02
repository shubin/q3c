// only include in cg_ext.c, g_ext.c, ui_ext.c

#ifndef Q3_VM
#include "q_shared.h"
extern dllSyscall_t syscall;
#endif

#define EMPTY

#ifdef Q3_VM
#define DEF_TRAP_0(ret, R, Name) typedef R (*Name##_t)(void); Name##_t Name
#define DEF_TRAP_1(ret, R, Name, A1) typedef R (*Name##_t)(A1); Name##_t Name
#define DEF_TRAP_2(ret, R, Name, A1, A2) typedef R (*Name##_t)(A1, A2); Name##_t Name
#define DEF_TRAP_3(ret, R, Name, A1, A2, A3) typedef R (*Name##_t)(A1, A2, A3); Name##_t Name
#define DEF_TRAP_4(ret, R, Name, A1, A2, A3, A4) typedef R (*Name##_t)(A1, A2, A3, A4); Name##_t Name
#define DEF_TRAP_5(ret, R, Name, A1, A2, A3, A4, A5) typedef R (*Name##_t)(A1, A2, A3, A4, A5); Name##_t Name
#define DEF_TRAP_6(ret, R, Name, A1, A2, A3, A4, A5, A6) typedef R (*Name##_t)(A1, A2, A3, A4, A5, A6); Name##_t Name
#else
#define DEF_TRAP_0(ret, R, Name) R Name(void) { ret syscall(cpma_ext.Name); }
#define DEF_TRAP_1(ret, R, Name, A1) R Name(A1 a1) { ret syscall(cpma_ext.Name, (int)a1); }
#define DEF_TRAP_2(ret, R, Name, A1, A2) R Name(A1 a1, A2 a2) { ret syscall(cpma_ext.Name, (int)a1, (int)a2); }
#define DEF_TRAP_3(ret, R, Name, A1, A2, A3) R Name(A1 a1, A2 a2, A3 a3) { ret syscall(cpma_ext.Name, (int)a1, (int)a2, (int)a3); }
#define DEF_TRAP_4(ret, R, Name, A1, A2, A3, A4) R Name(A1 a1, A2 a2, A3 a3, A4 a4) { ret syscall(cpma_ext.Name, (int)a1, (int)a2, (int)a3, (int)a4); }
#define DEF_TRAP_5(ret, R, Name, A1, A2, A3, A4, A5) R Name(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) { ret syscall(cpma_ext.Name, (int)a1, (int)a2, (int)a3, (int)a4, (int)a5); }
#define DEF_TRAP_6(ret, R, Name, A1, A2, A3, A4, A5, A6) R Name(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) { ret syscall(cpma_ext.Name, (int)a1, (int)a2, (int)a3, (int)a4, (int)a5, (int)a6); }
#endif

#ifdef Q3_VM
#define GET_TRAP(Name) \
	do { \
		if (trap_GetValue(syscallStr, sizeof(syscallStr), #Name) && \
			sscanf(syscallStr, "%d", &syscallId) == 1 && \
			syscallId != 0) { \
			cpma_ext.Name = syscallId; \
			Name = (Name##_t)(~syscallId); \
		} \
	} while (0)
#else
#define GET_TRAP(Name) \
	do { \
		if (trap_GetValue(syscallStr, sizeof(syscallStr), #Name) && \
			sscanf(syscallStr, "%d", &syscallId) == 1 && \
			syscallId != 0) { \
			cpma_ext.Name = syscallId; \
		} \
	} while (0)
#endif

#define GET_CMD(Name) \
	do { \
		if (trap_GetValue(syscallStr, sizeof(syscallStr), #Name) && \
			syscallStr[0] == '1') { \
			cpma_ext.Name = qtrue; \
		} \
	} while (0)

#define GET_CAP(Name) GET_CMD(Name)
