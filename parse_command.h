#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

int parse_mp(char* source, char* ip_addr, char* msg){
	// Exemple de source = "/mp 192.168.0.2 Coucou toi !";

	regex_t re;
	regmatch_t groups[3];

	int err = regcomp(&re, "/mp ([0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}) (.*)", REG_EXTENDED);
	if(err != 0) {
		perror("Erreur compilation regex");
		return -1;
	}

	int match = regexec(&re, source, 3, groups, 0);
	if(match == REG_NOMATCH) {
		return 0;
	}

	size_t maxGroups = 3;
	size_t g = 0;
	short group_not_found = 0;
	while(g < 3 && !group_not_found){
		size_t start = groups[g].rm_so;
		size_t end = groups[g].rm_eo;
		if(start == (size_t)-1) {
			group_not_found = 1;
		} else {
			switch(g) {
				case 1:
					strncpy(ip_addr, source+start, end-start);
					break;
				case 2:
					strncpy(msg, source+start, end-start);
					break;
				default:
					break;
			}
		}
		g++;
	}

	regfree(&re);
	if(group_not_found) return 0;
	else return 1;
}
