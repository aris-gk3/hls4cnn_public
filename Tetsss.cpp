#include "header.h"
#include <iostream>

void PE(px_data_t rslt[POF][POY][POX], px_data_t px[POY][POX],
		wt_data_t wt[POF]){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=wt complete dim=1

	static px_data_t tmp_rslt[POF][POY][POX]; // Intermediate results
#pragma HLS ARRAY_PARTITION variable=tmp_rslt complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_rslt complete dim=2
#pragma HLS ARRAY_PARTITION variable=tmp_rslt complete dim=3

	// "State" variables to know when each pixel calculation starts and ends
	static int state = 0; static data_bool start = 1; static data_bool end = 0;

	for(int Pof_i=0;Pof_i<POF;Pof_i++){
#pragma HLS UNROLL
		for(int Poy_i=0;Poy_i<POY;Poy_i++){
#pragma HLS UNROLL
			for(int Pox_i=0;Pox_i<POX;Pox_i++){
#pragma HLS UNROLL
				if( start ){
					tmp_rslt[Pof_i][Poy_i][Pox_i] = px_data_t(0);//'0000000000000000';
				}
				tmp_rslt[Pof_i][Poy_i][Pox_i] += px[Poy_i][Pox_i] * wt[Pof_i];
				if( end ){
					rslt[Pof_i][Poy_i][Pox_i] = tmp_rslt[Pof_i][Poy_i][Pox_i];
				}
			}
		}
	}
	// State Calculation
	if(state == NIF*NKX*NKY-1){
		start = 1;
		end = 0;
		state = 0;
	}
	else if(state == NIF*NKX*NKY-2){
		start = 0;
		end = 1;
		state++;
	}
	else{
		start = 0;
		end = 0;
		state++;
	}
}

void InBuf_indexing_read(int *rows, int *bank, int *s_rows){
#pragma HLS INLINE
	static int state_Nkx = 0;static int state_Nky = 0;
	if(state_Nkx==NKX-1 && state_Nky==NKY-1){
		(*rows) = 0;(*bank) = 0;(*s_rows) = 0;
	}
	else if(state_Nky>=S && (state_Nkx==NKX-1)){
		if( (*rows) == S-1){
			(*rows) = 0;
			if(NKY-S>POX){
				if((*bank) == POX-1){ // Here, considering the specific values of parameters, it may be unnecessary.
								   //Probably the compiler doesn't know it. So maybe specific ifs that can be ignored should be given
					(*bank) = 0; (*s_rows)++;
				}
				else{
					(*bank)++;
				}
			}
			else{
				(*bank)++;
			}
		}
		else{
			(*rows)++;
		}
	}
	if(state_Nkx==NKX-1){
		state_Nkx = 0;
		if(state_Nky==NKY-1){
			state_Nky = 0;
		}
		else{
			state_Nky++;
		}
	}
	else{
		state_Nkx++;
	}
}

