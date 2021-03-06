# 한양대학교 ERICA Operating Systems Project4 보고서
기계공학과 2015040719 최우경


## 기본 설명
이번에는 주어진 골격 코드에서 스레드 풀을 구현하는 프로젝트였다.

## Threadpool

### 요약

이번 프로젝트에서는 세개의 코드가 존재한다.

* client.c: threadpool.c에 있는 함수들을 가져와서 실행하는 것으로 동작한다. 수정할 필요가 없다.
* threadpool.c: 이 소스코드에서 프로젝트가 요청하는 함수들을 구현한다.
* threadpool.h: threadpool.c의 함수들을 담기위한 헤더 파일. 수정할 필요가 없다.

### 알고리즘 요약
각 소스코드는 다음과 같이 동작한다.
* client.c: 스레드 풀을 시작 및 종료시키고, 스레드 풀이 동작 중일 때 스레드 풀에 함수들을 등록한다. 미리 만들어져있는 코드이기에 수정할 부분은 없었다.
* threadpool.c: 등록된 함수들을 원형 버퍼에 등록하고, 들어온 순서대로 worker가 빼내서 실행한다. 버퍼가 가득 차있을 때에는 버퍼 상에 등록하지 않는다.

#### 세부 알고리즘
threadpool.c의 알고리즘을 직접 구현했으니 해당 소스코드의 내용을 설명하기로 한다.

```
static int head;
static int tail;
```
위 두 변수는 원형 버퍼를 구현하기위해서 추가한 것이다.

