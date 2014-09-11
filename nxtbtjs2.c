/*
 * jstest.c  Version 1.2
 *
 * Copyright (c) 1996-1999 Vojtech Pavlik
 *
 * Sponsored by SuSE
 */

/*
 * This program can be used to test all the features of the Linux
 * joystick API, including non-blocking and select() access, as
 * well as version 0.x compatibility mode. It is also intended to
 * serve as an example implementation for those who wish to learn
 * how to write their own joystick using applications.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <wiringPi.h>
#include <lcd.h>

#define max_message_size 14;

#ifndef __AXBTNMAP_H__
#define __AXBTNMAP_H__


/* The following values come from include/input.h in the kernel
   source; the small variant is used up to version 2.6.27, the large
   one from 2.6.28 onwards. We need to handle both values because the
   kernel doesn't; it only expects one of the values, and we need to
   determine which one at run-time. */
#define KEY_MAX_LARGE 0x2FF
#define KEY_MAX_SMALL 0x1FF

/* Axis map size. */
#define AXMAP_SIZE (ABS_MAX + 1)

/* Button map size. */
#define BTNMAP_SIZE (KEY_MAX_LARGE - BTN_MISC + 1)

/* Retrieves the current axis map in the given array, which must
   contain at least AXMAP_SIZE elements. Returns the result of the
   ioctl(): negative in case of an error, 0 otherwise for kernels up
   to 2.6.30, the length of the array actually copied for later
   kernels. */
int getaxmap(int fd, uint8_t *axmap);

/* Uses the given array as the axis map. The array must contain at
   least AXMAP_SIZE elements. Returns the result of the ioctl():
   negative in case of an error, 0 otherwise. */
int setaxmap(int fd, uint8_t *axmap);

/* Retrieves the current button map in the given array, which must
   contain at least BTNMAP_SIZE elements. Returns the result of the
   ioctl(): negative in case of an error, 0 otherwise for kernels up
   to 2.6.30, the length of the array actually copied for later
   kernels. */
int getbtnmap(int fd, uint16_t *btnmap);

/* Uses the given array as the button map. The array must contain at
   least BTNMAP_SIZE elements. Returns the result of the ioctl():
   negative in case of an error, 0 otherwise. */
int setbtnmap(int fd, uint16_t *btnmap);

#endif


char *axis_names[ABS_MAX + 1] = {
"X", "Y", "Z", "Rx", "Ry", "Rz", "Throttle", "Rudder", 
"Wheel", "Gas", "Brake", "?", "?", "?", "?", "?",
"Hat0X", "Hat0Y", "Hat1X", "Hat1Y", "Hat2X", "Hat2Y", "Hat3X", "Hat3Y",
"?", "?", "?", "?", "?", "?", "?", 
};

char *button_names[KEY_MAX - BTN_MISC + 1] = {
"Btn0", "Btn1", "Btn2", "Btn3", "Btn4", "Btn5", "Btn6", "Btn7", "Btn8", "Btn9", "?", "?", "?", "?", "?", "?",
"LeftBtn", "RightBtn", "MiddleBtn", "SideBtn", "ExtraBtn", "ForwardBtn", "BackBtn", "TaskBtn", "?", "?", "?", "?", "?", "?", "?", "?",
"Trigger", "ThumbBtn", "ThumbBtn2", "TopBtn", "TopBtn2", "PinkieBtn", "BaseBtn", "BaseBtn2", "BaseBtn3", "BaseBtn4", "BaseBtn5", "BaseBtn6", "BtnDead",
"BtnA", "BtnB", "BtnC", "BtnX", "BtnY", "BtnZ", "BtnTL", "BtnTR", "BtnTL2", "BtnTR2", "BtnSelect", "BtnStart", "BtnMode", "BtnThumbL", "BtnThumbR", "?",
"?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
"WheelBtn", "Gear up",
};

#define NAME_LENGTH 128

int nxtSocket;

int Cnt = 0;
unsigned char CurVal;
unsigned char PrevVal;
unsigned char ChangeStat=0;
unsigned char btBusy=0;

//#include "axbtnmap.h"

