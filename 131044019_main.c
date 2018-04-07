/**************************************************************************************************************/
/* Gozde DOGAN - 131044019                                                                                    */
/*                                                                                                            */
/* BIL 344 - System Programming Homework 2                                                                    */
/* signal and processes                                                                                       */
/*                                                                                                            */
/* 6 Nisan 2018                                                                                               */
/*                                                                                                            */
/* DEBUG mode eklenmistir. 																                      */
/* 20. satir yorum icinden cikartildiginda ekranda gorulmesi istenemyen seyler gorulur. 					  */
/**************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <signal.h>

//#define DEBUG 
#define MAXSIZE 100
#define PI 3.14


/***************************************** Function Prototypes *****************************************/
void multiprocessing(); 
/* dosya islemlerinin yapildigi ve multiprocess_DFT fonksiyonunun cagrildigi fonksiyon */

void multiprocess_DFT();
/* fork isleminin yapildigi fonksiyon */

void producedNumbers(int n, char fileName[]);
/* satir icin random sayi ureten fonksyiyon*/

void producedNumsCastString(int arr[], int n);
/* satir icin uretilmis random sayilari string'e ceviren fonksiyon */

void freeArr2D(int **arr, int size);
/* 2 boyutlu array'i free eden fonksiyon */

void updateFile();
/* Uretilen sayilarin yazildigi ve okundugu dosyayi guncelleyen fonksiyon */

void handler_SIGINT(int SignalID);
/* CTRL-C sinyali yakalandiginda calisan fonksiyon */

void handler_PRODUCE(int SignalID);
/* gonderilen urettim sinyali yakalandiginda calisan fonksiyor */

void handler_READ(int SignalID);
/* gonderilen okudum sinyali yakalandiginda calisan fonksiyon */
/********************************************************************************************************/

/**** GLOBAL VARIABLES ****/
FILE *fpInp;					/* input dosyasi */
int n = 0;						/* arguman olarak girilen satirdaki eleman sayisi = sutun sayisi */
int m = 0;						/* arguman olarak girilen satir sayisi */
char fileName[MAXSIZE];			/* arguman olarak girilen dosya */
int countM = 0;					/* */
char sProducedNums[MAXSIZE];	/* satirdaki elemanlari stringe cevrildiginde tutuldugu string */
int **arrOfProduceNums;			/* uretilen sayilarin tutuldugu integer array */
int indexForProduceArr = 0;		/* uretilen sayilarin tutuldugu integer arrayinin index i*/
int indexForRead = -1;			/* */
struct sigaction sa;			/* CTRL-C sinyali icin tutulan struct */
struct sigaction mask1;			/* PRODUCE sinyali icin tutulan struct */
struct sigaction mask2;			/* READ sinyali icin tutulan struct */
volatile sig_atomic_t mask1Control = 0;	/* PRODUCE sinyali geldiginde degistirilen degisken */
volatile sig_atomic_t mask2Control = 0;	/* READ sinyali geldiginde degistirilen degisken */
sigset_t blockmask;

FILE *fpProcessB;				/* Process B icin output dosyasi isaretcisi */
char childLogFile[MAXSIZE] = "ProcessB_Outputs.log";
FILE *fpProcessA;				/* Process A icin output dosyasi isaretcisi */
char parentLogFile[MAXSIZE] = "ProcessA_Outputs.log";

int OK = 0;
/*************************/



int main(int argc, char *argv[]){

/***************************************** USAGE CONTROL *****************************************/
	if(argc != 7){
		fprintf(stdout, "------------------------------Usage------------------------------\n");
        fprintf(stdout, "./multiprocess_DFT -N #numOfValue -X test.dat -M #stackOfSize\n");
        fprintf(stdout, "------------------------------------------------------------------\n");
        return 0;
	}

	/* Girilen parametrelerin hatalarini kontrol ederek islem yapilir */
	int status1 = 1, status2 = 1, status3 = 1;
	for (int i = 1; i < argc-1; i += 2)
	{
		if (strcmp("-N", argv[i]) == 0 && status1 == 1 && atoi(argv[i+1]) > 0){
			n = atoi(argv[i+1]);
			status1 = 0;
		}
		else if (strcmp("-X", argv[i]) == 0 && status2 == 1){
			sprintf(fileName, "%s", argv[i+1]);
			status2 = 0;
		}
		else if (strcmp("-M", argv[i]) == 0 && status3 == 1 && atoi(argv[i+1]) > 0){
			m = atoi(argv[i+1]);
			status3 = 0;
		}
		else{
			fprintf(stdout, "------------------------------Usage------------------------------\n");
	        fprintf(stdout, "./multiprocess_DFT -N #numOfValue -X test.dat -M #stackOfSize\n");
	        fprintf(stdout, "------------------------------------------------------------------\n");
	        return 0;
		}
	}
/*****************************************************************************************************/

	#ifdef DEBUG
		fprintf(stdout, "n: %d \t m: %d\t fileName: %s\n", n, m, fileName);
	#endif

	multiprocessing(); /* produce ve read islemleri */

	return 0;
}