```
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
```
<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/circle%2001.png">
위는 원형 버퍼가 차있을 때를 그림으로 나타낸 것이다. (출처: https://reakwon.tistory.com/30)

<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/circle%2002.png">
위는 원형 버퍼가 비어있을 때를 그림으로 나타낸 것이다. (출처:https://reakwon.tistory.com/30)

enqueue와 dequeue 모두 버퍼에 접근(critical section)하기에 양쪽 모두 mutex를 지닌 상태에서만 접근할 수 있도록 했다. enqueue와 dequeue가 잘 되었는지 여부는 다음의 알고리즘을 따랐다.
enqueue를 하게 되면, tail이 1 증가하면서 자신이 가리키는 위치에 구조체를 등록한다. 만약, tail이 가리키게 될 위치가 이미 head가 있는 위치라면 위의 그림처럼 해당 버퍼는 가득 찬 것으로 간주하게 된다.
dequeue의 경우에는 그와 반대로, head가 1 증가하면서 자신이 가리키는 위치에 있는 구조체를 구조체 포인터의 참조값에 대입해서 전달한다. 만약, head와 tail의 값이 같다면 위의 그림처럼 해당 버퍼는 빈 것으로 간주하게 되어서 아무런 동작을 하지 않는다.

```
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
```
worker는 동작하는 동안에 스레드 풀에 있는 구조체를 dequeue()를 통해서 꺼내와 실행하게 된다. 단, 실행하기 전에 세마포어인 sem의 값을 확인하고 1 이상일 경우에만 동작한다. 그렇지 않을 경우, 비어있는 버퍼에서 값을 얻어내려고 시도하기에 문제가 생길 수 있다. 그 외에 특별한 점은 없다.
pool_submit는 스레드 풀에 구조체를 등록하는데, enqueue가 실패할 경우 곧바로 종료된다. 그렇지 않을 경우 구조체 등록이 완료 되면, sem의 값을 1 증가 시켜서 worker가 해당 세마포어의 값을 확인하고 동작할 수 있도록 한다.

```
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
```
pool_init는 설명대로 변수, 락(뮤텍스), 세마포어, 스레드를 관리한다. head와 tail은 -1로 초기화 하는데, 이는 enqueue나 dequeue 시 배열 첫번째에 구조체를 등록하거나 제거하기 위함이다. memset을 0으로 하는 것은 스레드 풀의 버퍼 상에 있는 데이터를 초기화하기 위함이다.
pool_shutdown에서는 pool_init에서 실행한 락, 세마포어, 스레드를 종료한다. 스레드의 경우에는 while(1) 구문에 의해서 멈추지 않고 계속 동작하므로 pthread_cancel로 중지 시킨 후에 pthread_join으로 완전히 종료한다.

---
## 소스파일
### 기본 파일 설명
* Makefile: make를 행함. 이하 소스코드 별로 실행 파일을 각각 생성한다.
* client.c: main 함수가 존재한다. threadpool.c에서 함수를 가져와 실행한다.
* threadpool.c: 스레드 풀이 구현되어있다.

### 소스코드
* Makefile
```
#
# makefile for thread pool
#

CC=gcc
CFLAGS=-Wall
PTHREADS=-lpthread

all: client.o threadpool.o
	$(CC) $(CFLAGS) -o client client.o threadpool.o $(PTHREADS)

client.o: client.c
	$(CC) $(CFLAGS) -c client.c $(PTHREADS)

threadpool.o: threadpool.c threadpool.h
	$(CC) $(CFLAGS) -c threadpool.c $(PTHREADS)

clean:
	rm -rf *.o
	rm -rf client

```
소스코드를 -lpthread 옵션으로 컴파일한다.

* client.c
```
/*
 * Copyright 2021. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위한 교육용으로 제작되었습니다.
 */
#include <stdio.h>
#include <time.h>
#include "threadpool.h"

#define NCHAR 256
#define NTASK 32
#define NPOOH 62

char *p[NPOOH] = {
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x2E\x2E\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x57\x24\x24\x24\x24\x24\x75",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x46\x2A\x2A\x2B\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x6F\x57\x24\x24\x24\x65\x75",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x2E\x75\x65\x65\x65\x57\x65\x65\x6F\x2E\x2E\x20\x20\x20\x20\x20\x20\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x65\x57\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x62\x2D\x20\x64\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x57",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2C\x2C\x2C\x2C\x2C\x2C\x2C\x75\x65\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x48\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x7E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x65\x6F\x43\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x43\x22\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x54\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x2A\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x65\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x69\x24\x24\x24\x24\x24\x24\x24\x24\x46\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3F\x66\x22\x21\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x75\x64\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2A\x43\x6F",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x20\x20\x20\x6F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x21\x21\x21\x6D\x2E\x2A\x65\x65\x65\x57\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x66\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x55",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x21\x21\x21\x21\x21\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x20\x54\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2A\x21\x21\x2A\x2E\x6F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x65\x2C\x64\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x3A",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x65\x65\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x43",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x62\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2A\x2A\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x21",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x54\x62\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2A\x75\x4C\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x6F\x2E\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x46\x22\x20\x75\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x65\x6E\x20\x60\x60\x60\x20\x20\x20\x20\x2E\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x42\x2A\x20\x20\x3D\x2A\x22\x3F\x2E\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x57\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x6F\x23\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x52\x3A\x20\x3F\x24\x24\x24\x57\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22\x20\x3A\x21\x69\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x21\x6E\x2E\x3F\x24\x3F\x3F\x3F\x22\x22\x60\x60\x2E\x2E\x2E\x2E\x2E\x2E\x2E\x2C\x60\x60\x60\x60\x60\x60\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x60\x60\x20\x20\x20\x2E\x2E\x2E\x2B\x21\x21\x21",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x2A\x20\x2C\x2B\x3A\x3A\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x2A\x60",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x21\x3F\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x7E\x20\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x7E\x60",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2B\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x20\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x3F\x21\x60",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x27\x20\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x2C\x20\x21\x21\x21\x21",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x27\x20\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x20\x60\x21\x21\x3A",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x2B\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x7E\x7E\x21\x21\x20\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x20\x21\x21\x21\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x2E\x60\x3A\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x3A\x3A\x20\x60\x21\x21\x2B",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x7E\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x2E\x7E\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x2E\x60\x21\x21\x3A",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x7E\x7E\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x20\x3B\x21\x21\x21\x21\x7E\x60\x20\x2E\x2E\x65\x65\x65\x65\x65\x65\x6F\x2E\x60\x2B\x21\x2E\x21\x21\x21\x21\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3A\x2E\x2E\x20\x20\x20\x20\x60\x2B\x7E\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x20\x3A\x21\x3B\x60\x2E\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x75\x20\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x62\x65\x65\x65\x75\x2E\x2E\x20\x20\x60\x60\x60\x60\x60\x7E\x2B\x7E\x7E\x7E\x7E\x7E\x22\x20\x60\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x24\x62",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x55\x55\x24\x55\x24\x24\x24\x24\x24\x20\x7E\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x24\x24\x6F",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2E\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x7E\x20\x24\x24\x24\x75",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x21\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x38\x24\x24\x24\x24\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x58\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x60\x75\x24\x24\x24\x24\x24\x57",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x21\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x21\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22\x2E\x24\x24\x24\x24\x24\x24\x24\x3A",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x2E\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x66\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x27\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2E",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x21",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x21",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x69\x62\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x62\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22\x6F\x24\x24\x24\x62\x2E\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x65\x2E\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x64\x24\x24\x24\x24\x24\x24\x6F\x2E\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x48\x20\x24\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x57\x2E\x60\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x65\x2E\x20\x22\x3F\x3F\x24\x24\x24\x66\x20\x2E\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x64\x24\x24\x24\x24\x24\x24\x6F\x20\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x65\x65\x65\x65\x65\x65\x24\x24\x24\x24\x24\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x62\x75\x20\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x20\x33\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2A\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x64\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x65\x2E\x20\x22\x3F\x24\x24\x24\x24\x24\x3A\x60\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x38",
"\x20\x20\x20\x20\x20\x20\x20\x65\x24\x24\x65\x2E\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x2B\x20\x20\x22\x3F\x3F\x66\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x63",
"\x20\x20\x20\x20\x20\x20\x24\x24\x24\x24\x24\x24\x24\x6F\x20\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x22\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x60\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x62\x2E",
"\x20\x20\x20\x20\x20\x4D\x24\x24\x24\x24\x24\x24\x24\x24\x55\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x22\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x75",
"\x20\x20\x20\x20\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x75",
"\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x6F",
"\x20\x20\x20\x20\x20\x20\x20\x20\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x3F\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x3F\x3F\x24\x24\x24\x24\x24\x24\x24\x46\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x22\x3F\x33\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2E\x65\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x27",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x75\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x60\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x46\x22",
"\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x22\x22\x3F\x3F\x3F\x3F\x3F\x22\x22"
};

/*
 * 어떤 정수 X를 인자로 받아서 NCHAR 갯수 만큼 화면에 출력한다.
 * 출력이 종료되었음을 알리기 위해 마지막에 <X>를 출력한다.
 */
void number(void *param)
{
    int i, num;
    
    num = *(int *)param;
    for (i = 0; i < NCHAR; ++i)
        printf("%d", num);
    printf("<%d>\n", num);
}

/*
 * 아무 것도 안 하는 함수로 대기열에서 자리만 차지하기 위해 사용된다.
 */
void donothing(void *param)
{
    /* do nothing */;
}

/*
 * 디즈니 만화 캐릭터인 곰돌이(푸베어)를 출력하는 함수이다.
 */
void pooh(void *param)
{
    int i;
    
    printf("\n");
    for (i = 0; i < NPOOH; ++i)
        printf("%s\n", p[i]);
}

/*
 * 스레드 풀이 잘 구현되었는지 검증하기 위해 작성된 메인 함수이다.
 */
int main(void)
{
    int i, num[NTASK];
    struct timespec req;
    
    /*
     * 잠자는 시간을 10 msec으로 설정한다.
     */
    req.tv_sec = 0;
    req.tv_nsec = 10000000L;
    /*
     * 스레드 풀을 사용하기 전에 먼저 초기화한다.
     */
    pool_init();
    printf("Thread pool: init() completed.\n");
    /*
     * 스레드 풀이 정상으로 종료되는지 검사한다.
     */
    pool_shutdown();
    printf("Thread pool: shutdown() completed.\n");

    /*
     * 스레드 풀을 다시 가동한다.
     */
    pool_init();
    /*
     * 함수 number()를 스레드를 사용해서 실행하기 위해 NTASK 갯수 만큼 스레드 풀에 요청한다.
     * 스레드 풀의 대기열은 길이가 10개를 넘을 수 없어서 요청을 아무리 빨리 처리해도 받을 수 없는
     * 요청이 발생할 수 있다. 대기열이 차서 요청이 거절되면 오류 메시지를 출력한다.
     */
    for (i = 0; i < NTASK; ++i) {
        num[i] = i;
        if (pool_submit(number, num+i))
            printf("%d: Queue is full.\n", i);
    }
    /*
     * 10 msec 동안 자면서 스레드 풀의 대기열이 비기를 기다린다.
     */
    nanosleep(&req, NULL);
    /*
     * 대기열은 비어 있고, 일하는 스레드가 세 개 있으므로,
     * 아래와 같이 세 개의 작업을 요청하면, 두 개의 작업은 아무 것도 안 하니까
     * 마지막 작업은 아무런 방해 없이 곰돌이 이미지를 화면에 출력하게 된다.
     */
    pool_submit(donothing, NULL);
    pool_submit(donothing, NULL);
    pool_submit(pooh, NULL);
    /*
     * 곰돌이 모습을 다 보기 위해 10 msec 동안 작업이 종료되도록 기다린다.
     */
    nanosleep(&req, NULL);
    /*
     * 스레드 풀을 종료한다.
     */
    pool_shutdown();
    return 0;
}
```

* threadpool.h
```
/*
 * Copyright 2021. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위한 교육용으로 제작되었습니다.
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

int pool_submit(void (*f)(void *p), void *p);
void pool_init(void);
void pool_shutdown(void);

#endif
```

* threadpool.c
```
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
```
---
## 컴파일
<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/01.png">
<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/02.png">
제공된 Makefile로 컴파일을 했다. Warning 내용이 많이 나왔지만, 컴파일 자체는 문제없이 되었다.

## 실행 결과
<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/03.png">
화면 상에서 pool_init()와 pool_shutdown() 양쪽 모두 제대로 동작하는 것으로 나온다.

<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/04.png">
문자열을 확인하면 11번부터 Queue가 꽉 찼음을 알 수 있다. 의도한 것과 알맞게 동작하고 있다.

<img src="https://github.com/Leafsan/Threadpool/blob/master/Report/05.png">
숫자를 출력하는 스레드 풀의 처리가 끝난 후, 푸 그림을 처리하는 것도 문제 없이 동작한다.

## 결론
스레드 풀은 의도한대로 동작한다고 말할 수 있다. 처음에 세마포어 사용을 잘못해서 worker가 빈 주소값에 접근하는 것때문에 꽤 오래 고민했다. 다행히 원형 큐가 내부에 존재하는 자료의 수를 세는 변수를 가지는 경우도 있다는 것을 깨닫고 세마포어를 그와 비슷하게 설정하니 잘 동작하게 되었다.