/* The following values come from include/joystick.h in the kernel source. */
#define JSIOCSBTNMAP_LARGE _IOW('j', 0x33, __u16[KEY_MAX_LARGE - BTN_MISC + 1])
#define JSIOCSBTNMAP_SMALL _IOW('j', 0x33, __u16[KEY_MAX_SMALL - BTN_MISC + 1])
#define JSIOCGBTNMAP_LARGE _IOR('j', 0x34, __u16[KEY_MAX_LARGE - BTN_MISC + 1])
#define JSIOCGBTNMAP_SMALL _IOR('j', 0x34, __u16[KEY_MAX_SMALL - BTN_MISC + 1])

int determine_ioctl(int fd, int *ioctls, int *ioctl_used, void *argp)
{
	int i, retval = 0;

	/* Try each ioctl in turn. */
	for (i = 0; ioctls[i]; i++) {
		if ((retval = ioctl(fd, ioctls[i], argp)) >= 0) {
			/* The ioctl did something. */
			*ioctl_used = ioctls[i];
			return retval;
		} else if (errno != -EINVAL) {
			/* Some other error occurred. */
			return retval;
		}
	}
	return retval;
}

int getbtnmap(int fd, uint16_t *btnmap)
{
	static int jsiocgbtnmap = 0;
	int ioctls[] = { JSIOCGBTNMAP, JSIOCGBTNMAP_LARGE, JSIOCGBTNMAP_SMALL, 0 };

	if (jsiocgbtnmap != 0) {
		/* We already know which ioctl to use. */
		return ioctl(fd, jsiocgbtnmap, btnmap);
	} else {
		return determine_ioctl(fd, ioctls, &jsiocgbtnmap, btnmap);
	}
}

int setbtnmap(int fd, uint16_t *btnmap)
{
	static int jsiocsbtnmap = 0;
	int ioctls[] = { JSIOCSBTNMAP, JSIOCSBTNMAP_LARGE, JSIOCSBTNMAP_SMALL, 0 };

	if (jsiocsbtnmap != 0) {
		/* We already know which ioctl to use. */
		return ioctl(fd, jsiocsbtnmap, btnmap);
	} else {
		return determine_ioctl(fd, ioctls, &jsiocsbtnmap, btnmap);
	}
}

int getaxmap(int fd, uint8_t *axmap)
{
	return ioctl(fd, JSIOCGAXMAP, axmap);
}

int setaxmap(int fd, uint8_t *axmap)
{
	return ioctl(fd, JSIOCSAXMAP, axmap);
}

int init_bluetooth(char *btAddress){
	struct sockaddr_rc addr={0};
	int status;
	nxtSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = (uint8_t) 1;
	str2ba(btAddress, &addr.rc_bdaddr);
	status = connect(nxtSocket, (struct sockaddr *)&addr, sizeof(addr));
	if(status <0){
		perror("Error connecting Bluetooth");
		return status;
	}
	return 0;
}

int nxt_getbattery(void){
	char cmd[4]={0x02, 0x00, 0x00, 0x0b};
//	char reply[max_message_size];
	char reply[14];
	int result;
	int blevel;
	int replylength;
	int error =0;
	if((result = write(nxtSocket, cmd, 4))<0){
		perror("error sending getbatterylevel command");
		return result;
	}
	if((result=read(nxtSocket, reply,2)) <0){
		perror("error receiving getbattrylevel reply");
		return result;
	}
	replylength = reply[0] + (reply[1] *256);
	if((result = read(nxtSocket, reply, replylength))<0){
		perror("error receiving getbattrylevel reply");
		return result;
	}
	if(replylength != result){
		fprintf(stderr,
			"getbatterylevel : length do not match : %d != %d\n",
			replylength, result);
	}
	if(reply[0] != 0x02){
		fprintf(stderr, "getbatterylevel : byte 0 : %x != 0x02\n", reply[0]);
		error = 1;
	}
	if(reply[1] != 0x0b){
		fprintf(stderr, "getbatterylevel : byte 1 : %x != 0x13\n", reply[1]);
		error = 1;
	}
	if(reply[2] != 0x00){
		fprintf(stderr, "getbatterylevel : byte 2, status : %x \n", reply[2]);
		error = 1;
	}
	if(error){
		return -1;
	}
	blevel = reply[3] + (reply[4]*256);
	return blevel;
}