void BUF2InternalReg(px_data_t InBuf[POY][WRD_INBUF][POX],
		px_data_t InternalReg[POY][POX],
		hls::stream<px_data_t> fifo_arr[POY-1][POX],
		int InBuf_addr_base,
		data_bool west_pad, data_bool east_pad
		//int rows, int bank, int s_rows
		){
#pragma HLS INLINE
	// For Poy=POY-1 we always write from InBuf, but different indexing at Nky>=S.
	// For Poy<POY-1 we read from InBuf for Nky<S and from FIFOs for Nky>=S.
	static int state = 0; // For choosing the routing from InBuf to InternalReg
	static int state_Nif = 0;static int state_Nky = 0;static int state_Nkx = 0; // For knowing in which loop iteration we are currently
	px_data_t tmpInBuf0[POY];
#pragma HLS ARRAY_PARTITION variable=tmpInBuf0 type=complete
	px_data_t tmpInBuf1[POY];
	px_data_t tmpInBuf2[POY];
#pragma HLS ARRAY_PARTITION variable=tmpInBuf2 type=complete
    const int collumn0[NKX] = {  1,0,1,2 };
#pragma HLS ARRAY_PARTITION variable=collumn0 type=complete
    const int collumn1[NKX] = {  0,1,0,1 };
#pragma HLS ARRAY_PARTITION variable=collumn1 type=complete
    const int collumn2[NKX] = { -1,0,1,0 };
#pragma HLS ARRAY_PARTITION variable=collumn2 type=complete
    int rows_index, bank_index;
    static int rows = 0;static int bank = 0;static int s_rows = 0; // Variables that contain info, relevant to choosing row and bank of Inbuf to load from.

    if(state_Nky<S){
    	// Poy<POY-1
    	for(int Poy_i=0;Poy_i<POY-1;Poy_i++){
#pragma HLS UNROLL
    		if( east_pad && state_Nkx==(NKX-1)){ //Pox=0
				tmpInBuf0[Poy_i] = 0;
			}
			else{
				tmpInBuf0[Poy_i] = InBuf[Poy_i][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ state_Nky*WRD_1ROW+collumn0[state_Nkx]][0];
			}
			tmpInBuf1[Poy_i] = InBuf[Poy_i][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ state_Nky*WRD_1ROW+collumn1[state_Nkx]][1];
			if( west_pad && state_Nkx==0){
				tmpInBuf2[Poy_i] = 0;
			}
			else{
				tmpInBuf2[Poy_i] = InBuf[Poy_i][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ state_Nky*WRD_1ROW+collumn2[state_Nkx]][2];
			}
			// Write InternalReg
			if(state == 0){
				InternalReg[Poy_i][2] = tmpInBuf0[Poy_i];
				InternalReg[Poy_i][1] = tmpInBuf1[Poy_i];
				InternalReg[Poy_i][0] = tmpInBuf2[Poy_i];
			}
			else if(state == 1){
				InternalReg[Poy_i][0] = tmpInBuf0[Poy_i];
				InternalReg[Poy_i][2] = tmpInBuf1[Poy_i];
				InternalReg[Poy_i][1] = tmpInBuf2[Poy_i];
			}
			else{
				InternalReg[Poy_i][1] = tmpInBuf0[Poy_i];
				InternalReg[Poy_i][0] = tmpInBuf1[Poy_i];
				InternalReg[Poy_i][2] = tmpInBuf2[Poy_i];
			}
    	}
    	// Poy=POY-1
		rows_index = state_Nky;
		bank_index = POY-1;
		if( east_pad && state_Nkx==(NKX-1)){ //Pox=0
			tmpInBuf0[POY-1] = 0;
		}
		else{
			tmpInBuf0[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn0[state_Nkx]][0];
		}
		tmpInBuf1[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn1[state_Nkx]][1];
		if( west_pad && state_Nkx==0){
			tmpInBuf2[POY-1] = 0;
		}
		else{
			tmpInBuf2[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn2[state_Nkx]][2];
		}

		// Write InternalReg
		if(state == 0){
			InternalReg[POY-1][2] = tmpInBuf0[POY-1];
			InternalReg[POY-1][1] = tmpInBuf1[POY-1];
			InternalReg[POY-1][0] = tmpInBuf2[POY-1];
		}
		else if(state == 1){
			InternalReg[POY-1][0] = tmpInBuf0[POY-1];
			InternalReg[POY-1][2] = tmpInBuf1[POY-1];
			InternalReg[POY-1][1] = tmpInBuf2[POY-1];
		}
		else{
			InternalReg[POY-1][1] = tmpInBuf0[POY-1];
			InternalReg[POY-1][0] = tmpInBuf1[POY-1];
			InternalReg[POY-1][2] = tmpInBuf2[POY-1];
		}
    }
    else{
    	// Poy<POY-1
    	for(int Poy_i=0;Poy_i<POY-1;Poy_i++){
#pragma HLS UNROLL
			for(int Pox_i=0;Pox_i<POX;Pox_i++){
#pragma HLS UNROLL
				fifo_arr[Poy_i][Pox_i].read(InternalReg[Poy_i][Pox_i]);
			}
    	}
		// Poy=POY-1
		rows_index = (S*(s_rows+1)+rows);
		bank_index = bank;
		if( east_pad && state_Nkx==(NKX-1)){ //Pox=0
			tmpInBuf0[POY-1] = 0;
		}
		else{
			tmpInBuf0[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn0[state_Nkx]][0];
		}
		tmpInBuf1[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn1[state_Nkx]][1];
		if( west_pad && state_Nkx==0){
			tmpInBuf2[POY-1] = 0;
		}
		else{
			tmpInBuf2[POY-1] = InBuf[bank_index][InBuf_addr_base + state_Nif*ROWS_1MAP*WRD_1ROW+ rows_index*WRD_1ROW+collumn2[state_Nkx]][2];
		}

		// Write InternalReg
		if(state == 0){
			InternalReg[POY-1][2] = tmpInBuf0[POY-1];
			InternalReg[POY-1][1] = tmpInBuf1[POY-1];
			InternalReg[POY-1][0] = tmpInBuf2[POY-1];
		}
		else if(state == 1){
			InternalReg[POY-1][0] = tmpInBuf0[POY-1];
			InternalReg[POY-1][2] = tmpInBuf1[POY-1];
			InternalReg[POY-1][1] = tmpInBuf2[POY-1];
		}
		else{
			InternalReg[POY-1][1] = tmpInBuf0[POY-1];
			InternalReg[POY-1][0] = tmpInBuf1[POY-1];
			InternalReg[POY-1][2] = tmpInBuf2[POY-1];
		}
    }
	//******************************************************************************************************************

	if(state_Nkx==NKX-1){
		state = 0;
	}
	else if(state == POX-1){
		state = 0;
	}
	else{
		state++;
	}

	if(state_Nkx==NKX-1){
		if(state_Nky==NKY-1){
			if(state_Nif==NIF-1){
				state_Nif = 0;
			}
			else{
				state_Nif++;
			}
			state_Nky = 0;
		}
		else{
			state_Nky++;
		}
		state_Nkx = 0;
	}
	else{
		state_Nkx++;
	}
	InBuf_indexing_read(&rows, &bank, &s_rows); // Inlined
}

