/*
 * log.h
 *
 *  Created on: Nov 27, 2026
 *      Author: 31204
 */

#ifndef LOG_LOG_H_
#define LOG_LOG_H_



void log_fmt(const char *fmt, ...);

#define LOG(...)  log_fmt(__VA_ARGS__)



#endif /* LOG_LOG_H_ */
