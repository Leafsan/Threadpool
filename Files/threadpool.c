/*
 * Copyright 2021. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위한 교육용으로 제작되었습니다.
 */
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include "threadpool.h"

/*
 * 스레드 풀의 FIFO 대기열 길이와 일꾼 스레드의 갯수를 지정한다.
 */
#define QUEUE_SIZE 10
#define NUMBER_OF_BEES 3

/*
 * 스레드를 통해 실행할 작업 함수와 함수의 인자정보 구조체 타입
 */
typedef struct {
    void (*function)(void *p);
    void *data;
} task_t;

/*
 * 스레드 풀의 FIFO 대기열인 worktodo 배열로 원형 버퍼의 역할을 한다.
 */
static task_t worktodo[QUEUE_SIZE];

static int head;
static int tail;

/*
 * mutex는 대기열을 조회하거나 변경하기 위해 사용하는 상호배타 락이다.
 */
static pthread_mutex_t mutex;

/*
 * 대기열에 새 작업을 넣는다.
 * enqueue()는 성공하면 0, 꽉 차서 넣을 수 없으면 1을 리턴한다.
 */
static int enqueue(task_t t)
{
    pthread_mutex_lock(&mutex);
    if (((tail+1)%QUEUE_SIZE) == head) {
        pthread_mutex_unlock(&mutex);
        return 1;
    }        
    tail = (tail+1)%QUEUE_SIZE;
    worktodo[tail] = t;
    pthread_mutex_unlock(&mutex);
    return 0;
}

/*
 * 대기열에서 실행을 기다리는 작업을 꺼낸다.
 * dequeue()는 성공하면 0, 대기열에 작업이 없으면 1을 리턴한다.
 */
static int dequeue(task_t *t)
{
    task_t tmp;
    pthread_mutex_lock(&mutex);
    if (head == tail)
        return 1;
    head = (head + 1)%QUEUE_SIZE;
    tmp = worktodo[head];
    *t = tmp;
    pthread_mutex_unlock(&mutex);
    return 0;
}

/*
 * bee는 작업을 수행하는 일꾼 스레드의 ID를 저장하는 배열이다.
 * 세마포 sem은 카운팅 세마포로 그 값은 대기열에 입력된 작업의 갯수를 나타낸다.
 */
static pthread_t bee[NUMBER_OF_BEES];
static sem_t *sem;

/*
 * 풀에 있는 일꾼 스레드로 FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 */
static void *worker(void *param)
{
    while(1) {
        task_t temp;
        sem_wait(&sem);

        if (dequeue(&temp) == 0)
            (*temp.function)(temp.data);

    }
    pthread_exit(0);
}

/*
 * 스레드 풀에서 실행시킬 함수와 인자의 주소를 넘겨주며 작업을 요청한다.
 * pool_submit()은 작업 요청이 성공하면 0을, 그렇지 않으면 1을 리턴한다.
 */
int pool_submit(void (*f)(void *p), void *p)
{
    task_t temp_task;
    temp_task.function = f;
    temp_task.data = p;

    if (enqueue(temp_task) == 1)
        return 1;

    sem_post(&sem);
    return 0;
}

/*
 * 각종 변수, 락, 세마포, 일꾼 스레드 생성 등 스레드 풀을 초기화한다.
 */
void pool_init(void)
{
    head = -1;
    tail = -1;
    sem_init(&sem, 0, 0);
    pthread_mutex_init(&mutex, NULL);
    memset(worktodo, 0, sizeof(task_t) * QUEUE_SIZE);
    for (int i = 0; i < NUMBER_OF_BEES; i++)
        pthread_create(&bee[i], NULL, worker, NULL);
}

/*
 * 현재 진행 중인 모든 일꾼 스레드를 종료시키고, 락과 세마포를 제거한다.
 */
void pool_shutdown(void)
{
    sem_destroy(&sem);
    pthread_mutex_destroy(&mutex);
    for (int i = 0; i < NUMBER_OF_BEES; i++)
        pthread_cancel(bee[i]);
    for (int i = 0; i < NUMBER_OF_BEES; i++)
        pthread_join(bee[i], NULL);
}