/*****************************************************************************/
/* log dosyalari acilir.
/* input dosyasi acilir, guncellenir.
/* fork isleminin gerceklestigi fonksiyon cagirilir.
/* uretilen sayilarin tutuldugu array'e dinamik bellek ayrilir.
/*****************************************************************************/
void multiprocessing(){

	fpProcessB = fopen(childLogFile, "w");
	fpProcessA = fopen(parentLogFile, "w");

	fpInp = fopen(fileName, "w");
	fprintf(stdout, "\n");
	fclose(fpInp);

	int i = 0;

	arrOfProduceNums = (int **)calloc(m, sizeof(int*));
	for (int i = 0; i < m; ++i)
		arrOfProduceNums[i] = (int*)calloc(n, sizeof(int));
	
	fprintf(fpProcessA, "Process A\n");
	fprintf(fpProcessB, "Process B\n");

	fclose(fpProcessA);
	fclose(fpProcessB);

	multiprocess_DFT();
	updateFile();

	for (int i = 0; i < m; ++i)
		free(arrOfProduceNums[i]);
	free(arrOfProduceNums);

}


/*************************************************************************************/
/* fork islemi yapilir.
/* CTRL-c sinyali bloklama islemi yapilir.
/* PRODUCE ve READ sinyalleri kontrol edilir. 
/* sayilar uretilir.
/* uretilen sayilar karmasik sayilara cevrilir.
/*************************************************************************************/
void multiprocess_DFT(){
	int status;
	
	/* sa struct'inda CTRL-C sinyali eklendi */
	if ((sigemptyset(&sa.sa_mask) == -1) || (sigaddset(&sa.sa_mask, SIGINT) == -1)){
	   perror("Failed to initialize the signal mask");
	   exit(EXIT_FAILURE);
	}

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &handler_SIGINT; /* sinyal geldiginde calisacak fonksiyon belirlendi */


	/* mask1 struct'inda SIGUSR1 sinyali eklendi */
	if ((sigemptyset(&mask1.sa_mask) == -1) || (sigaddset(&mask1.sa_mask, SIGUSR1) == -1)){
	   perror("Failed to initialize the signal mask");
	   exit(EXIT_FAILURE);
	}

	/* mask2 struct'inda SIGUSR2 sinyali eklendi */
	if ((sigemptyset(&mask2.sa_mask) == -1) || (sigaddset(&mask2.sa_mask, SIGUSR2) == -1)){
	   perror("Failed to initialize the signal mask");
	   exit(EXIT_FAILURE);
	}


	while(!(indexForProduceArr >= m)){
		#ifdef DEBUG
			fprintf(stdout, "\n\n*****indexForProduceArr: %d*****\n\n", indexForProduceArr);
		#endif

		pid_t child_pid = fork();

		if(child_pid == -1){
			perror("fork: ");
			exit(EXIT_FAILURE);
		}
		else if (child_pid == 0){
			//SIGINT'i blokla
			if(indexForProduceArr < m && indexForProduceArr >= 0){ //sonsuz dongu olmali
				
				if (sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL) == -1)
		       		break;

		       	#ifdef DEBUG
			    	fprintf(stderr, "\nSIGINT signal blocked\n\n");
			    #endif


		    	if(sigpending(&sa.sa_mask) == -1)
	    			perror("sigpending");

    			sigfillset (&blockmask);
				mask1.sa_handler = handler_PRODUCE;
				mask1.sa_mask = blockmask;
				mask1.sa_flags = 0;
				sigaction (SIGUSR2, &mask2, NULL);
				while(!mask2Control);

				fpProcessB = fopen(childLogFile, "a+");

				#ifdef DEBUG
					fprintf(stdout, "pid: %d \t parent_pid: %d\n\n", getpid(), getppid());
				#endif

				fprintf(fpProcessB, "pid: %d\tparent_pid: %d\n", getpid(), getppid());

			    #ifdef DEBUG
			    	fprintf(stdout, "<-CHILD: indexForProduceArr: %d\n", indexForProduceArr);	
		    	#endif

		    	fprintf(fpProcessB, "indexForProduceArr: %d\n", indexForProduceArr);

				indexForProduceArr = indexForProduceArr - 1;
				indexForRead -= 1;
				OK = 1;

				#ifdef DEBUG
					fprintf(stdout, "->CHILD: indexForProduceArr: %d\n", indexForProduceArr);	
				#endif

				producedNumsCastString(arrOfProduceNums[indexForProduceArr], n);
				fprintf(stdout, "Process B: the dft of line %d %s is: ", indexForProduceArr+1, sProducedNums );
				fprintf(fpProcessB, "Process B: the dft of line %d %s is: ", indexForProduceArr+1, sProducedNums );

				/*** http://computerk001.blogspot.com.tr/2014/01/dspwrite-c-program-to-find-dft-of-given.html ***/
				double real[100], imag[100];

			    for(int i = 0; i < n; i++)
			    {
			        real[i] = 0.0;
			        imag[i] = 0.0;
			        for(int j = 0; j < n; j++)
			         {
			           real[i] = real[i] + arrOfProduceNums[indexForProduceArr][j] * cos((2*PI*i*(j-n))/n);
			           imag[i] = imag[i] + arrOfProduceNums[indexForProduceArr][j] * sin((2*PI*i*(j-n))/n);
			         }
			         imag[i] = imag[i]*(-1.0);
			    }
                /*************************************************************************************************/ 

			    for(int i = 0; i < n; i++){
			        fprintf(stdout, "(%.2f)+(%.2f)i ", real[i], imag[i]);
			        fprintf(fpProcessB, "(%.2f)+(%.2f)i ", real[i], imag[i]);
			    }
			    fprintf(stdout, "\n");
			    fprintf(fpProcessB, "\n");

				//READ SIGNAL 
				kill(getppid(), SIGUSR1);

				//SIGINT unblock
				if (sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL) == -1)
		       		break;

	       		#ifdef DEBUG
			   		fprintf(stderr, "\nSIGINT signal unblocked\n\n");
				#endif

		   		if(sigaction(SIGINT, &sa, NULL) == -1){
		   			perror("Signal SIGINT: ");
		   		}
		   		fclose(fpProcessB);
				_exit(EXIT_SUCCESS);
			}
		} 
		else{
			// parent

			if(indexForProduceArr < m){ //sonsuz dongu olmali

				if (sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL) == -1)
		       		break;

		       	#ifdef DEBUG
			    	fprintf(stderr, "\nSIGINT signal blocked\n\n");
			    #endif

				if(sigpending(&sa.sa_mask) == -1)
	    			perror("sigpending");

	    		fpProcessA = fopen(parentLogFile, "a+");

				#ifdef DEBUG
					fprintf(stdout, "pid: %d\n\n", getpid());
				#endif

				fprintf(fpProcessA, "pid: %d\n", getpid());

				#ifdef DEBUG
					fprintf(stdout, "<-PARENT: indexForProduceArr: %d\n", indexForProduceArr);
				#endif

				producedNumbers(n, fileName);

				countM++;
				

				fprintf(stdout, "\nProcess A: i’m producing a random sequence for line %d: ", indexForProduceArr+1);
				fprintf(fpProcessA, "Process A: i’m producing a random sequence for line %d: ", indexForProduceArr+1);
				producedNumsCastString(arrOfProduceNums[indexForProduceArr], n);
				fprintf(stdout, "%s\n", sProducedNums);
				fprintf(fpProcessA, "%s\n\n", sProducedNums);


				indexForProduceArr = indexForProduceArr + 1;
				indexForRead += 1;

				#ifdef DEBUG
					fprintf(stdout, "->PARENT: indexForProduceArr: %d\n", indexForProduceArr);
				#endif

				//fprintf(stdout, "Sinyal yollaniyor\n");
				// RODUCE SIGNAL
				if(kill(child_pid, SIGUSR1) == -1)
					perror("kill Signal: ");
				

				sigfillset (&blockmask);
				mask1.sa_handler = handler_READ;
				mask1.sa_mask = blockmask;
				mask1.sa_flags = 0;
				sigaction (SIGUSR1, &mask1, NULL);			
				while(mask1Control);

				// SIGINT unblock
				if (sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL) == -1)
		       		break;

	       		#ifdef DEBUG
			   		fprintf(stderr, "\nSIGINT signal unblocked\n\n");
				#endif

		   		if(sigaction(SIGINT, &sa, NULL) == -1){
		   			perror("Signal SIGINT: ");
		   		}
				
				fclose(fpProcessA);
				waitpid(child_pid, &status, 0);
			}
		}
		if(indexForProduceArr == m)
			kill(getpid(), SIGINT);
	}
}

