#include "ap_int.h" // For arbitrary precision data types
#include "hls_stream.h" // For Streams/FIFOs

// This header includes all the Parameter and Function Declarations.
// It also includes Data Type Definitions.
// The Function Definitions are in the respective files.

// Problem Defined Parameters (v1)
#define NKX 4
#define NKY 4
#define NIF 3
#define NOF 6
#define NIX 20
#define NIY 20
#define NOX ((NIX-NKX)/S + 1) // 9
#define NOY ((NIY-NKY)/S + 1) // 9
#define TIF NIF
#define TOF 3
#define TOX NOX //9
#define TOY 3
#define TIX ((TOX-1)*S + NKX) // =NIX=20, but 18 because of zero padding
#define TIY ((TOY-1)*S + NKY) // 8, but 6 because of zero padding
#define PIF 1
#define POF 3
#define POX 3
#define POY 3
#define PIX POX
#define PIY POY
#define S 2
#define ZERO_PAD 1
#define POOL_FILTER_X 2
#define POOL_FILTER_Y 2

// VGG-16 Layer 3 Parameters
//#define NKX 3
//#define NKY 3
//#define NIF 64
//#define NIX 224
//#define NIY 224
//#define NOF 128
//#define NOX ((NIX-NKX)/S + 1) // 112
//#define NOY ((NIY-NKY)/S + 1) // 112
//#define TIF NIF
//#define TIX ((TOX-1)*S + NKX) // 18
//#define TIY ((TOY-1)*S + NKY) // 18
//#define TOF 3
//#define TOX NOX
//#define TOY 3
//#define PIF 1
//#define POF 3
//#define POX 3
//#define POY 3
//#define PIX POX
//#define PIY POY
//#define S 1
//#define ZERO_PAD 1

// Define ceiling calculations of division
#define my_ceil( a, b ) ( (a%b) ? a/b + 1 : a/b )

//// Define Data Types
//typedef ap_uint<1> data_bool;
//typedef  ap_uint<16> px_data_t; //ap_uint<16>
//typedef ap_uint<16> wt_data_t; //ap_uint<16>

// For better view at debugging
typedef ap_uint<1> data_bool;
typedef  int px_data_t; //ap_uint<16>
typedef int wt_data_t; //ap_uint<16>
typedef hls::stream<px_data_t,2> px_stream;
typedef hls::stream<wt_data_t,2> wt_stream;
typedef hls::stream<px_data_t,2> rslt_stream;

// Buffer Sizing
#define OUTBUF_NUM 2
// const int INTERNAL_REG_SIZE = POX*S+ZERO_PAD;
	// Input Buffer
const int WRD_1ROW = my_ceil( (TIX-2*ZERO_PAD), POX ); // 6
const int ROWS_1MAP = my_ceil(my_ceil(TIY,S),POY)*S; // 4
const int WRD_INBUF = WRD_1ROW * ROWS_1MAP * TIF; // 6*4*3=72
	// Weight Buffer
const int WRD_WTBUF = NKX * NKY * TIF * my_ceil(TOF,POF);
	// Output Buffer
const int WRD_OUTBUF = my_ceil(TOF, OUTBUF_NUM) * TOY * my_ceil(TOX, POX); //18
const int WRD_1ROW_O = my_ceil(TOX, POX); //3
// Steps
const int STEP_OUTBUF_POY = my_ceil(TOX,POX);
const int STEP_OUTBUF_POF_OUTBUFNUM = TOY*my_ceil(TOX,POX);

// Input and Output Buffer Writing Pattern for 1 Tix line
// Tix=18, Pox=3
const int col[TIX] = {0,1,2,
		0,1,2,
		0,1,2,
		0,1,2,
		0,1,2,
		0,1,2,
		0,1};
const int row[TIX] = {0,0,0,
		1,1,1,
		2,2,2,
		3,3,3,
		4,4,4,
		5,5,5,
		6,6};

// They are used for loading px from InBuf from BUF2PE
// They are dependent on POX, NKX, ZERO_PAD, Stride
// Here POX=3, NKX=4, ZERO_PAD=1, STRIDE=2
//const int InBuf_row[NKX][POX] =
//{
//		{-1,0,1},
//		{0,0,1},
//		{0,1,1},
//		{0,1,2}
//};