void Reg2FIFO_PE(px_data_t InternalReg[POY][POX], hls::stream<px_data_t> fifo_arr[POY-1][POX], px_data_t to_PE[POY][POX]){
#pragma HLS INLINE
	static int state_Nky = 0;static int count = 0;
	// Forward to FIFOs and PEs
    for(int Pox_i=0;Pox_i<POX;Pox_i++){
#pragma HLS UNROLL
    	for(int Poy_i=0;Poy_i<POY;Poy_i++){
#pragma HLS UNROLL
			if( (Poy_i!=0)&&(state_Nky<NKY-S) ){
    			fifo_arr[Poy_i-1][Pox_i].write(InternalReg[Poy_i][Pox_i]);
			}
			to_PE[Poy_i][Pox_i] = InternalReg[Poy_i][Pox_i];
    	}
    }
    if(count==NKX-1){
        if(state_Nky==NKY-1){
        	state_Nky = 0;
        }
        else{
        	state_Nky++;
        }
        count = 0;
    }
    else{
    	count++;
    }

}

void wt_load(wt_data_t WtBuf[WRD_WTBUF][POF], int tmp_Wt_addr,
		wt_data_t wt[POF]){
#pragma HLS INLINE
	static int state_Nif = 0;static int state_Nky = 0;static int state_Nkx = 0;
	int Wt_addr_final;
	Wt_addr_final = tmp_Wt_addr + state_Nif*NKX*NKY + state_Nky*NKX + state_Nkx;
	for(int count=0;count<POF;count++){
#pragma HLS unroll
		wt[count] = WtBuf[Wt_addr_final][count];
	}
	if(state_Nkx==NKX-1){
		if(state_Nky==NKY-1){
			if(state_Nif==NIF-1){
				state_Nif = 0;
			}
			else{
				state_Nif++;
			}
			state_Nky = 0;
		}
		else{
			state_Nky++;
		}
		state_Nkx = 0;
	}
	else{
		state_Nkx++;
	}
}

void InBuf2PE(px_data_t InBuf[POY][WRD_INBUF][POX],
		data_bool west_pad, data_bool east_pad,
		int InBuf_addr_base,
		px_data_t to_PE[POY][POX]){
#pragma HLS INLINE off
	px_data_t InternalReg[POY][POX]; // Internal Registers of BUF2PE data bus
#pragma HLS ARRAY_PARTITION variable=InternalReg complete dim=1
#pragma HLS ARRAY_PARTITION variable=InternalReg complete dim=2
    static hls::stream<px_data_t> fifo_arr[POY-1][POX]; // FIFOs of BUF2PE data bus
#pragma HLS STREAM variable=fifo_arr depth=8
#pragma HLS ARRAY_PARTITION variable=fifo_arr complete dim=1
#pragma HLS ARRAY_PARTITION variable=fifo_arr complete dim=2
	//static int rows = 0;static int bank = 0;static int s_rows = 0;
	BUF2InternalReg(InBuf, InternalReg, fifo_arr, InBuf_addr_base,
			west_pad, east_pad);//, rows, bank, s_rows); // Inlined
	Reg2FIFO_PE(InternalReg, fifo_arr, to_PE); // Inlined
	//InBuf_indexing_read(&rows, &bank, &s_rows); // Inlined
}

