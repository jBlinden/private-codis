#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <string.h>
#include <stdint.h>

#include "socket.h"
#include "util.h"

using namespace std;

#define VERBOSE 0
#define OPTIMAL 1
#define LOGGING 0

static const uint64_t MAX_OT_BATCH = 15000000;
static const int TIMEOUT_MS = 10000;

static const uint8_t bits_FSM1 = 4;
static const uint8_t bits_FSM2 = 6;

static const uint8_t bits_FSM1_SOT = 1;
static const uint8_t bits_FSM2_SOT = 2;

static const uint8_t store_sOT_as_key = 0;

static const uint8_t FSM1_StepSize = 2;
static const uint8_t FSM2_StepSize = 2;

bool Listen(CSocket* socket, int port);
bool Connect(CSocket* socket, const char* address, int port);

struct SenderOTCorrelation {
  uint8_t K;
  uint32_t N;
  AESContext msgKey;
  uint8_t* blindingFactors;

  SenderOTCorrelation(uint32_t N, uint8_t K);
  SenderOTCorrelation(ifstream &in, uint8_t bitsPerFSM);
  ~SenderOTCorrelation();

  void WriteToFile(ofstream& fout, uint8_t bitsPerFSM) const;
  void ReadFromFile(ifstream& fin, uint8_t bitsPerFSM);
};

struct ReceiverOTCorrelation {
  uint8_t K;
  uint32_t N;
  AESContext choiceKey;

  uint8_t* choices;
  uint8_t* blindingFactors;


  ReceiverOTCorrelation(uint32_t N, uint8_t K);
  ReceiverOTCorrelation(ifstream &in, uint8_t bitsPerFSM);
  ~ReceiverOTCorrelation();

  void ComputeChoiceBits();
  
  void WriteToFile(ofstream& fout, uint8_t bitsPerFSM) const;
  void ReadFromFile(ifstream& fin, uint8_t bitsPerFSM);
};

struct ProtocolArgs {
  uint32_t N;
  uint32_t C;
  uint32_t FSM1_rounds;
  uint32_t FSM2_rounds;
  uint8_t* FSM1_steps;
  uint8_t* FSM2_steps;
  uint8_t* bits_per_component;

  bool alloc_OT;

  ~ProtocolArgs();

  ProtocolArgs(uint32_t N, uint32_t C, uint8_t* FSM1_steps, uint8_t* FSM2_steps, uint32_t FSM1_rounds, uint32_t FSM2_rounds, uint8_t* bits_per_component) :
    N(N), C(C), FSM1_steps(FSM1_steps), FSM2_steps(FSM2_steps), FSM1_rounds(FSM1_rounds), FSM2_rounds(FSM2_rounds), bits_per_component(bits_per_component) { alloc_OT = false; };
  ProtocolArgs(ifstream& in);
};

void CreateOTCorrelation(SenderOTCorrelation& senderCorrelation,
                         ReceiverOTCorrelation& receiverCorrelation, uint8_t bitsPerFSMROT, uint8_t bitsPerFSMSOT);

void ConstructOTReceiverMsg(uint8_t* receiverMsg, const ReceiverOTCorrelation& rc,
                            const uint8_t* inputArr, uint32_t nOTs);

void ConstructOTSenderMsg(uint8_t* senderMsg, const SenderOTCorrelation& sc, const uint8_t* msgs,
                          uint8_t* receiverArr, uint32_t nOTs, uint8_t bitsPerMsg);

void RecoverReceiverMsg(uint8_t* msgs, const ReceiverOTCorrelation& rc,
                        uint8_t* inputArr, uint8_t* senderMsgs, uint32_t nOTs, uint8_t bitsPerMsg, uint8_t bitsPerFSM);

// OT sender/receiver protocols with OT correlations
void OTSenderProtocol(CSocket* socket, const SenderOTCorrelation& corr,
                      uint32_t nOTs, const uint8_t* msgs);

void OTReceiverProtocol(CSocket* socket, uint8_t* outputs, const ReceiverOTCorrelation& corr,
                        uint32_t nOTs, const uint8_t* choiceBits);

void CreateAndExportOTCorrelation(ofstream& out1, ofstream& out2, uint32_t N, uint8_t K, uint8_t bitsPerFSMROT, uint8_t bitsPerFSMSOT);


void WriteProtocolArgs(ofstream& out1, struct ProtocolArgs info);

uint8_t* ReadInConfigFile(ifstream& in, uint32_t &C);

SenderOTCorrelation** ReadInSenderOTsFSM1(ifstream& in, struct ProtocolArgs args);
SenderOTCorrelation** ReadInSenderOTsFSM2(ifstream& in, struct ProtocolArgs args);

ReceiverOTCorrelation** ReadInReceiverOTsFSM1(ifstream& in, struct ProtocolArgs args);
ReceiverOTCorrelation** ReadInReceiverOTsFSM2(ifstream& in, struct ProtocolArgs args);

static block *GetByteStream(const AESContext &ctx, uint32_t nBytes, uint64_t tag);

static inline void Log(const string& type, const string& msg) {
  cout << "[" << type << "] " << msg << endl;
}

static inline void ServerLog(const string& msg) {
    Log("server", msg);
}

static inline void ClientLog(const string& msg) {
    Log("client", msg);
}

uint8_t count_ones(uint8_t byte);

uint32_t* ReadInEntry(ifstream& in, uint32_t C);

uint32_t** ReadInDB(ifstream& in, uint32_t N, uint32_t C);

void SendLargePacked(CSocket* socket, uint8_t bitsPerMsg, uint64_t length, uint8_t* data);
void ReceiveLargePacked(CSocket* socket, uint8_t bitsPerMsg, uint64_t length, uint8_t* out);

#endif