//const int InBuf_col[NKX][POX] =
//{
//		{2,1,0},
//		{0,2,1},
//		{1,0,2},
//		{2,1,0}
//};

// It is made to navigate correct position on InBuf while writing iterating over writing Tiy (lines of Tix length)
// These change based on Tiy, Poy and Stride
// Here is for Tiy=20, Poy=3, S=2
//const int InBuf_banks[TIY] = {
//	0, 0,
//	1, 1,
//	2, 2,
//	0, 0,
//	1, 1,
//	2, 2,
//	0, 0,
//	1, 1,
//	2, 2,
//	0,0
//};
//const int InBuf_rows[TIY] = {
//	0, WRD_1ROW,
//	0, WRD_1ROW,
//	0, WRD_1ROW,
//	2*WRD_1ROW, 3*WRD_1ROW,
//	2*WRD_1ROW, 3*WRD_1ROW,
//	2*WRD_1ROW, 3*WRD_1ROW,
//	4*WRD_1ROW, 5*WRD_1ROW,
//	4*WRD_1ROW, 5*WRD_1ROW,
//	4*WRD_1ROW, 5*WRD_1ROW,
//	5*WRD_1ROW, 6*WRD_1ROW
//};
// Here is for Tiy=8, Poy=3, S=2
const int InBuf_banks[TIY] = {
	0, 0,
	1, 1,
	2, 2,
	0, 0,
};
const int InBuf_rows[TIY] = {
	0, WRD_1ROW,
	0, WRD_1ROW,
	0, WRD_1ROW,
	2*WRD_1ROW, 3*WRD_1ROW
};

// Input Buffer Accessing Pattern
// for Pox=3, S=2, Nkx=4
// col=(Pox_i*S-ZERO_PAD)%POX;
// wrd=(Pox_i*S-ZERO_PAD)/POX;
//const int IB_wrd[NKX][POX] = {
//		{0,0,1},
//		{0,1,1},
//		{0,1,2},
//		{1,1,2}
//};
//const int IB_col[NKX][POX] = {
//		{0,2,1},
//		{1,0,2},
//		{2,1,0},
//		{0,2,1}
//};
// With Padding also (Padding=1)
//const int IB_wrd[NKX][POX] = {
//		{-1,0,1},
//		{0,0,1},
//		{0,1,1},
//		{0,1,2}
//};
//const int IB_col[NKX][POX] = {
//		{2,1,0},
//		{0,2,1},
//		{1,0,2},
//		{2,1,0}
//};


//template< class Type, int n, int k >
//int countOf1( Type (&)[n][k] ) { return n; }
//template< class Type, int n, int k >
//int countOf2( Type (&)[n][k] ) { return k; }
//
//typedef int IB_wrd[NKX][POX];
//typedef int IB_col[NKX][POX];
//typedef int Arr[NKX][POX];
//
//inline Arr& IB_wrdRef(){
//    Arr theArray;
//	for(int Nkx_i=0; Nkx_i<NKX; Nkx_i++){
//		for(int Pox_i=0; Pox_i<POX; Pox_i++){
//			theArray[Nkx_i][Pox_i] = (Pox_i*S-ZERO_PAD+Nkx_i)/POX;
//		}
//	}
//    return theArray;
//}
//inline Arr& IB_colRef(){
//    Arr theArray;
//	for(int Nkx_i=0; Nkx_i<NKX; Nkx_i++){
//		for(int Pox_i=0; Pox_i<POX; Pox_i++){
//			theArray[Nkx_i][Pox_i] = (Pox_i*S-ZERO_PAD+Nkx_i)%POX;
//		}
//	}
//    return theArray;
//}
//
//int tmp_Arr[NKX][POX];
//
//&tmp_Arr = IB_wrdRef();
//
//const Arr& Array_1 = IB_wrdRef();
//const Arr& Array_2 = IB_colRef();
//
//int Arr[3]={1,2,3};
//const int Arr_constant[3] = Arr;
//
//
//
//inline void declare_Input_Buffer_Accessing_Pattern(){
//	int IB_wrd_tmp[NKX][POX];
//	int IB_col_tmp[NKX][POX];
//	for(int Nkx_i=0;Nkx_i<NKX;Nkx_i++){
//		for(int Pox_i=0;Pox_i<POX;Pox_i++){
//			IB_col_tmp[Nkx_i][Pox_i] = (Pox_i*S-ZERO_PAD+Nkx_i)%POX;
//			IB_wrd_tmp[Nkx_i][Pox_i] = (Pox_i*S-ZERO_PAD+Nkx_i)/POX;
//		}
//	}
//}