void Buf2Pe(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		data_bool west_pad, data_bool east_pad,
		int Wt_addr_base, int InBuf_addr_base,
		px_data_t to_PE[POY][POX], wt_data_t wt[POF]){
#pragma HLS INLINE
	InBuf2PE(InBuf, west_pad, east_pad, InBuf_addr_base, to_PE);
	wt_load(WtBuf, Wt_addr_base, wt);
}

void Parallel_Calc_no_Shift(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		int Tof_step, int Toy_step, int Tox_step,
		int *Tof_step_next, int *Toy_step_next, int *Tox_step_next,
		px_data_t rslt[POF][POY][POX]){
#pragma HLS INLINE off
	// Declarations
	data_bool west_pad; data_bool east_pad;
	int Wt_addr_base; int InBuf_addr_base;
	// Precomputations
	west_pad = (Tox_step==0);
	east_pad = (Tox_step==my_ceil(TOX,POX)-1);
	InBuf_addr_base = Toy_step*WRD_1ROW*S+Tox_step*S;
	Wt_addr_base = Tof_step*NKX*NKY*TIF;

	NifNkyNkx_Loop: for(int i=0;i<NIF*NKY*NKX;i++){
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE
		px_data_t to_PE[POY][POX]; // Pixels being transported to PE
		wt_data_t wt[POF];
		Buf2Pe(InBuf, WtBuf, west_pad, east_pad, Wt_addr_base, InBuf_addr_base, to_PE, wt);
		PE(rslt, to_PE, wt);
	}
	*Tof_step_next = Tof_step;
	*Toy_step_next = Toy_step;
	*Tox_step_next = Tox_step;
}

void PE2BUF(px_data_t PE_results[POF][POY][POX],
		int Tof_step, int Toy_step, int Tox_step,
		px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]){
#pragma HLS INLINE off
#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=PE_results complete

	// Declarations
	int crnt_wrd; int crnt_bank;
	data_bool Pof_last; data_bool Poy_last; data_bool Pox_last;
	// Pre-Computations
	Pof_last = (TOF%POF!=0)&&(Tof_step==TOF/POF);
	Poy_last = (TOY%POY!=0)&&(Toy_step==TOY/POY);
	Pox_last = (TOX%POX!=0)&&(Tox_step==TOX/POX);
	int POF_count = (Pof_last)? TOF%POF : POF;
	int POY_count = (Poy_last)? TOY%POY : POY;
    int POX_count = (Pox_last)? TOX%POX : POX;
    crnt_wrd = Tox_step + Toy_step*POY*my_ceil(TOX,POX)
                + ((Tof_step*POF)/OUTBUF_NUM)*my_ceil(TOX,POX)*TOY;
    crnt_bank = (Tof_step*POF)%OUTBUF_NUM;

    Loop_POY_count: for(int Poy_i=0;Poy_i<POY;Poy_i++){
        Loop_my_ceil: for(int index2=0;index2<my_ceil(POF,OUTBUF_NUM);index2++){
        	for(int OutBuf_Num_i=0;OutBuf_Num_i<OUTBUF_NUM;OutBuf_Num_i++){
#pragma HLS UNROLL
				for(int Pox_i=0;Pox_i<POX;Pox_i++){
#pragma HLS UNROLL
					if(Poy_i<POY_count && index2<my_ceil(POF_count,OUTBUF_NUM)){
						if(index2*OUTBUF_NUM+OutBuf_Num_i<POF_count
								&& Pox_i<POX_count){
							OutBuf[(crnt_bank+OutBuf_Num_i)%OUTBUF_NUM]
								   [Poy_i*my_ceil(TOX,POX) + crnt_wrd
									+index2*my_ceil(TOX,POX)*TOY
									+((crnt_bank+OutBuf_Num_i)/OUTBUF_NUM)*my_ceil(TOX,POX)*TOY]
										   [Pox_i] =
							PE_results[(OutBuf_Num_i+index2*OUTBUF_NUM)][Poy_i][Pox_i];
						}
						else{
							OutBuf[(crnt_bank+OutBuf_Num_i)%OUTBUF_NUM]
								[index2*my_ceil(TOX,POX)*TOY + Poy_i*my_ceil(TOX,POX) + crnt_wrd
								+((crnt_bank+OutBuf_Num_i)/OUTBUF_NUM)*my_ceil(TOX,POX)*TOY]
								[Pox_i] = 0;
						}
					}
				}
			}
        }
    }
}