int nxt_move(void){
	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x05,0x02,0x64,0x20,0x00,0x00,0x00,0x00};
	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x05,0x02,0x64,0x20,0x00,0x00,0x00,0x00};
	int result;
	btBusy=1;
	if((result = write(nxtSocket, bmd, 14))<0){
		perror("error sending getbatterylevel command");
	//	return result;
	}

	if((result = write(nxtSocket, cmd, 14))<0){
		perror("error sending getbatterylevel command");
	//	return result;
	}
	btBusy=0;
	return result;
}

int nxt_move_sync(int c,int sync){
	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x07,0x02,0x00,0x20,0x00,0x00,0x00,0x00};
	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x07,0x02,0x00,0x20,0x00,0x00,0x00,0x00};
	char dmd[6]={0x04,0x00,0x80,0x0a,0x01,0x01};
	char emd[6]={0x04,0x00,0x80,0x0a,0x02,0x01};
	int result=0;
	bmd[5]=c;
	cmd[5]=c;
	btBusy=1;

	if(sync){

		if((result = write(nxtSocket, dmd, 6))<0){
			perror("error sending move B motor command");
		}
		if((result = write(nxtSocket, emd, 6))<0){
			perror("error sending move C motor command");
		}
	}

	if((result = write(nxtSocket, bmd, 14))<0){
		perror("error sending move B motor command");
	}
	if((result = write(nxtSocket, cmd, 14))<0){
		perror("error sending move C motor command");
	}
	btBusy= 0;
	return result;
}

int nxt_move_vr(int b, int c){
	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
//	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
//	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	int result=0;
	bmd[5]=b;
	cmd[5]=c;
	btBusy=1;
	if((result = write(nxtSocket, bmd, 14))<0){
		perror("error sending move B motor command");
	//	return result;
		}
	if((result = write(nxtSocket, cmd, 14))<0){
		perror("error sending move C motor command");
	//	return result;
	}
	btBusy= 0;
	return result;
}

int nxt_move_b(int b,int scale){
	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
//	char bmd[14]={0x0c,0x00,0x80,0x04,0x01,0x00,0x07,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	int result=0;
	b=b/scale;
	if(b>100) b=100;
	if(b<-100) b=-100;
	bmd[5]=b;
	btBusy=0;
	if((result = write(nxtSocket, bmd, 14))<0){
		perror("error sending move B motor command");
	//	return result;
		}
	btBusy= 0;
	return result;
}

int nxt_move_c(int c,int scale){
	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
//	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x07,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	int result=0;
	c=c/scale;
	if(c>100) c=100;
	if(c<-100) c=-100;
	cmd[5]=c;
	btBusy=0;
	if((result = write(nxtSocket, cmd, 14))<0){
		perror("error sending move C motor command");
	//	return result;
	}
	btBusy= 0;
	return result;
}

