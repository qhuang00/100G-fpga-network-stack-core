/************************************************
Copyright (c) 2016, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors 
may be used to endorse or promote products derived from this software 
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2015 Xilinx, Inc.
************************************************/
#include "tx_engine.hpp"

using namespace hls;
using namespace std;

void simulateSARtables(	stream<rxSarEntry>&				rxSar2txEng_upd_rsp,
						stream<txTxSarReply>&			txSar2txEng_upd_rsp,
						stream<ap_uint<16> >&			txEng2rxSar_upd_req,
						stream<txTxSarQuery>&			txEng2txSar_upd_req)
{
	ap_uint<16> addr;
	txTxSarQuery in_txaccess;

	if (!txEng2rxSar_upd_req.empty())
	{
		txEng2rxSar_upd_req.read(addr);
		rxSar2txEng_upd_rsp.write((rxSarEntry) {0x0023, 0xadbd});
	}
	if (!txEng2txSar_upd_req.empty())
	{
		txEng2txSar_upd_req.read(in_txaccess);
		if (in_txaccess.write == 0)
		{
			txSar2txEng_upd_rsp.write(((txTxSarReply) {3, 5, 0xffff, 1500, false,false}));
		}
		//omit write
	}
}

void simulateTxBuffer(stream<mmCmd>&	command,
						stream<axiWord>& dataOut)
{
	static mmCmd cmd;
	static ap_uint<1> fsmState = 0;
	static ap_uint<16> wordCount = 0;

	static ap_uint<4> 	bcd_l=0;
	static ap_uint<4> 	bcd_h=0;

	axiWord memWord;

	switch (fsmState) {
		case 0:
			if (!command.empty()) {
				command.read(cmd);
				fsmState = 1;
			}
			break;
		case 1:

			memWord.last = 0x0;
			for (int i=0 ; i < 64 ; i++){
				if (wordCount <= cmd.bbt){
					memWord.data(i*8+7,i*8) = (bcd_h,bcd_l);
					memWord.keep.bit(i) = 1;

					bcd_l++;
					if (bcd_l==10){
						bcd_h++;
						bcd_l=0;
						if (bcd_h==10){
							bcd_h=0;
						}
					}
				}
				wordCount++;
			}

			if (wordCount >= cmd.bbt) {
				memWord.last = 0x1;
				fsmState = 0;
				wordCount = 0;
			}

			//cout << "Payload: " << hex << memWord.data << "\tkeep: " << memWord.keep << "\tlast: " << dec << memWord.last << endl;

			dataOut.write(memWord);
			break;
	}
}

void simulateRevSLUP(stream<ap_uint<16> >&			txEng2sLookup_rev_req,
					stream<fourTuple>&				sLookup2txEng_rev_rsp)
{
	fourTuple tuple;
	tuple.dstIp = 0x0101010a;
	tuple.dstPort = 0x5001;
	tuple.srcIp = 0x01010101;
	tuple.srcPort = 0xaffff;
	if (!txEng2sLookup_rev_req.empty())
	{
		txEng2sLookup_rev_req.read();
		sLookup2txEng_rev_rsp.write(tuple);
	}
}