void BUF_Calc(px_data_t InBuf[POY][WRD_INBUF][POX], wt_data_t WtBuf[WRD_WTBUF][POF],
		px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX]){
#pragma HLS INLINE
    px_data_t PE_results[POF][POY][POX];
//#pragma HLS STREAM type=fifo variable=PE_results
#pragma HLS ARRAY_PARTITION variable=PE_results complete dim=3
#pragma HLS ARRAY_PARTITION variable=PE_results complete dim=2
#pragma HLS ARRAY_PARTITION variable=PE_results complete dim=1
// Tif is not needed here bc Loop-2 is calculated before and Nif=Tif
// Buffer Calculations
    Loop_Tof_step: for(int Tof_step=0;Tof_step<my_ceil(TOF,POF);Tof_step++){
//#pragma HLS PIPELINE I don't place pipeline here, because there is no loop
        Loop_Toy_step: for(int Toy_step=0;Toy_step<my_ceil(TOY,POY);Toy_step++){
//#pragma HLS PIPELINE I don't place pipeline here, because there is no loop
            Loop_Tox_step: for(int Tox_step=0;Tox_step<my_ceil(TOX,POX);Tox_step++){
#pragma HLS PIPELINE off
            	int Tof_step_next; int Toy_step_next; int Tox_step_next;
            	Parallel_Calc_no_Shift(InBuf, WtBuf,
            			Tof_step, Toy_step, Tox_step,
						&Tof_step_next, &Toy_step_next, &Tox_step_next,
            			PE_results);
            	PE2BUF(PE_results, Tof_step_next, Toy_step_next, Tox_step_next, OutBuf);
            }
        }
    }
}

void InBuf_indexing(int *InBuf_rows_count, int *InBuf_rows_precompute,
		int *InBuf_banks_precompute){
	if( (*InBuf_rows_count) == S-1){
		(*InBuf_rows_count) = 0;
		if( (*InBuf_banks_precompute) == POY-1){
			(*InBuf_banks_precompute) = 0;
			(*InBuf_rows_precompute) += WRD_1ROW;
		}
		else{
			(*InBuf_banks_precompute)++;
			(*InBuf_rows_precompute) -= (S-1)*WRD_1ROW;
		}
	}
	else{
		(*InBuf_rows_count)++;
		(*InBuf_rows_precompute) += WRD_1ROW;
	}
}