// Function Declarations

// ******  NEW  ******
void CNN_Layer_top(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
	wt_data_t WtMap[NIF][NOF][NKX][NKY],
	px_data_t OfMap[NOF][NOX][NOY]);
void CNN_Layer_dfl(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
	wt_data_t WtMap[NIF][NOF][NKX][NKY],
	px_data_t OfMap[NOF][NOX][NOY]);
void load_IfMap(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
	int IfMap_x_Address, data_bool east_pad, data_bool west_pad,
	px_data_t InBuf[POY][WRD_INBUF][POX]);
void load_WtMap(wt_data_t WtMap[NIF][NOF][NKX][NKY],
	wt_data_t WtBuf[WRD_WTBUF][POF]);
void compute_Maps(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
	px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);
void store_Maps(px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX],
	px_data_t OfMap[NOF][NOX][NOY]);
void Control_Logic(int *InBuf_Address_Base,
		int *WtBuf_Address_Base, data_bool *west_pad, data_bool *east_pad);
void Control_Logic_2(int *crnt_wrd, int *crnt_bank);
void Window_Calculation(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		int InBuf_Address_Base, int WtBuf_Address_Base, data_bool west_pad, data_bool east_pad,
		px_data_t rslt_stream[POF][POY][POX]);
void PE2BUF(px_data_t rslt_stream[POF][POY][POX],
			int crnt_wrd, int crnt_bank,
			px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);
void INBUF2PE_shell(px_data_t InBuf[POY][WRD_INBUF][POX],
		int InBuf_Address_Window, data_bool west_pad, data_bool east_pad,
		px_data_t px_stream[POY][POX]);
void INBUF2PE(px_data_t InBuf[POY][WRD_INBUF][POX], hls::stream<px_data_t> fifo_arr[POY-1][POX],
	int InBuf_Address_Window, data_bool west_pad, data_bool east_pad,
	px_data_t px_stream[POY][POX]);
//void INBUF2PE(px_data_t InBuf[POY][WRD_INBUF][POX],
//	int InBuf_Address_Window, data_bool west_pad, data_bool east_pad,
//	px_data_t px_stream[POY][POX]);
void WTBUF2PE(wt_data_t WtBuf[WRD_WTBUF][POF],
	int WtBuf_Address_Window,
	wt_data_t wt_stream[POF]);
void PE(px_data_t px_stream[POY][POX], wt_data_t wt_stream[POF],
		px_data_t rslt_stream[POF][POY][POX]);
void BUF2InternalReg(px_data_t InBuf[POY][WRD_INBUF][POX], 
		int InBuf_Address_Window, data_bool west_pad, data_bool east_pad,
		hls::stream<px_data_t> fifo_arr[POY-1][POX],
		px_data_t InternalReg[POY][POX]);
void Reg2FIFO_PE(px_data_t InternalReg[POY][POX], hls::stream<px_data_t> fifo_arr[POY-1][POX],
		px_data_t px_stream[POY][POX]);
void InBuf_read_Controller(int InBuf_Address_Window, data_bool west_pad, data_bool east_pad, int InBuf_Address[POY], data_bool *state_Buf2InternalReg, int *mux_state,
	data_bool *condition1, data_bool *condition2, int *bank,
	data_bool *state_Reg2FIFO_PE);
void BUF2InternalReg_Controller(int InBuf_Address_offset[POY],
			data_bool *state_Buf2InternalReg, int *mux_state, 
			data_bool *condition1, data_bool *condition2, int *bank);
