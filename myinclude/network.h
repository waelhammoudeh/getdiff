/*
 * network.h
 *
 *  Created on: Dec 7, 2018
 *      Author: wael
 */

#ifndef NETWORK_H_
#define NETWORK_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define URL_NAME_LENGTH 512

int checkURL(const char* name, char *service, char** ipStr);

void *get_in_addr(struct sockaddr *sa);

int checkPort (const char* port, uint16_t *value);

#endif /* SOURCE_NETWORK_H_ */
