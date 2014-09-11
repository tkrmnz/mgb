/*
 * lcd.c:
 *	Text-based LCD driver.
 *	This is designed to drive the parallel interface LCD drivers
 *	based in the Hitachi HD44780U controller and compatables.
 *
 * Copyright (c) 2012 Gordon Henderson.
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>
#include <string.h>
//#include <time.h>

#include <wiringPi.h>
#include <lcd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
//#include <netinet/in.h>
//#include <net/if.h>

//unsigned char EncoderRead()
//{
//
//}
int Cnt = 0;
unsigned char CurVal;
unsigned char PrevVal;
unsigned char ChangeStat=0;
int mcnt=0;
 int fd1; 
char BTid[18];


//************************************************
void ItemSelector(unsigned char num){	 
  FILE *fp;
 
  char fBuf[60],vBuf[128];
  char BTidDisp[16],BTnm[18];
  char buf1 [512] ;
  char buf2 [512] ;

		
  unsigned char cnt;
  int ii,jj;
  int i;	

ChangeStat=0;
 fp = fopen("/root/btlist.conf","r");
  if(fp != NULL){

  fBuf[0]=0;
  cnt = 0;
    while((fgets(fBuf,sizeof fBuf, fp) != NULL)&&(cnt<num)){
	
	strncpy(BTid,&fBuf[0],17);//NXT BT addr
	BTid[17]=0;
	for(ii=0,jj=0;ii<17;ii++){
	  if(BTid[ii]!=':'){
	    BTidDisp[jj]=BTid[ii];
	    jj++;
	  }
	}//remove : to create NXT ID
	BTidDisp[jj]=0;
	
//	strcpy(BTnm,buf3);
	strncpy(BTnm,&fBuf[18],16);
	vBuf[8]=0;
//	printf("%s",fBuf);
	printf("%s\n",BTid);
	printf("%s\n",BTidDisp);

	printf("%s\n",BTnm);
	cnt++;

  }
	for(i=0;i<18;i++)if(BTnm[i]==0x0a)BTnm[i]=0x20;

	  sprintf (buf1, "    %s            ",BTnm);

	  sprintf (buf2, "    %s",BTidDisp);
	
	for(ii=0;ii<8;ii++) if(vBuf[ii]<0x20)vBuf[ii]=0x20;
	 lcdPosition (fd1, 0, 0) ;
	 lcdPuts (fd1, buf1);
	 lcdPosition (fd1, 0, 1) ;
	 lcdPuts (fd1, buf2);

}
fclose(fp);

}
//************************************************
int main (void)
{
  FILE *fp2;
  int fd1; 
  int btcnt;	

  char buf2 [64] ;
 
 if (wiringPiSetup () == -1)
    exit (1) ;

  fd1 = lcdInit (2, 16, 4, 11, 10, 0,1,2,3,0,0,0,0) ;

  if (fd1 == -1)
  {
    printf ("lcdInit 1 failed\n") ;
    return 1 ;
  }

pinMode (4, INPUT) ; 	// Pin 4 EncA  
pinMode (5, INPUT) ; 	// Pin 5 EncB
pinMode (6, INPUT) ; 	// Pin 6 EncSw
CurVal = digitalRead(4);
PrevVal = CurVal;

lcdPosition (fd1, 0, 0); lcdPuts (fd1, " GLXP  MoonBots ");
//sleep(1);
lcdPosition (fd1, 0, 0); lcdPuts (fd1, "  Select Robot  ");
lcdPosition (fd1, 0, 1); lcdPuts (fd1, "Then Press Knob ");
//sleep(1);


    btcnt = 0;
    ChangeStat = 0;	

     sprintf (buf2, "Button Pressed") ;

    while(digitalRead (6) ==1)
    {


      CurVal = digitalRead(4);
      if(PrevVal!= CurVal){
	if(CurVal == 0){
	  ChangeStat=1;	
	  if(digitalRead(5)==1){
		Cnt--;
		if(Cnt<0)Cnt=0;
	}
	  else Cnt++;
	}
	PrevVal = CurVal;
      }

      //      CurVal = (digitalRead(4)<<1)|digitalRead(5);

//      sprintf (buf1, "%02d", Cnt);
      if (digitalRead (6) == 1){
	if(Cnt<1)btcnt = 1;
	else btcnt=Cnt;
	if(ChangeStat == 1) ItemSelector(btcnt);
//	lcdPosition (fd1, 0, 1) ;
//	lcdPuts (fd1, buf1);
      }
      else{
	Cnt = 0;
	lcdPosition (fd1, 0, 1) ;
	lcdPuts (fd1,buf2);
      }

      delay (1) ;
    }

  fp2 = fopen("/root/btslct.conf","wb");
  fprintf(fp2,"%s", &BTid[0]);
  fclose(fp2);

lcdPosition (fd1, 0, 0); lcdPuts (fd1, "Done            ");

  return 0 ;
}