/****************************************************************************************/
/* Uretilen sayilarin bir satirini string'e cevirir.
/****************************************************************************************/
void producedNumsCastString(int arr[], int n){
	sprintf(sProducedNums, "( ");
	for(int i = 0; i < n; i++)
		sprintf(sProducedNums, "%s%d ", sProducedNums, arr[i]);		// stringe cevrildi
	sprintf(sProducedNums, "%s)", sProducedNums);
}


/******************************************************************************************/
/* satir icin random sayilar uretir.
/* uretilen bu sayilar arraye atilir.
/******************************************************************************************/
void producedNumbers(int n, char fileName[]){

	for(int i = 0; i < n; i++){
		arrOfProduceNums[indexForProduceArr][i] = rand() % 100;			// uretildi , arraye atildi
	}
	//indexForProduceArr += 1;
	//indexForRead += 1;

	//fprintf(stdout, "PARENT: indexForProduceArr: %d \t indexForProduceArr-1: %d\n", indexForProduceArr, indexForProduceArr);

	#ifdef DEBUG
		fprintf(stdout, "producedNums: \n");

		for(int i = 0; i < n; i++)
			fprintf(stdout, "arr[%d] = %d \n", i, arrOfProduceNums[indexForProduceArr-1][i]);
	#endif
}

