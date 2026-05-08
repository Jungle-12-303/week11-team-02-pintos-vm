#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/*
 * 카운팅 세마포어.
 */
struct semaphore {
	/*
	 * 현재 값.
	 */
	unsigned value;
	/*
	 * 기다리는 스레드들의 리스트.
	 */
	struct list waiters;
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/*
 * 락.
 */
struct lock {
	/*
	 * 락을 들고 있는 스레드(디버깅 목적).
	 */
	struct thread *holder;
	/*
	 * 접근을 제어하는 이진 세마포어.
	 */
	struct semaphore semaphore;
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/*
 * 조건 변수.
 */
struct condition {
	/*
	 * 기다리는 스레드들의 리스트.
	 */
	struct list waiters;
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/*
 * 최적화 배리어.
 *
 * 컴파일러는 최적화 배리어를 가로질러 연산 순서를 재배치하지 않는다.
 * 자세한 내용은 참고서의 "Optimization Barriers"를 보라.
 */
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
