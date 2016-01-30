#include<stdio.h>
#include <stdlib.h>
#include  <string.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h>


#define MYTIMER ITIMER_PROF
void start_timer(void) {
	int ret=0;
	struct timeval sched_timer = { 0 , 100000 } ;
    struct itimerval timer;
    timer.it_interval = sched_timer; //copy the timeval for the next timer to be triggered
    timer.it_value    = sched_timer; //this value would be reset to the next value on a timer expiry
	
    if((ret = setitimer(MYTIMER, &timer, NULL)) < 0 ) {
        printf("Error in setting up the timer.Exiting...:%d\n", ret);
		perror("Error");
    }
    return ;
}

void stop_timer(void) {
    struct timeval tv = { 0, 0 }; //zero up the timeval struct
    struct itimerval timer = { tv, tv } ; //load up the itimer with a zero timer
    if(setitimer(MYTIMER,&timer,NULL) < 0) {
        printf("Error in Setting up the Timer:\n");
        exit(1);
    }
}


void setup_signal(int signum,void (*handler)(int) ) {
    struct sigaction sigact ;
    sigset_t set; //the signal set
    sigact.sa_handler = handler; //sigprof handler
    sigfillset(&set); //by default block all signals
    sigdelset(&set,signum); //dont block signum.Allow signum only
    sigdelset(&set,SIGKILL); //dont block signum.Allow signum only
    sigdelset(&set,SIGQUIT); //dont block signum.Allow signum only
    sigdelset(&set,SIGTERM); //dont block signum.Allow signum only
    sigact.sa_mask = set; //copy the signal mask
    sigact.sa_flags =  SA_NOMASK; //amounts to a non maskable interrupt
    if( sigaction(signum,&sigact,NULL) < 0) {
        fprintf(stderr,"Failed to initialise the Signal (%d)\n",signum);
        exit(1);
    }
    return ;
}


#define LL_ADD_HEAD(list, item)                         \
        (item)->link.next = (list)->link.next;          \
        (item)->link.prev = &(list)->link;              \
        (list)->link.next->prev = &(item)->link;        \
        (list)->link.next = &(item)->link;              \


#define LL_ADD_TAIL(list, item)                         \
        (item)->link.next = &(list)->link;              \
        (item)->link.prev = (list)->link.prev;          \
        (list)->link.prev->next = &(item)->link;        \
        (list)->link.prev = &(item)->link;              \


#define     IS_QUEUE_EMPTY(queue)   (((queue)->link.next==(void*)(queue))?1:0)
#define     LL_INIT(list)           (list)->link.next = (list)->link.prev = (void*)(list);


struct __jmp_buf {
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int eip;
};
typedef struct __jmp_buf jmp_buf[1];

typedef struct frame{
    unsigned int eip;
    unsigned int ebp;
}FRAME;

typedef struct link
{
	struct link *prev;
	struct link *next;
}LINK;

typedef struct thread{
	LINK link;
    jmp_buf ctx;
    int (*fun)();
    int *stack;
    char name[20];
}THREAD;


THREAD *current=NULL;

void enqueue_thread(THREAD *t)
{
	if(current == NULL)
	{
		current = t;
		return;
	}
	LL_ADD_TAIL(current, t);
}

THREAD* create_thread(char *name, int (*fun)(THREAD *t, void*ctx), void *ctx ){
    THREAD *t= (THREAD*)malloc(sizeof(THREAD));
    assert(t!=NULL);
    strcpy(t->name, name);
    setjmp(t->ctx);
    t->stack = (unsigned int *) ((char *)malloc(4096) + 4092);
    assert(t->stack != NULL);
	t->stack = t->stack-2;
	t->stack[1] = (int)t;
	t->stack[2] = (int)ctx;

    t->ctx[0].ebp = (unsigned) t->stack;
    t->ctx[0].esp = ((int)t->ctx[0].ebp);
    t->ctx[0].eip = (unsigned) fun;
	LL_INIT(t);
	printf("BP = %x, SP = %x, PC = %x\n", t->ctx[0].ebp, t->ctx[0].esp, t->ctx[0].eip);
	enqueue_thread(t);
    return t;
}

THREAD* init_thread(char *name ){
    THREAD *t= (THREAD*)malloc(sizeof(THREAD));
    assert(t!=NULL);
    strcpy(t->name, name);
	LL_INIT(t);
	printf("BP = %x, SP = %x, PC = %x\n", t->ctx[0].ebp, t->ctx[0].esp, t->ctx[0].eip);
	enqueue_thread(t);
    if(setjmp(t->ctx) != 0)
		return t;
	longjmp(t->ctx,1);

    return t;
}









char* address(int *);
int trace()
{
	int *frame=NULL;
	asm("movl %%ebp, %0;" :"=r"(frame));
    while( frame[0] != 0 ){
        printf("   STACK [%-10s] EBP=%x, EIP=%x\n", address((int*)frame[1]), frame[0], frame[1]);
        frame=(int*)frame[0];
    }
}
THREAD* walkstack(THREAD *t){

    int count=0;
    int ebp = 0; 

	asm("movl %%esp, %0;" :"=r"(ebp));
    FRAME *f = (FRAME*)(((char*)ebp));

    for(count=0;count < 4; count++){
        printf("%d [ %08x : %08x ] [%s]\n",  count, f->ebp, f->eip, address((int*)f->eip));
		f=(FRAME*)(f->ebp);
    }
}


int wait(){
    int i;
    for(i=1;i<0x4FFFFF;i++);
}
int fun_counter(THREAD *t, void *ctx){
    int i=0;
    for(i =0;i<=100000000;i++)
    {
        printf(" [%2s] = %d\n", t->name, i);
        wait();

		if(i%20==0)trace();
    }
}

int fun_display(THREAD *t, void *ctx){
    int i=0;
    for( i =0;i<=10000000;i++)
    {
        printf(" [%2s] = %d\n", t->name, i);
        wait();
    }
}
void schedule_thread(){
	
	if(current == NULL) return;

    if(setjmp(current->ctx) != 0){
        return ;
    }
	current = (THREAD*)current->link.next;

    if(current != NULL){
        printf("[ SCHED ] Thread NAME= %s\n", current->name);
        longjmp(current->ctx,1);
    }

}
void initialise_timer(void) {
    setup_signal(SIGPROF, schedule_thread);
    start_timer();
}

int main(int argc, char *argv[])
{
	THREAD *t1 = NULL;
	THREAD *t2 = NULL;
	THREAD *t3 = NULL;
    initialise_timer();
	init_thread( "MAIN");
	t1 = create_thread("T1", fun_counter, NULL);
    t2 = create_thread("T2", fun_display, NULL);


	int count=0;
	while(1)
    {
		printf("[ MAIN ] count:%d\n", count++);
		wait();
		
    }
}

char* address(int *p)
{

    if(p == (int*)fun_counter) return "fun_counter";
    if(p == (int*)fun_display) return "fun_display";
    if(p == (int*)main) return "main";
    return "unknown";
}