/***********************************************************************************************/
/* Array deki degerler dosyaya aktarilir
/***********************************************************************************************/
void updateFile(){
	fpInp = fopen(fileName, "w");
	if(indexForProduceArr-1 != -1){
		for (int i = 0; i <= indexForProduceArr-1; ++i){
			for (int j = 0; j < n; ++j){
				fprintf(fpInp, "%d ", arrOfProduceNums[i][j]);
			}
			fprintf(fpInp, "\n");
		}
	}
	fclose(fpInp);
}

/**************************************************************************************************/
/* Gelen iki boyutlu array free edilir.
/**************************************************************************************************/
void freeArr2D(int **arr, int size){
	for (int i = 0; i < size; ++i)
		free(arr[i]);
	free(arr);
}

/***************************************************************************************************/
/* CTRL-C sinyali yakalandiginda calisir.
/* acik dosyalari kapatir. 
/* input dosyasini update eder.
/***************************************************************************************************/
void handler_SIGINT(int SignalID){

	if(SignalID == SIGINT){
		fprintf(stdout, "\nCTRL-C SIGNAL CAME\n");
		if(fclose(fpProcessA) == -1)
			perror("parentLogFile: ");

		if(fclose(fpProcessB) == -1)
			perror("childLogFile: ");

		if (fpInp = fopen(fileName, "w"))
			updateFile();

		if(fclose(fpInp) == -1)
			perror("fpInp: ");

		if(remove("temp.txt") == -1)
			perror("remove temp.txt: ");

		for (int i = 0; i < m; ++i)
			free(arrOfProduceNums[i]);
		free(arrOfProduceNums);

		_exit(EXIT_SUCCESS);
	}
}

/*****************************************************************************************************/
/* PRODUCE (SIGUSR2) sinyal i geldiginde kontrol degiskenine 1 yazar
/*****************************************************************************************************/
void handler_PRODUCE(int SignalID){
	if(SignalID == SIGUSR2){
		mask1Control = 1;
	}

}

/*****************************************************************************************************/
/* READ (SIGUSR1) sinyal i geldiginde kontrol degiskenine 1 yazar
/*****************************************************************************************************/
void handler_READ(int SignalID){
	if(SignalID == SIGUSR1){
		mask2Control = 1;
	}
}