// What if TIX is a leftover
// This version uses precomputing to know the indexes to write to InBuf
// Is general in terms of values, besides Tix
void IF2BUF(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		px_data_t InBuf[POY][WRD_INBUF][POX],
		//int IfMap_y_base_address,
		int Noy_step, int IfMap_x_Address,
		data_bool north_pad, data_bool south_pad, data_bool east_pad, data_bool west_pad){
// Have to double check for bursting
// IfMap-y_base_address is used because after reading all y lines
// for 1 Map we need to do it all again
	int Buf_bank,Buf_word,Buf_Wrd_base=0,IfMap_y_Address;
	int InBuf_banks_precompute = 0, InBuf_rows_precompute = 0, InBuf_rows_count = 0;

	// Here, maybe I should check if we have smaller tile because of division reminders
	int x_tile_size = TIX - east_pad * ZERO_PAD - west_pad * ZERO_PAD;
	int IfMap_y_base_address;
	if(north_pad){
		IfMap_y_base_address = 0;
	}
	else{
		IfMap_y_base_address = Noy_step*(TIY-NKY+S)-ZERO_PAD;
	}

	Input_Loop_Tif: for(int Tif_i=0;Tif_i<TIF;Tif_i++){
#pragma HLS UNROLL factor=1
		IfMap_y_Address = IfMap_y_base_address;
		// Conditional for North Padding
		Input_Loop_North_Pad: for(int i=0; i<ZERO_PAD; i++){
#pragma HLS UNROLL factor=1
			if(north_pad==0){
				//Buf_bank = InBuf_banks[i];
				Buf_bank = InBuf_banks_precompute;
				Buf_word = Buf_Wrd_base + InBuf_rows_precompute;
				Input_Loop_Line_1:for(int Tix_i=0;Tix_i<x_tile_size;Tix_i++){
#pragma HLS PIPELINE
					InBuf[Buf_bank][Buf_word+row[Tix_i]][col[Tix_i]] = IfMap[Tif_i][IfMap_x_Address+Tix_i][IfMap_y_Address];
				}
				IfMap_y_Address++;
			}
			else{
				Buf_bank = InBuf_banks_precompute;
				Buf_word = Buf_Wrd_base + InBuf_rows_precompute;
				IF2BUF_label11:for(int Tix_i=0;Tix_i<x_tile_size;Tix_i++){
#pragma HLS PIPELINE
					InBuf[Buf_bank][Buf_word+row[Tix_i]][col[Tix_i]] = 0;
				}
			}
			InBuf_indexing(&InBuf_rows_count, &InBuf_rows_precompute,
					&InBuf_banks_precompute);
		}
		// ********************************
		Input_Loop_Tiy_Main: for(int Tiy_i=0;Tiy_i<TIY-2*ZERO_PAD;Tiy_i++){
#pragma HLS UNROLL factor=1
			//Buf_bank = InBuf_banks[ZERO_PAD+Tiy_i];
			//Buf_word = Buf_Wrd_base + InBuf_rows[ZERO_PAD+Tiy_i];
			Buf_bank = InBuf_banks_precompute;
			Buf_word = Buf_Wrd_base + InBuf_rows_precompute;
			Input_Loop_Line_2:for(int Tix_i=0;Tix_i<x_tile_size;Tix_i++){
#pragma HLS PIPELINE
				InBuf[Buf_bank][Buf_word+row[Tix_i]][col[Tix_i]] = IfMap[Tif_i][IfMap_x_Address+Tix_i][IfMap_y_Address];
			}
			//
			IfMap_y_Address++;
			InBuf_indexing(&InBuf_rows_count, &InBuf_rows_precompute,
					&InBuf_banks_precompute);
		}
		// Conditional for South Padding
		Input_Loop_South_Pad: for(int i=0; i<ZERO_PAD; i++){
#pragma HLS UNROLL factor=1
			if(south_pad==0){
				//Buf_bank = InBuf_banks[i];
				Buf_bank = InBuf_banks_precompute;
				Buf_word = Buf_Wrd_base + InBuf_rows_precompute;
				Input_Loop_Line_3:for(int Tix_i=0;Tix_i<x_tile_size;Tix_i++){
#pragma HLS PIPELINE
					InBuf[Buf_bank][Buf_word+row[Tix_i]][col[Tix_i]] = IfMap[Tif_i][IfMap_x_Address+Tix_i][IfMap_y_Address];
				}
				IfMap_y_Address++;
			}
			else{
				Buf_bank = InBuf_banks_precompute;
				Buf_word = Buf_Wrd_base + InBuf_rows_precompute;
				IF2BUF_label123b:for(int Tix_i=0;Tix_i<x_tile_size;Tix_i++){
#pragma HLS PIPELINE
					InBuf[Buf_bank][Buf_word+row[Tix_i]][col[Tix_i]] = 0;
				}
			}
			InBuf_indexing(&InBuf_rows_count, &InBuf_rows_precompute,
					&InBuf_banks_precompute);
		}
		// ********************************
		Buf_Wrd_base += ROWS_1MAP*WRD_1ROW; // After finishing writing the tile of 1 map to InBuf you add (ROWS_1MAP*WRD_1ROW)
		InBuf_rows_precompute = 0;
		InBuf_rows_count = 0;
		InBuf_banks_precompute = 0;
	}
}

void WtBuf_indexing(int *Buf_bank, int *Wrd_base){
	if( (*Buf_bank) == POF-1){
		(*Buf_bank) = 0;
		(*Wrd_base) += NIF;
	}
	else{
		(*Buf_bank)++;
	}
}

