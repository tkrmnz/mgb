#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
  FILE *pp;
  FILE *fp;

  pp = popen("hcitool scan","r");
  fp = fopen("/root/btlist.conf","wb");
  if (pp != NULL) {
	while (1) {
	  char *line;
	  char buf[1000];
//	  char BTaddr[18];
//	  char BTname[17];

	  line = fgets(buf, sizeof buf, pp);
	  if (line == NULL) break;
	  else  if(line[0] != 'S'){
		 printf("%s", line);
		fprintf(fp,"%s", &line[1]);
//		strncpy(BTaddr,&line[1],17);
//		BTaddr[17]='\0';
//		strncpy(BTname,&line[19],16);
//		BTname[16]='\0';
//		printf("%s\n",BTaddr);
//		printf("%s\n",BTname);


	  }
	}
//	fprintf(fp,"%c", 0x00);
	fclose(fp);
	pclose(pp);

	}
  return 0;
}
