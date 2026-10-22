/*
 * processor_thread.h
 * Purpose : Processor thread API: consumes SensorData from the shared buffer,
 *           computes the rolling average and raises alarms.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include "sms_common.h"

void *processor_thread(void *arg);

#endif /* PROCESSOR_THREAD_H */
