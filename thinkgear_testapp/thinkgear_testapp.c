#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "thinkgear.h"

/**
 * Prompts and waits for the user to press ENTER.
 */
void
wait() {
    printf( "\n" );
    printf( "Press the ENTER key...\n" );
    fflush( stdout );
    getc( stdin );
}

/**
 * Program which prints ThinkGear Raw Wave Values to stdout.
 */
int
main( void ) {
    
	/*腦波裝置設定參數*/
    char *comPortName  = NULL;
    int   dllVersion   = 0;
    int   connectionId = 0;
    int   packetsRead  = 0;
    int   errCode      = 0;
	int   rawData      = 0;
	int   attentionData = 0;
	int set_filter_flag = 0;

	/*時間*/
	double secondsToRun = 0;
	time_t startTime = 0;
	time_t currTime = 0;
	int   times = 0; //次數
	char  *currTimeStr = NULL;

	/*訊號處理*/
	char  state[30] = "";
	char  tempstate[10] = "";
	int   T0 = 0;
	int   T1 = 0;

	/*眨眼訊號*/
	char  blinkState[30] = ""; //眨眼狀態
	int th_raw_peak = 0; //眨眼判斷-波峰
	int th_raw_valley = 0; //眨眼判斷-波谷
	int   blinkTimes = 0; //眨眼次數
	int   blinkinterval = 0; //眨眼區間
	int   peakCheck = 0;
	int   blink_t = 0;

	/*專注訊號*/
	int   baseline = 0;
	
	/*狀態文件輸出*/
	FILE *fp;

	/* 確認是否讀取腦波訊號 */
	fp = fopen("../../snapRaspi/SetState.txt","w+");
	fprintf(fp, "setting");
	fclose(fp);

    /* Print driver version number */
    dllVersion = TG_GetVersion();
    printf( "Stream SDK for PC version: %d\n", dllVersion );
    
    /* Get a connection ID handle to ThinkGear */
    connectionId = TG_GetNewConnectionId();
    if( connectionId < 0 ) {
        fprintf( stderr, "ERROR: TG_GetNewConnectionId() returned %d.\n",
                connectionId );
        wait();
        exit( EXIT_FAILURE );
    }
    
    /* Set/open stream (raw bytes) log file for connection */
    errCode = TG_SetStreamLog( connectionId, "streamLog.txt" );
    if( errCode < 0 ) {
        fprintf( stderr, "ERROR: TG_SetStreamLog() returned %d.\n", errCode );
        wait();
        exit( EXIT_FAILURE );
    }
    
    /* Set/open data (ThinkGear values) log file for connection */
    errCode = TG_SetDataLog( connectionId, "dataLog.txt" );
    if( errCode < 0 ) {
        fprintf( stderr, "ERROR: TG_SetDataLog() returned %d.\n", errCode );
        wait();
        exit( EXIT_FAILURE );
    }
    
    /* Attempt to connect the connection ID handle to serial port "COM5" */
    /* NOTE: On Windows, COM10 and higher must be preceded by \\.\, as in
     *       "\\\\.\\COM12" (must escape backslashes in strings).  COM9
     *       and lower do not require the \\.\, but are allowed to include
     *       them.  On Mac OS X, COM ports are named like
     *       "/dev/tty.MindSet-DevB-1".
     */
    comPortName = "COM3"; /*重要: 須設定與腦波的連接埠*/
    errCode = TG_Connect( connectionId,
                         comPortName,
                         TG_BAUD_57600,
                         TG_STREAM_PACKETS );
    
	/*確認是否有連接成功*/
	if( errCode < 0 ) {
        fprintf( stderr, "ERROR: TG_Connect() returned %d.\n", errCode );
        wait();
        exit( EXIT_FAILURE );
    }
	
    /* Keep reading ThinkGear Packets from the connection for 5 seconds... */
    secondsToRun = 2;
    startTime = time( NULL );

	printf("Wait for connecting the Status. It will cost about ten seconds.\n");
    do {
			
            packetsRead = TG_ReadPackets( connectionId, 1 );

	} while ((int)TG_GetValue(connectionId, TG_DATA_ATTENTION) == 0);
	printf("Succeed to receive the status, total times:%f\n", difftime(time(NULL), startTime));
	
	/* measuring the baseline of Attention */
	printf("Measure the baseline of Attention\n");
	do {
		if (times % 10 == 0) {
			packetsRead = TG_ReadPackets(connectionId, 1);
			baseline = (int)TG_GetValue(connectionId, TG_DATA_ATTENTION) + baseline;
		}
		times++;
	} while (times < 40000);
	baseline = baseline / (times/10);
	blinkinterval = 250;
	printf("Baseline,%d\n", baseline);

	/* measuring the thresholding value of blink */
	times = 0;
	printf("measure the thresholding value of blink\n");
	do {
		if (times % 5 == 0) {
			packetsRead = TG_ReadPackets(connectionId, 1);
			rawData = (int)TG_GetValue(connectionId, TG_DATA_RAW);
			if (th_raw_peak < rawData)
				th_raw_peak = rawData;
			else if (th_raw_valley > rawData)
				th_raw_valley = rawData;
		}
		times++;
	} while (times < 40000);
	th_raw_peak = th_raw_peak * 1.2; //設定判斷眨眼的狀態的波峰值
	th_raw_valley = th_raw_valley * 1.2; //設定判斷眨眼的狀態的波谷值

	/* setting the blink parameters 目前給予定值 */
	//if (th_raw_peak < 220)
		th_raw_peak = 250;
	//if (th_raw_valley > -220)
		th_raw_valley = -250;


	 
	printf("Peak:%d, Valley:%d\n", th_raw_peak, th_raw_valley);

	/*已完成所有設定，輸出完成至Snap! */
	fp = fopen("../../snapRaspi/SetState.txt", "w+");
	fprintf(fp, "finish");
	fclose(fp);
	
	// Start to read packet
	printf("auto read test begin:\n");
	fflush(stdin);
	errCode = TG_EnableAutoRead(connectionId,1);

	
	if (errCode == 0) {
		packetsRead = 0;
		errCode = MWM15_setFilterType(connectionId, MWM15_FILTER_TYPE_60HZ);//MWM15_FILTER_TYPE_60HZ
		printf("MWM15_setFilterType: %d\n", errCode);
		while (packetsRead < 5000000) {
			/* If raw value has been updated ... */
			if (TG_GetValueStatus(connectionId, TG_DATA_RAW) != 0) {

				if (packetsRead % 5 == 0) { // too much stdout operation will lose some data (每五筆算一筆資料)
					rawData = (int)TG_GetValue(connectionId, TG_DATA_RAW);
					attentionData = (int)TG_GetValue(connectionId, TG_DATA_ATTENTION);
					
					// detect the blink 
					if (rawData > th_raw_peak && peakCheck == 0) //起始狀態
					{
						peakCheck = 1;
						T0 = packetsRead;
						
					}
					else if (rawData < th_raw_valley && peakCheck == 1) // 判斷是否為一完整眨眼訊號
					{
						if(blinkTimes == 0){
							T1 = packetsRead; //判斷是否為第一次眨眼，藉此判斷時間區段內是否為雙眨眼
						}
						blinkTimes = blinkTimes + 1; //紀錄眨眼次數
						peakCheck = 0;
						currTime = time(NULL);
						sprintf(blinkState, "%d", currTime);
						fp = fopen("../../snapRaspi/BlinkState.txt", "w+"); //紀錄目前時間，判斷是否要暫停
						fprintf(fp, "%s", blinkState);
						fclose(fp);
						printf("blink\n");						
					}
					else if (packetsRead - T0 > blinkinterval)
					{
						peakCheck = 0;
						T0 = packetsRead;
					}

					// check state of motion
					if (blinkTimes > 0)
					{
						if (packetsRead - T1 > blinkinterval) {
							if (blinkTimes >= 2) {
								printf("blink twice\n");
								currTime = time(NULL);
								blink_t = currTime;
								sprintf(state, "%d multiblink", currTime);
								strcpy(tempstate, "multiblink");
							}
							else {
								printf("blink once\n");
								currTime = time(NULL);
								blink_t = currTime;
								sprintf(state, "%d onceblink", currTime);
								strcpy(tempstate, "onceblink");
							}
							T1 = 0;
							blinkTimes = 0;
						}
					}
					// detect the attention
					else if (attentionData - baseline > 2 && strcmp(tempstate,"infocus") != 0 && difftime(time(NULL), blink_t) > 2) {
						currTime = time(NULL);
						sprintf(state, "%d infocus", currTime);
						strcpy(tempstate, "infocus");
					}
					else{
						strcpy(state, "");
						strcpy(tempstate, "");
					}

					// 如果有狀態，則輸出狀態
					if (strcmp(state, "") != 0)
					{
						fp = fopen("../../snapRaspi/BrainWaveState.txt", "w+"); //輸出狀態
						fprintf(fp, "%s", state);
						fclose(fp);
					}
				}
				else {
					TG_GetValue(connectionId, TG_DATA_RAW);
				}
				packetsRead++;
				
				//wait for a while to call MWM15_getFilterType
				if (packetsRead == 800 || packetsRead == 1600) {// at lease 1s after MWM15_setFilterType cmd
					set_filter_flag++;
					errCode = MWM15_getFilterType(connectionId);

					printf(" \nMWM15_getFilterType   result: %d  index: %d\n", errCode, packetsRead);

				}

			}


		}

		errCode = TG_EnableAutoRead(connectionId, 0); //stop
		printf("auto read test stoped: %d \n", errCode);
	}
	else {
		printf("auto read test failed: %d \n", errCode);
	}

	TG_Disconnect(connectionId); // disconnect test
    TG_FreeConnection( connectionId );

    wait();
    return( EXIT_SUCCESS );
}
