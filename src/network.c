/*
 * network.c
 *
 *  Created on: Dec 7, 2018
 *      Author: wael
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "util.h"
#include "ztError.h"

#include "getdiff.h"

/* checkURL(): verify url pointed to by name as valid host name, writes ip in ipStr.
 * source for the function: Beej's Guide to Network Programming.
 * function retrieves a list of ips for url, then try to connect one by one until
 * a successful connection.
 * parameters:
 * 		name: pointer to server to verify; www.someserver.com or 123.456.789.255
 *     service: protocol as string, default value is "http"
 *     ipStr: pointer where ip string is written to.
 ******************************************************************************/
int checkURL(const char* name, char *service, char** ipStr){

	ASSERTARGS(name);

	struct addrinfo hints, *res, *pmover;
	int result;
	char ipstr[INET6_ADDRSTRLEN];
	char *serviceArg = "http";
	//char s[INET6_ADDRSTRLEN];
	int		sockfd;

	/* prototype:
	int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP
	                const char *service,  // e.g. "http" or port number
	                const struct addrinfo *hints,
	                struct addrinfo **res);
	*/

	if (service && strlen(service)) //caller passed service, use it instead of "http".

		serviceArg = service;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; //AF_UNSPEC; AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;

	if ((result = getaddrinfo(name, serviceArg, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
		fprintf(stderr, "%s: Error could not retrieve IP address for specified server: %s\n\n",
				progName, name);
	    return ztParseNoIP;
	}

	//printf("checkURL(): IP addresses for %s:\n\n", name);

	for(pmover = res; pmover != NULL; pmover = pmover->ai_next) {
		void *addr;
	    char *ipver;

	    // get the pointer to the address itself,
	    // different fields in IPv4 and IPv6:
	    if (pmover->ai_family == AF_INET) { // IPv4
	    	struct sockaddr_in *ipv4 = (struct sockaddr_in *)pmover->ai_addr;
	    	addr = &(ipv4->sin_addr);
	    	ipver = "IPv4";
	    }
	    else { // IPv6

	    	struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)pmover->ai_addr;
	    	addr = &(ipv6->sin6_addr);
	    	ipver = "IPv6";

	    }
	    // convert the IP to a string and print it:
	    inet_ntop(pmover->ai_family, addr, ipstr, sizeof ipstr);
	    printf("checkURL()  %s: %s\n\n", ipver, ipstr);
	}

	// loop through all the results and connect to the first we can
	for(pmover = res; pmover != NULL; pmover = pmover->ai_next) {
		if ((sockfd = socket(pmover->ai_family, pmover->ai_socktype,
	         pmover->ai_protocol)) == -1) {
	         	 perror("client: socket");
	         	 continue;
	    }

	    if (connect(sockfd, pmover->ai_addr, pmover->ai_addrlen) == -1) {
	            close(sockfd);
	            perror("client: connect");
	            continue;
	    }

	    break;
	}

	if (pmover == NULL) {
	    fprintf(stderr, "checkURL(): failed to connect to: %s\n", name);
	    return ztNoConnError;
	}

	//we have a connection; copy ipstr into parameter ipStr
	inet_ntop(pmover->ai_family, get_in_addr((struct sockaddr *)pmover->ai_addr),
	            ipstr, sizeof ipstr);

	*ipStr = strdup(ipstr);
	if (ipStr == NULL){
		printf("checkURL(): Error allocating memory; strdup() failed!\n");
		return ztMemoryAllocate;
	}

	//printf("client: connecting to %s\n", ipstr);

	freeaddrinfo(res); // free the linked list
	close(sockfd);

	return ztSuccess;

} //END checkURL()

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* checkPort(): checks numeric string port if it is a valid port number
 * converts string to long and places its value into value.
 */
int checkPort (const char* port, uint16_t *value){

	long num;
	char* end;

	uint16_t MAX_PORT = 65535;

	num = strtol (port, &end, 10);

	if (*end != '\0')
		/* The user specified non digits in the port number. */
		return ztInvalidArg;

	if (num < 1 || num > MAX_PORT) //out of range

		return ztOutOfRangePara;

	*value = (uint16_t)num;

	return ztSuccess;
}

