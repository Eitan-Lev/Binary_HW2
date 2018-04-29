
//
// This tool counts the number of times a routine is executed and 
// the number of instructions executed in a routine
//

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include "pin.H"

ofstream outFile;

// Holds instruction count for a single procedure
typedef struct RtnCount
{
    string _name;
    string _image;
    ADDRINT _address;
	ADDRINT _image_address;
    RTN _rtn;
    UINT64 _icount;
    struct RtnCount * _next;
} RTN_COUNT;

// Linked list of instruction counts for each routine
RTN_COUNT * RtnList = 0;

// This function is called before every instruction is executed
VOID docount(UINT64 * counter)
{
    (*counter)++;
}
    
const char * StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}

// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID *v)
{
    
    // Allocate a counter for this routine
    RTN_COUNT * rc = new RTN_COUNT;

    // The RTN goes away when the image is unloaded, so save it now
    // because we need it in the fini
    rc->_name = RTN_Name(rtn);
    rc->_image = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
    rc->_address = RTN_Address(rtn);
	rc->_image_address = IMG_Entry(SEC_Img(RTN_Sec(rtn)));
    rc->_icount = 0;

    // Add to list of routines
    rc->_next = RtnList;
    RtnList = rc;
            
    RTN_Open(rtn);
    
    // For each instruction of the routine
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        // Insert a call to docount to increment the instruction counter for this rtn
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_icount), IARG_END);
    }

    
    RTN_Close(rtn);
}


RTN_COUNT* popMaxRoutine(){
	RTN_COUNT *  head = RtnList;
	//INPUT CHECK: EMPTY LIST OR LAST ELEMENT
	if(!head)
		return 0;
	if(!head->_next){
		RtnList=0;
		return head;
	}
	//MAX COUNT INIT
	RTN_COUNT *  rc = RtnList; // list iterator
	UINT64 max_count = rc->_icount;
	RTN_COUNT* max_routine = rc;
	//MAX COUNT SEARCH
	RTN_COUNT * prev = 0; // pointer to the prev node of the maximal element
	while(rc->_next){
		//if next element is bigger, update state
		UINT64 cur_count = rc->_next->_icount;
		 if ( cur_count > max_count){
            max_count = cur_count;
            max_routine = rc->_next;
            prev = rc;
        }
		rc = rc->_next;
	}
	if(prev){ 
		prev->_next = max_routine->_next;
	}else{ //first element is maximal
		RtnList = RtnList->_next;
	}
	
	return max_routine;
	
}


// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID *v)
{
    while (RtnList){
        RTN_COUNT* currentRoutine = popMaxRoutine();

        if (currentRoutine ->_icount > 0) {
			 outFile << "0x"<< hex << currentRoutine->_image_address << dec <<  "," << currentRoutine->_image <<  ","
             << "0x" << hex << currentRoutine->_address << dec << "," << currentRoutine->_name << "," << currentRoutine->_icount << endl;
		}

	}

}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This Pintool counts the number of times a routine is executed" << endl;
    cerr << "and the number of instructions executed in a routine" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}



/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();

    outFile.open("rtn-output.csv");

    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