void InBuf_read_indexing(int Nky_i, int Nkx_i,
						int *rows, int *bank, int *s_rows);
void InBuf_write_indexing(int *InBuf_rows_count, int *InBuf_rows_precompute,
		int *InBuf_banks_precompute);
void WtBuf_write_indexing(int *Buf_bank, int *Wrd_base);
void OutBuf_read_indexing(int *OutBuf_rows_precompute,
		int *OutBuf_banks_precompute);

// ******  OLD  ******
void PE(hls::stream<px_data_t> rslt[POF][POY][POX], px_data_t px[POY][POX],
		hls::stream<wt_data_t> wt[POF]);
void BUF2PE_wo_Shift(px_data_t InBuf[POY][WRD_INBUF][POX], px_data_t to_PE[POY][POX],
		int Nif_i, int Nky_i, int Nkx_i, data_bool west_pad,
		data_bool east_pad, data_bool* start, data_bool* end,
		int InBuf_address, int tmp_Wt_addr, int* tmp_Wt_addr_final);
void Parallel_Calc(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		px_data_t rslt[POF][POY][POX], data_bool west_pad, data_bool east_pad,
		int Wt_address, int InBuf_Wrd);
void PE2BUF(px_data_t PE_results [POF][POY][POX], px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX],
		int crnt_wrd, int crnt_bank, data_bool Pof_last, data_bool Poy_last, data_bool Pox_last);
void PE2BUF_v2(px_data_t PE_results [POF][POY][POX], px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX],
		int crnt_wrd, int crnt_bank, data_bool Pof_last, data_bool Poy_last, data_bool Pox_last);
void BUF_Calc(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);
//void IF2BUF(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD], px_data_t InBuf[POY][WRD_INBUF][POX],
//		int IfMap_y_Address, data_bool north_pad, data_bool south_pad);
void IF2BUF(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		px_data_t InBuf[POY][WRD_INBUF][POX],
		int IfMap_y_Address, int IfMap_x_Address,
		data_bool north_pad, data_bool south_pad, data_bool east_pad, data_bool west_pad);
void WT2BUF(wt_data_t WtMap[NIF][NOF][NKX][NKY], wt_data_t WtBuf[WRD_WTBUF][POF],
		int WtMap_no_Address);
void BUF2OF(px_data_t OfMap[NOF][NOX][NOY], px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX],
		int OfMap_no_Address, int OfMap_y_Address, int OfMap_x_Address);
void CNN_Layer(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		wt_data_t WtMap[NIF][NOF][NKX][NKY],
		px_data_t OfMap[NOF][NOX][NOY]);

// Function Declarations for Testing
int Print_Check_Parameters();
void print_ComparedMap(float Compared[NOF][NOX][NOY]);
void print_OutBuf(px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);
void print_OfMap(px_data_t OfMap[NOF][NOX][NOY]);
void print_WtMap(wt_data_t WtMap[NIF][NOF][NKX][NKY]);
void print_WtBuf(wt_data_t WtBuf[WRD_WTBUF][POF]);
void print_InBuf(px_data_t InBuf[POY][WRD_INBUF][POX]);
void print_IfMap(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD]);
void print_InBuf(px_data_t InBuf[POY][WRD_INBUF][POX]);
void print_OutBuf(px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);

void IF2BUF_software(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		px_data_t InBuf[POY][WRD_INBUF][POX],
		int IfMap_y_Address, data_bool north_pad, data_bool south_pad);
void WT2BUF_software(wt_data_t WtMap[NIF][NOF][NKX][NKY],
		wt_data_t WtBuf[WRD_WTBUF][POF]);
void BUF2OF_software(px_data_t OfMap[NOF][NOX][NOY],
		px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]);
void IF2BUF_golden(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		px_data_t InBuf[POY][WRD_INBUF][POX],
		int IfMap_y_Address, data_bool north_pad, data_bool south_pad);
void IF2BUF_test();
void WT2BUF_test();
void BUF2OF_test();



// Parameters Testbench Functions
//int Print_Check_Parameters();
//// Memory to Buffer Transfer Testbench Functions
//void print_IfMap(px_data_t IfMap[Nif][Nix][Niy]);

// Tile Testbench Functions
