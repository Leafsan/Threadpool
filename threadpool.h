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