// It is well assumed that Nif=Tif
void WT2BUF(wt_data_t WtMap[NIF][NOF][NKX][NKY], wt_data_t WtBuf[WRD_WTBUF][POF],
		int WtMap_no_Address){
	// precompute for Buf_bank
	// maybe, instead of addition, multiply from Tof_i, to not ruin perfect nest
	int Buf_bank=0, Wrd_base=0;

	WT2BUF_label2:for(int Tof_i=0;Tof_i<TOF;Tof_i++){
		WT2BUF_label3:for(int Nif_i=0;Nif_i<NIF;Nif_i++){
			for(int Nky_i=0;Nky_i<NKY;Nky_i++){
				WT2BUF_label4:for(int Nkx_i=0;Nkx_i<NKX;Nkx_i++){
#pragma HLS PIPELINE
					WtBuf[(Wrd_base+Nif_i)*NKX*NKY + Nkx_i + Nky_i*NKX][Buf_bank] = WtMap[Nif_i][WtMap_no_Address+Tof_i][Nkx_i][Nky_i];
				}
			}
		}
		WtBuf_indexing(&Buf_bank, &Wrd_base);
	}
}

void OutBuf_indexing(int *OutBuf_rows_precompute,
		int *OutBuf_banks_precompute){
	if( (*OutBuf_banks_precompute) == OUTBUF_NUM-1){
		(*OutBuf_banks_precompute) = 0;
		(*OutBuf_rows_precompute) += (TOY);
	}
	else{
		(*OutBuf_banks_precompute)++;
	}
}

void BUF2OF(px_data_t OfMap[NOF][NOX][NOY], px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX],
		int OfMap_no_Address,
		//int OfMap_y_Address,
		int Noy_step, int OfMap_x_Address){
// WRD_OUTBUF=my_ceil(TOF, OUTBUF_NUM) * TOY * my_ceil(TOX, POX)
	int Buf_bank=0, Buf_word=0;
	int OfMap_y_Address = Noy_step*TOY;
	// Here, I should check if Tox, Toy, Tof have reminders
	for(int Tof_i=0;Tof_i<TOF;Tof_i++){
		for(int Toy_i=0;Toy_i<TOY;Toy_i++){
			OF2BUF_line_load:for(int Tox_i=0;Tox_i<TOX;Tox_i++){
#pragma HLS PIPELINE
				OfMap[OfMap_no_Address+Tof_i][OfMap_x_Address+Tox_i][OfMap_y_Address+Toy_i] =
						OutBuf[Buf_bank][(Buf_word+Toy_i)*WRD_1ROW_O+row[Tox_i]][col[Tox_i]];
			}
		}
		OutBuf_indexing(&Buf_word, &Buf_bank);
	}
}

void tile_dfl(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		px_data_t OfMap[NOF][NOX][NOY],
		wt_data_t WtBuf[WRD_WTBUF][POF],
		int Noy_step, int no_Address){
// NIX, NIY consists of zero padding
	px_data_t InBuf[POY][WRD_INBUF][POX];
#pragma HLS ARRAY_PARTITION variable=InBuf complete dim=1
#pragma HLS ARRAY_PARTITION variable=InBuf complete dim=3
	px_data_t OutBuf[OUTBUF_NUM][WRD_OUTBUF][POX];
#pragma HLS ARRAY_PARTITION variable=OutBuf complete dim=1
#pragma HLS ARRAY_PARTITION variable=OutBuf complete dim=3

	IF2BUF(IfMap, InBuf, Noy_step, 0, (Noy_step==0), (Noy_step==my_ceil(NOY,TOY)-1), 1, 1);
	BUF_Calc(InBuf, WtBuf, OutBuf);
	BUF2OF(OfMap, OutBuf, no_Address, Noy_step, 0);
}

// Doesn't handle leftover data
void CNN_Layer(px_data_t IfMap[NIF][NIX-2*ZERO_PAD][NIY-2*ZERO_PAD],
		wt_data_t WtMap[NIF][NOF][NKX][NKY],
		px_data_t OfMap[NOF][NOX][NOY]){
	wt_data_t WtBuf[WRD_WTBUF][POF];
#pragma HLS ARRAY_PARTITION variable=WtBuf complete dim=2
// Variables for addresses
	int IfMap_y_Address, OfMap_y_Address, no_Address;
	Loop_Nof: for(int Nof_step=0;Nof_step<my_ceil(NOF,TOF);Nof_step++){
		no_Address = Nof_step*TOF;
		WT2BUF(WtMap, WtBuf, no_Address);
		Loop_Noy: for(int Noy_step=0;Noy_step<my_ceil(NOY,TOY);Noy_step++){
			tile_dfl(IfMap, OfMap, WtBuf, Noy_step, no_Address);
		}
	}
}