int main()
{
	ap_uint<16> sessionID;
	stream<extendedEvent>			eventEng2txEng_event("eventEng2txEng_event");
	stream<rxSarEntry>				rxSar2txEng_upd_rsp("rxSar2txEng_upd_rsp");
	stream<txTxSarReply>			txSar2txEng_upd_rsp("txSar2txEng_upd_rsp");
	stream<axiWord>					txBufferReadData("txBufferReadData");
	stream<fourTuple>				sLookup2txEng_rev_rsp("sLookup2txEng_rev_rsp");
	stream<ap_uint<16> >			txEng2rxSar_upd_req("txEng2rxSar_upd_req");
	stream<txTxSarQuery>			txEng2txSar_upd_req("txEng2txSar_upd_req");
	stream<txRetransmitTimerSet>	txEng2timer_setRetransmitTimer("txEng2timer_setRetransmitTimer");
	stream<ap_uint<16> >			txEng2timer_setProbeTimer("txEng2timer_setProbeTimer");
	stream<mmCmd>					txBufferReadCmd("txBufferReadCmd");
	stream<ap_uint<16> >			txEng2sLookup_rev_req("txEng2sLookup_rev_req");
	stream<axiWord>					ipTxData;
	stream<ap_uint<1> > 			readCountFifo("readCountFifo");

	vector<int> values;
	vector<int> lengths;

	ofstream outputFile;


	outputFile.open("/home/mario/Documents/cmac_100g/submodules/tcp_ip_cores/TOE/src/tx_engine/out.dat");
	if (!outputFile) {
		cout << "Error: could not open test output file." << endl;
		return -1;
	}

	axiWord outData;

	uint16_t strbTemp;
	uint64_t dataTemp;
	uint16_t lastTemp;
	int count = 0;
	while (count < 1000)
	{
		int v = rand() % 100;
		int v1 = rand() % 32000;
		int len = rand() % 10;

		/*event ev;
		ev.type = TX;
		ev.address = v1;
		ev.length = len+2;
		ev.sessionID = v;
		values.push_back(v);
		lengths.push_back(len+2);*/

		if ((count % 9) == 0){
		//if (count  == 9) {
			eventEng2txEng_event.write(event(TX, 3, 100, 536));
			values.push_back(3);
			lengths.push_back(536);
		}

		tx_engine(	eventEng2txEng_event,
					rxSar2txEng_upd_rsp,
					txSar2txEng_upd_rsp,
					txBufferReadData,
					sLookup2txEng_rev_rsp,
					txEng2rxSar_upd_req,
					txEng2txSar_upd_req,
					txEng2timer_setRetransmitTimer,
					txEng2timer_setProbeTimer,
					txBufferReadCmd,
					txEng2sLookup_rev_req,
					ipTxData,
					readCountFifo);
		simulateSARtables(rxSar2txEng_upd_rsp, txSar2txEng_upd_rsp, txEng2rxSar_upd_req, txEng2txSar_upd_req);
		simulateTxBuffer(txBufferReadCmd, txBufferReadData);
		simulateRevSLUP(txEng2sLookup_rev_req, sLookup2txEng_rev_rsp);
		
		while (!ipTxData.empty()) {
			ipTxData.read(outData);
			//outputFile << hex;
			//outputFile << setfill('0');
			outputFile << "Data: " << hex << outData.data << "\tkeep: " << outData.keep << "\tlast: " << dec << outData.last << endl;
			//outputFile << setw(1) ;
			//if (outData.last) {
			//	outputFile << "len: " << lengths[count] << endl;
			//	count++;
			//}
		}
		count++;
	}
	count = 0;
	while (count < 1000)
	{
		tx_engine(	eventEng2txEng_event,
					rxSar2txEng_upd_rsp,
					txSar2txEng_upd_rsp,
					txBufferReadData,
					sLookup2txEng_rev_rsp,
					txEng2rxSar_upd_req,
					txEng2txSar_upd_req,
					txEng2timer_setRetransmitTimer,
					txEng2timer_setProbeTimer,
					txBufferReadCmd,
					txEng2sLookup_rev_req,
					ipTxData,
					readCountFifo);
		simulateSARtables(rxSar2txEng_upd_rsp, txSar2txEng_upd_rsp, txEng2rxSar_upd_req, txEng2txSar_upd_req);
		simulateTxBuffer(txBufferReadCmd, txBufferReadData);
		simulateRevSLUP(txEng2sLookup_rev_req, sLookup2txEng_rev_rsp);
		count++;
	}

	outputFile << "Data: " << endl;
	count = 0;
	while (!ipTxData.empty())
	{
		ipTxData.read(outData);
//		outputFile << hex;
//		outputFile << setfill('0');
//		outputFile << setw(16) << (uint32_t) outData.data(63, 32) << (uint32_t) outData.data(31, 0) << " " << setw(2) << outData.keep << " ";
//		outputFile << setw(1) << outData.last << endl;
		outputFile << "Data: " << hex << outData.data << "\tkeep: " << outData.keep << "\tlast: " << dec << outData.last << endl;
//		if (outData.last)
//		{
//			outputFile << "len: " << lengths[count] << endl;
//			count++;
//		}
	}

	outputFile << "rtTimer: " << endl;
	count = 0;
	txRetransmitTimerSet set;
	while (!(txEng2timer_setRetransmitTimer.empty()))
	{
		txEng2timer_setRetransmitTimer.read(set);
		outputFile << set.sessionID << " ";
		outputFile << ((set.sessionID == values[count]) ? "match" : "no match");
		outputFile << endl;
		count++;
	}

	outputFile << "timer2: " << endl;
	count = 0;
	while (!(txEng2timer_setProbeTimer.empty()))
	{
		txEng2timer_setProbeTimer.read(sessionID);
		outputFile << sessionID << " ";
		outputFile << ((sessionID == values[count]) ? "match" : "no match");
		outputFile << endl;
		count++;
	}



	values.clear();
	lengths.clear();
/*	values2.clear();
	values3.clear();
*/
	//should return comparison

	return 0;
}