int nxt_move_x(int c,int scale,unsigned char motor){
	char cmd[14]={0x0c,0x00,0x80,0x04,0x00,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
//	char cmd[14]={0x0c,0x00,0x80,0x04,0x02,0x00,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	int result=0;
	c=c/scale;
	if(c>100) c=100;
	if(c<-100) c=-100;
	cmd[5]=c;
	cmd[2]=motor;
	btBusy=1;
	if((result = write(nxtSocket, cmd, 14))<0){
		perror("error sending move C motor command");
	//	return result;
	}
	btBusy= 0;
	return result;
}

int nxt_move_arm(int a){
//	char amd[14]={0x0c,0x00,0x80,0x04,0x00,0x32,0x05,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	char amd[14]={0x0c,0x00,0x80,0x04,0x00,0x32,0x03,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	int result;
	amd[5]=a;
	btBusy=1;
	if((result = write(nxtSocket, amd, 14))<0){
		perror("error sending move arm command");
	//	return result;
	}
	btBusy=0;
	return result;
}

int nxt_start_prog(){
//	char amd[14]={0x0c,0x00,0x80,0x04,0x00,0x32,0x05,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	char amd[18]={0x10,0x00,0x80,0x00,0x77,0x68,0x69,0x74,0x65,0x6c,0x69,0x6e,0x65,0x2e,0x72,0x78,0x65,0x00};
	int result;
	btBusy=1;
	if((result = write(nxtSocket, amd, 18))<0){
		perror("error sending start program command");
	//	return result;
	}
	btBusy=0;
	return result;
}

int nxt_stop_prog(){
//	char amd[14]={0x0c,0x00,0x80,0x04,0x00,0x32,0x05,0x01,0x00,0x20,0x00,0x00,0x00,0x00};
	char amd[4]={0x02,0x00,0x80,0x01};
	int result;
	btBusy=1;
	if((result = write(nxtSocket, amd, 4))<0){
		perror("error sending stop program command");
	//	return result;
	}
	btBusy=0;
	return result;
}


int main (int argc, char **argv)
{
	FILE *fp;
 	char fBuf[60];
	int fd1; 
	char buf2 [64] ;
	
//	int PinA = 0;
//	int PinB = 2;
//	int PinC = 3;
	int fd;
	int i;
	unsigned long  curJsTime=0;
	unsigned long prevJsTime=0;
	int botDir=-1;
	unsigned char armDir=0;



	unsigned char axes = 2;
	unsigned char buttons = 2;
	int version = 0x000800;
	char name[NAME_LENGTH] = "Unknown";
	uint16_t btnmap[BTNMAP_SIZE];
	uint8_t axmap[AXMAP_SIZE];
	int btnmapok = 1;

	int blevel;
//	char btaddress[18]="00:16:53:0e:c9:97"; //old NXT

//	char btaddress[18]="00:16:53:03:ca:76"; //new NXT
	char btaddress[18];

	time_t curr_time, prev_time, diff_time;


        fp = fopen("/root/btslct.conf","r");
    	if(fp != NULL){
    	fBuf[0]=0;
    	while(fgets(fBuf,sizeof fBuf, fp) != NULL){
		strncpy(btaddress,&fBuf[0],18);//NXT BT addr
    	  }
  	}
  	fclose(fp);



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
	sleep(1);
	lcdPosition (fd1, 0, 0); lcdPuts (fd1, "  Connecting to ");
	lcdPosition (fd1, 0, 1); lcdPuts (fd1, "     Robot      ");
	//sleep(1);


	int waitForStart =0;

	if(init_bluetooth(btaddress) <0){
		waitForStart =0;
		close(nxtSocket);
		lcdPosition (fd1, 0, 0); lcdPuts (fd1, "     Robot      ");
		lcdPosition (fd1, 0, 1); lcdPuts (fd1, " Not Connected  ");
		sleep(1);
		return 1;
	}
	else
		waitForStart =1;

	lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Robot       ");
	lcdPosition (fd1, 0, 1); lcdPuts (fd1, "Connected Via BT");
	sleep(1);
//	digitalWrite(PinA, 0);
	printf("bluetooth connected to %s \n", btaddress);

	blevel = nxt_getbattery();
	if(blevel<0){
		waitForStart =0;
		close(nxtSocket);
		lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Robot       ");
		lcdPosition (fd1, 0, 1); lcdPuts (fd1, "  Low Battery   ");
		sleep(1);

		return 1;
	}
	else
		waitForStart =1;

	printf("battery level: %.2f\n",blevel/1000.00);

//	mlevel = nxt_move();

	nxt_move_vr(0,0);

	
/*
	if (argc < 2 || argc > 3 || !strcmp("--help", argv[1])) {
		puts("");
		puts("Usage: jstest [<mode>] <device>");
		puts("");
		puts("Modes:");
		puts("  --normal           One-line mode showing immediate status");
		puts("  --old              Same as --normal, using 0.x interface");
		puts("  --event            Prints events as they come in");
		puts("  --nonblock         Same as --event, in nonblocking mode");
		puts("  --select           Same as --event, using select() call");
		puts("");
		return 1;
	}
*/
	if ((fd = open(argv[argc - 1], O_RDONLY)) < 0) {
		perror("jstest");
		lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Gamepad     ");
		lcdPosition (fd1, 0, 1); lcdPuts (fd1, "   Not Found    ");
		sleep(1);

		return 1;
	}



//	digitalWrite(PinB, 0);

	ioctl(fd, JSIOCGVERSION, &version);
	ioctl(fd, JSIOCGAXES, &axes);
	ioctl(fd, JSIOCGBUTTONS, &buttons);
	ioctl(fd, JSIOCGNAME(NAME_LENGTH), name);

	getaxmap(fd, axmap);
	getbtnmap(fd, btnmap);

	printf("Driver version is %d.%d.%d.\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	/* Determine whether the button map is usable. */
	for (i = 0; btnmapok && i < buttons; i++) {
		if (btnmap[i] < BTN_MISC || btnmap[i] > KEY_MAX) {
			btnmapok = 0;
			break;
		}
	}
	if (!btnmapok) {
		/* btnmap out of range for names. Don't print any. */
		puts("jstest is not fully compatible with your kernel. Unable to retrieve button map!");
		printf("Joystick (%s) has %d axes ", name, axes);
		printf("and %d buttons.\n", buttons);
	} else {
		printf("Joystick (%s) has %d axes (", name, axes);
		for (i = 0; i < axes; i++)
			printf("%s%s", i > 0 ? ", " : "", axis_names[axmap[i]]);
		puts(")");

		printf("and %d buttons (", buttons);
		for (i = 0; i < buttons; i++) {
			printf("%s%s", i > 0 ? ", " : "", button_names[btnmap[i] - BTN_MISC]);
		}
		puts(").");
	}

	printf("Testing ... (interrupt to exit)\n");



	int *axis;
	char *button;
//	int ii;
	struct js_event js;
	int apwr =  0;
	int preApwr=0;
	int mvpwr =  0;
	int preMvpwr = 0;
	int waitForRed=0;
	int motorNotSync = 0;

	axis = calloc(axes, sizeof(int));
	button = calloc(buttons, sizeof(char));
	fcntl(fd, F_SETFL, O_NONBLOCK);//js non-blocking mode, remove for event mode



	lcdPosition (fd1, 0, 0); lcdPuts (fd1, "  Set Gamepad   ");
	lcdPosition (fd1, 0, 1); lcdPuts (fd1, "  Mode to Red?  ");


//	while((!waitForRed)&&(read(fd, &js, sizeof(struct js_event)) != sizeof(struct js_event))) {
	while(!waitForRed){
		read(fd, &js, sizeof(struct js_event));
			if((js.type & ~JS_EVENT_INIT) == JS_EVENT_AXIS){
				axis[js.number] = js.value;
				if((axis[0]!=0)||(axis[1]!=0)||(axis[2]!=0)||(axis[3]!=0)||(axis[4]!=0))
					waitForRed=0;
				else
					waitForRed=1;
			}//if
			fflush(stdout);
		}

//	sleep(2);



	lcdPosition (fd1, 0, 0); lcdPuts (fd1, "  Robot Ready   ");
	lcdPosition (fd1, 0, 1); lcdPuts (fd1, " Gamepad Ready  ");
	sleep(1);
//		digitalWrite(PinC, 0);
//	ii = 0;
	prev_time = time((time_t *)0);
	while(1){
	while(waitForStart){
		curr_time = time((time_t *)0);
		diff_time = curr_time - prev_time;
		if(diff_time >= 1){
			blevel = nxt_getbattery();
			if(blevel<0){
				close(nxtSocket);
				lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Robot       ");
				lcdPosition (fd1, 0, 1); lcdPuts (fd1, "Connection Lost ");
				sleep(1);
				waitForStart=0;
//				return 1;
			}//blevel
			else{
//				sprintf (buf2, "Battery: %.2fV  ",blevel/1000.00);
//				lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Robot       ");
				sprintf (buf2, "Connected: %.2fV",blevel/1000.00);
				lcdPosition (fd1, 0, 0); lcdPuts (fd1, " GLXP  MoonBots ");
				lcdPosition (fd1, 0, 1); lcdPuts (fd1, buf2);
			}
			printf("battery level: %.2f\n",blevel/1000.00);
//			printf("Time diff %ld\n",diff_time);
			prev_time = curr_time;
			diff_time=0;
		}//diff_time

		while (read(fd, &js, sizeof(struct js_event)) == sizeof(struct js_event)) {

		if(!btBusy){

			switch(js.type & ~JS_EVENT_INIT) {
				case JS_EVENT_BUTTON:
	//				ii=0;
					button[js.number] = js.value;

					if(button[8]){
					  if(botDir==1) botDir=-1;
					  else botDir=1;
					}
					if(button[9]){
					  if(armDir) armDir=0;
					  else armDir=1;
					}

	 				if(button[0] && !button[2]) apwr = 30;
					else if(!button[0] && button[2]) apwr = -30;
					else apwr = 0;

					if(armDir){
					  apwr = -1* apwr;
					}

					if(apwr!=preApwr){
					  nxt_move_arm(apwr);
					  preApwr=apwr;
					}


	 				if(button[6] && !button[4] && !button[7] && !button[5]) mvpwr = -20;
	 				else if(!button[6] && button[4] && !button[7] && !button[5]) mvpwr = 20;
	 				else if(!button[6] && !button[4] && button[7] && !button[5]) mvpwr = -100;
	 				else if(!button[6] && !button[4] && !button[7] && button[5]) mvpwr = 100;
					else mvpwr = 0;


					if(mvpwr!=preMvpwr){
					  nxt_move_sync(botDir*mvpwr,motorNotSync);
	                 			motorNotSync=0;
					  preMvpwr=mvpwr;
					}




	//				if(button[0]) nxt_start_prog();
	//				if(button[2]) nxt_stop_prog();

					break;

				case JS_EVENT_AXIS:

					if((js.number == 1)||(js.number==4)){//process only axis 1 and 4
						curJsTime=js.time;

//					if(curJsTime != prevJsTime){
						axis[js.number] = js.value;
//							printf("%d %d %d %d %d\n",axis[1],axis[2],axis[3], axis[4],js.number);

						if(js.number==1) {
//							nxt_move_x(botDir*axis[1],320,1);
//							nxt_move_b(botDir*axis[1],420);
							motorNotSync=1;
							nxt_move_b(botDir*axis[1],520);
						}
						if(js.number==4) {
//							nxt_move_x(botDir*axis[4],320,2);
//							nxt_move_c(botDir*axis[4],420);
							motorNotSync=1;
							nxt_move_c(botDir*axis[4],520);
						}
//						if(botDir)
//					  		nxt_move_vr((axis[1]/320),(axis[4]/320));
//						else
//					  		nxt_move_vr(-1*(axis[1]/320),-1*(axis[4]/320));

//						prevJsTime=curJsTime;
//					}

					}
					break;
			}//switch JS
	
	//				if(botDir)
	//				  nxt_move_vr((axis[1]/320),(axis[4]/320));
	//				else
	//				  nxt_move_vr(-1*(axis[1]/320),-1*(axis[4]/320));

//			ii++;
//		usleep(100000);
		}//if !btBusy
//		usleep(10000);


		}//while js read
		if(errno != EAGAIN){
			perror("\njstest1: error reading");
			return 1;
		}//if error

//		usleep(10000);

	}//waitforstart

	if(digitalRead(6) ==0){
		if(init_bluetooth(btaddress) <0){
			waitForStart =0;
			close(nxtSocket);
			lcdPosition (fd1, 0, 0); lcdPuts (fd1, "    Robot       ");
			lcdPosition (fd1, 0, 1); lcdPuts (fd1, " Not Connected  ");
			sleep(1);
		}
		else
			waitForStart =1;
	}
	else{
			lcdPosition (fd1, 0, 0); lcdPuts (fd1, "   Press Knob   ");
			lcdPosition (fd1, 0, 1); lcdPuts (fd1, "  to Reconnect  ");
	}

	}//while(1)
	printf("jstest2: unknown mode: %s\n", argv[1]);

	close(nxtSocket);

	return -1;
}
