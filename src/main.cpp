
#include <windows.h>
#include <sys\timeb.h>
#include <iostream>
#include "direct.h"
#include "IniFile.h"
#include "Frame.h"
#include "ScalerTop.h"
#include "wdrTop.h"

#define OPENCV_SUPPORT
#define BAYER_TEST
#ifdef OPENCV_SUPPORT
#include <iostream>
#include <fstream>
#include "opencv2\opencv.hpp"
using namespace cv;
#endif
using namespace std;


#define PROBE(name, frame, idx) \
	{ \
	ostringstream oss; \
	if(idx < 0)	\
	oss << dsPath << "\\" << name << ".ppm"; \
	else	\
	oss << dsPath << "\\" << name << '_' << setfill('0') << setw(4) << idx << ".ppm"; \
	frame.storePPM( oss.str() ); \
	}

int getNumCores() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
void get_working_dir(void)
{
	char buf[_MAX_PATH];

	if (_getcwd(buf, sizeof(buf)) == NULL) {
		strerror_s(buf, sizeof(buf), errno);
		fprintf(stderr, "Err.:(%d) - %s: curr. workdir\n", errno, buf);
		exit(EXIT_FAILURE);
	}
	printf("working dir. is %s\n", buf);
}

typedef struct PARAMS_frame
{
    int idx;
	int startFrameIdx;
	int tW;
	int tH;
	int randomSeed;
	int bayer;
	string srcFilename;
	string dstFilename;
	WdrTop * wdrTop;
	int repeat_en;
    Frame frameIn;
	Frame frameOut;
	Frame frameCI;
	Frame frameTemp;
} PARAMS_frame, *pPARAMS_frame;

void ci_frame(Frame &outFrame, /*const*/ Frame &inFrame, int bayer);
DWORD WINAPI myThread(void* parameter)
{
	string dsPath = ".";
    pPARAMS_frame param_c = (pPARAMS_frame)parameter;
	if( param_c->repeat_en ) {
		if (-1 == param_c->tW || -1 == param_c->tH) {
			param_c->tW = param_c->frameIn.getWidth();
			param_c->tH = param_c->frameIn.getHeight();
		}
		//frameIn = frameIn.crop( (frameIn.getWidth()-tW)/2, (frameIn.getHeight()-tH)/2, tW, tH );
		param_c->frameTemp = param_c->frameIn.rolling_crop( param_c->idx*param_c->randomSeed, 0, param_c->tW, param_c->tH );
	}
	else
		param_c->frameTemp = param_c->frameIn;

#ifdef BAYER_TEST
	param_c->wdrTop->run_bayer( param_c->frameOut, param_c->frameTemp, param_c->idx + param_c->startFrameIdx, param_c->bayer);
	ci_frame(param_c->frameCI, param_c->frameOut, param_c->bayer);
	PROBE(param_c->dstFilename, param_c->frameCI, param_c->idx + param_c->startFrameIdx);
#else
	param_c->wdrTop->run( param_c->frameOut, param_c->frameTemp, param_c->idx + param_c->startFrameIdx);
	PROBE(param_c->dstFilename, param_c->frameOut, param_c->idx + param_c->startFrameIdx);
#endif
	return 0;
}
int main(int argc, char**argv)
{
	CIniFile inifile;
	int t_i;
	struct timeb start, end;
#ifdef _WIN64
	printf("64-bit Binary build date: %s %s\n", __DATE__, __TIME__);
#else
	printf("32-bit Binary build date: %s %s\n", __DATE__, __TIME__);
#endif
	get_working_dir();
	int cores = 1;
#if 0
	cores = getNumCores();
#endif
	HANDLE *threadHandle = new HANDLE [cores];
	PARAMS_frame ** params = new pPARAMS_frame [cores];
	for (t_i = 0; t_i < cores; t_i ++) {
        // Allocate memory for thread data.
        params[t_i] = (pPARAMS_frame) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                sizeof(PARAMS_frame));

        if( params[t_i] == NULL ) {
			system("pause");
			return -1;
		}
	}
	memset (threadHandle, 0, sizeof(HANDLE)*cores);

	inifile.Create(argv[1]);
	TOption opt;

	int	processFrames = INT_MAX;
	int	startFrameIdx = 0;
	string srcFilename;
	string dstFilename;
	string dsPath = ".";
	string prefix = "MCDi"; 
	bool randomEn = false;
	int randomSeed = 0;

	int repeat_en = 0;
	int tW, tH;

	if(inifile.SetSection("Global")) {
		if(inifile.GetOption("SRCFilename", opt))
			srcFilename = opt._strValue;
		else{
			cerr << "Error : SRCFilename" << endl;
			exit(-1);
		}
		if(inifile.GetOption("ProbePath", opt))
			dsPath = opt._strValue;
		if(inifile.GetOption("Frames", opt))
			processFrames = opt._nValue;
		if(inifile.GetOption("StartIdx", opt))
			startFrameIdx = opt._nValue;
		if(inifile.GetOption("repeat_en", opt))
			repeat_en = opt._nValue;
		if(inifile.GetOption("target_width", opt))
			tW = opt._nValue;
		if(inifile.GetOption("target_height", opt))
			tH = opt._nValue;
		if(inifile.GetOption("random_en", opt))
			randomEn = (opt._nValue!=0)?true:false;
		if(inifile.GetOption("random_seed", opt))
			randomSeed = opt._nValue;
	}
	else {
		cerr << "Error : Global Section not found" << endl;
		exit(-1);
	}
	if ( 5 == argc ) {
		srcFilename = argv[2];
		dsPath = argv[3];
		prefix = argv[4];
		cout << "processing " << srcFilename << " " << processFrames << " frames" << endl;
	}
	else if (6 <= argc) {
		srcFilename = argv[2];
		dstFilename = argv[3];
		dsPath = argv[4];
		prefix = argv[5];
		if (7 == argc)
			processFrames = stoi(argv[6], NULL, 10);;
		cout << "processing " << srcFilename << " " << processFrames << " frames" << endl;
	}
	//WdrTop * wdrTop = new WdrTop();
	//wdrTop->init( inifile );
