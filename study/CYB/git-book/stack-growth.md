# Stack Growth 번역

## Stack Growth

프로젝트 2에서 스택은 `USER_STACK`에서 시작하는 단일 페이지였고, 프로그램의 실행은 이 크기로 제한되었다. 이제는 스택이 현재 크기를 넘어 더 커지면, 필요에 따라 추가 페이지를 할당한다.

추가 페이지는 그것이 "겉보기에" stack access로 보일 때에만 할당하라. stack access와 다른 접근을 구분하려는 heuristic을 설계하라.

사용자 프로그램이 stack pointer 아래의 스택 영역에 쓰는 것은 버그이다. 왜냐하면 전형적인 실제 운영체제는 언제든지 프로세스를 인터럽트하여 "signal"을 전달할 수 있고, 이 과정에서 스택 위의 데이터를 수정할 수 있기 때문이다. 하지만 x86-64의 `PUSH` 명령은 stack pointer를 조정하기 전에 먼저 접근 권한을 검사하므로, stack pointer보다 8바이트 아래에서 페이지 폴트를 일으킬 수 있다.

여러분은 사용자 프로그램의 현재 stack pointer 값을 얻을 수 있어야 한다. 시스템 콜 안이나, 사용자 프로그램이 발생시킨 페이지 폴트 안에서는 각각 `syscall_handler()` 또는 `page_fault()`에 전달되는 `struct intr_frame`의 `rsp` 멤버에서 그 값을 얻을 수 있다. 만약 유효하지 않은 메모리 접근을 감지하기 위해 페이지 폴트에 의존한다면, 커널 안에서 페이지 폴트가 발생하는 또 다른 경우도 처리해야 한다. 프로세서는 예외로 인해 user mode에서 kernel mode로 전환될 때에만 stack pointer를 저장하므로, `page_fault()`에 전달된 `struct intr_frame`에서 `rsp`를 읽으면 사용자 stack pointer가 아니라 정의되지 않은 값을 얻게 된다. 따라서 user mode에서 kernel mode로 처음 전환될 때 `rsp`를 `struct thread`에 저장하는 것과 같은 다른 방법을 마련해야 한다.

stack growth 기능을 구현하라. 이를 위해 먼저 `vm/vm.c`의 `vm_try_handle_fault`를 수정하여 stack growth를 식별해야 한다. stack growth를 식별한 뒤에는, 스택을 확장하기 위해 `vm/vm.c`의 `vm_stack_growth`를 호출해야 한다. `vm_stack_growth`를 구현하라.

```c
bool vm_try_handle_fault (struct intr_frame *f, void *addr,
    bool user, bool write, bool not_present);
```

이 함수는 페이지 폴트 예외를 처리하는 동안 `userprog/exception.c`의 `page_fault`에서 호출된다. 이 함수 안에서, 해당 페이지 폴트가 stack growth를 위한 유효한 경우인지 아닌지를 검사해야 한다. 만약 이 fault가 stack growth로 처리될 수 있다고 확인되면, fault가 난 주소를 인자로 하여 `vm_stack_growth`를 호출하라.

```c
void vm_stack_growth (void *addr);
```

`addr`가 더 이상 fault가 발생하는 주소가 아니게 될 때까지, 하나 이상의 anonymous page를 할당하여 스택 크기를 증가시킨다. 할당을 처리할 때는 반드시 `addr`를 `PGSIZE` 기준으로 round down 해야 한다.

대부분의 운영체제는 스택 크기에 어떤 절대적인 제한을 둔다. 어떤 운영체제는 그 제한을 사용자가 조정할 수 있게 하기도 하는데, 예를 들어 많은 Unix 시스템에서 `ulimit` 명령이 그런 역할을 한다. 많은 GNU/Linux 시스템에서는 기본 제한이 8MB이다. 이 프로젝트에서는 스택 크기의 최대치를 1MB로 제한해야 한다.

이제 모든 stack-growth 테스트 케이스가 통과해야 한다.
