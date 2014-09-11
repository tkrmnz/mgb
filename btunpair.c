#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>
#include <string.h>
//#include <time.h>

#include <wiringPi.h>
#include <lcd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

int main(void){
  FILE *fp;
  char stat=0;
int fd1; 

  char fBuf[60];
  char BTid[17];
  char sBuf[512];
  char wBuf[512];


//  FILE *pp;
//  char *Line;
//  char buf[1000];



 if (wiringPiSetup () == -1)
    exit (1) ;

  fd1 = lcdInit (2, 16, 4, 11, 10, 0,1,2,3,0,0,0,0) ;

  if (fd1 == -1)
  {
    printf ("lcdInit 1 failed\n") ;
    return 1 ;
  }
/*
  pp = popen("hcitool dev","r");
  if(pp !=NULL){
    Line = fgets(buf, sizeof buf, pp);
    printf("%s",Line);
    pclose(pp);
  }
*/


  fp = fopen("/root/btslct.conf","r");
    if(fp != NULL){
    fBuf[0]=0;
    while(fgets(fBuf,sizeof fBuf, fp) != NULL){
	strncpy(BTid,&fBuf[0],18);//NXT BT addr
    }
	stat=1;  
  
  fclose(fp);
  }
if(stat){  
	lcdPosition (fd1, 0, 0); lcdPuts (fd1, " GLXP  MoonBots ");
	lcdPosition (fd1, 0, 1); lcdPuts (fd1, "  Initializing  ");
	sleep(1);
  
	printf("%s\n",BTid);
	sprintf(sBuf,"bluez-test-device disconnect %s\n\n",BTid);
  	system(sBuf); 
	sleep(1);
	sprintf(wBuf,"bluez-test-device remove %s\n\n",BTid);
  	system(wBuf); 
	return 0;
	}
  else 
    return -1;
}