#if 0
#ifdef BAYER_TEST
	Frame frameIn(Frame::CS_BAYER12), frameOut(Frame::CS_BAYER12), frameCI(Frame::CS_RGB);
#else
	Frame frameIn(Frame::CS_RGB), frameOut(Frame::CS_RGB);
#endif
#endif
	for (int i = 0; i < cores && i < processFrames; i ++) {
		pPARAMS_frame param_c = params[i];
		//param_c->idx = idx;
		param_c->startFrameIdx = startFrameIdx;
		param_c->tW = tW;
		param_c->tH = tH;
		param_c->randomSeed = randomSeed;
		param_c->bayer = 0;//bayer;
		param_c->srcFilename = srcFilename;
		param_c->dstFilename = dstFilename;
		WdrTop * wdrTop = new WdrTop();
		wdrTop->init( inifile );
		param_c->wdrTop = wdrTop;
		param_c->repeat_en = repeat_en;
		int bayer_bit = 16;	// 12
#ifdef BAYER_TEST
		param_c->frameIn.setType((12 == bayer_bit)? Frame::CS_BAYER12: Frame::CS_BAYER16);
		param_c->frameOut.setType((12 == bayer_bit)? Frame::CS_BAYER12: Frame::CS_BAYER16);
		param_c->frameCI.setType(Frame::CS_RGB);
#else
		param_c->frameIn.setType(Frame::CS_RGB);
		param_c->frameOut.setType(Frame::CS_RGB);
#endif
		param_c->frameIn.readPPM( param_c->srcFilename, param_c->startFrameIdx, bayer_bit, param_c->bayer);
	}
    ftime(&start);
	for( int idx = 0 ; idx < processFrames ; ++idx )
	{
		//idx = 47;
		int bayer = 0;
		// allocate thread
		while (1) {
			for (t_i = 0; t_i < cores; t_i ++) {
				if (NULL == threadHandle[t_i]) {
					break;
				}
			}
			if (t_i < cores)
				break;
			else {
				for (t_i = 0; t_i < cores; t_i ++) {
					if (threadHandle[t_i]) {
						if (WAIT_OBJECT_0 == WaitForSingleObject (threadHandle[t_i], 0)) {
							CloseHandle(threadHandle[t_i]);
							threadHandle[t_i] = NULL;
							//printf ("allocate core %d\n", t_i);
							break;
						}
					}
				}
				if (t_i >= cores)
					Sleep (50);	// sleep for 0.05 sec
			}
		}
		// t_i is current thread index
		pPARAMS_frame param_c = params[t_i];
		param_c->idx = idx;
	    threadHandle[t_i] = CreateThread(NULL, 0, myThread, param_c, 0, NULL);
		continue;
#if 0
		if( randomEn )
		{
			frameIn.create( tW, tH );
			srand(randomSeed + idx);
			for( int y = 0 ; y < tH ; ++y )
				for( int x = 0 ; x < tW ; ++x )
					frameIn(x,y) = PIXEL( rand()&0x3FF, rand()&0x3FF, rand()&0x3FF );
		}
		else
		{
			frameIn.readPPM( srcFilename, /*idx*/ + startFrameIdx, 12, bayer);
			if( repeat_en ) {
				if (-1 == tW || -1 == tH) {
					tW = frameIn.getWidth();
					tH = frameIn.getHeight();
				}
				//frameIn = frameIn.crop( (frameIn.getWidth()-tW)/2, (frameIn.getHeight()-tH)/2, tW, tH );
				frameIn = frameIn.rolling_crop( idx*randomSeed, 0, tW, tH );
			}
		}
#ifdef BAYER_TEST
		wdrTop->run_bayer( frameOut, frameIn, idx + startFrameIdx, bayer);
		ci_frame(frameCI, frameOut, bayer);
		PROBE(dstFilename, frameCI, idx + startFrameIdx);
#else
		wdrTop->run( frameOut, frameIn, idx + startFrameIdx);
		PROBE(dstFilename, frameOut, idx + startFrameIdx);
#endif
#endif
	}
	// sync all thread
	for (t_i = 0; t_i < cores; t_i ++) {
		if (threadHandle[t_i]) {
			WaitForSingleObject (threadHandle[t_i], INFINITE);
			CloseHandle(threadHandle[t_i]);
		}
	}
    ftime(&end);
    int diff = (int) (1000.0 * (end.time - start.time) + (end.millitm - start.millitm));
    printf("\nOperation took %u seconds\n", diff/1000);
	params[0]->wdrTop->dumpParameter(params[0]->frameIn.getWidth(), params[0]->frameIn.getHeight(), dstFilename);
	params[0]->wdrTop->dumpRegister(params[0]->frameIn.getWidth(), params[0]->frameIn.getHeight(), dstFilename);
	for (int i = 0; i < cores && i < processFrames; i ++) {
		pPARAMS_frame param_c = params[i];
		delete param_c->wdrTop;
	}
	delete threadHandle;
	delete params;
	return 0;
}
