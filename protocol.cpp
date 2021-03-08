#include "protocol.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include "util.h"

uint8_t *Unpack(uint8_t bitsPerMsg, uint64_t length, uint8_t* data, uint8_t* out);
uint8_t *Pack(uint8_t bitsPerMsg, uint64_t length, uint8_t* data);
uint64_t packLength(uint64_t length, uint8_t bitsPerMsg);

bool Listen(CSocket *socket, int port)
{
  if (!socket->Socket())
  {
    return false;
  }

  if (!socket->Bind((uint16_t)port))
  {
    return false;
  }

  if (!socket->Listen())
  {
    return false;
  }

  CSocket sock;
  if (!socket->Accept(sock))
  {
    return false;
  }

  socket->AttachFrom(sock);
  sock.Detach();

  return true;
}

bool Connect(CSocket *socket, const char *address, int port)
{
  for (auto i = 0; i < 1000; i++)
  {
    if (!socket->Socket())
    {
      return false;
    }
    if (socket->Connect(address, (uint16_t)port, TIMEOUT_MS))
    {
      return true;
    }
    usleep(20);
  }

  return false;
}

SenderOTCorrelation::SenderOTCorrelation(uint32_t N, uint8_t K) : N(N), K(K)
{
  blindingFactors = new uint8_t[K * N];
}

SenderOTCorrelation::SenderOTCorrelation(ifstream &in, uint8_t bitsPerFSM)
{
  ReadFromFile(in, bitsPerFSM);
}

SenderOTCorrelation::~SenderOTCorrelation()
{
  delete[] blindingFactors;
}

ReceiverOTCorrelation::ReceiverOTCorrelation(uint32_t N, uint8_t K) : N(N), K(K)
{
  choices = new uint8_t[N];
  blindingFactors = new uint8_t[N];
}

ReceiverOTCorrelation::ReceiverOTCorrelation(ifstream &in, uint8_t bitsPerFSM)
{
  ReadFromFile(in, bitsPerFSM);
}

ReceiverOTCorrelation::~ReceiverOTCorrelation()
{
  delete[] choices;
  delete[] blindingFactors;
}

void CreateOTCorrelation(SenderOTCorrelation &sc,
                         ReceiverOTCorrelation &rc, uint8_t bitsPerFSMROT, uint8_t bitsPerFSMSOT)
{
  if ((sc.N != rc.N) ||
      (sc.K != rc.K))
  {
    throw "sender/receiver correlation type mismatch";
  }

  uint8_t K = rc.K;
  uint32_t N = rc.N;

  AESContext senderKey;
  senderKey.FillBuffer((uint8_t *)sc.blindingFactors, N * K * sizeof(uint8_t), 0);

  for (int i = 0; i < N * K; i++){
    sc.blindingFactors[i] &= (uint8_t)((1 << bitsPerFSMSOT) - 1);
  }
  
  AESContext receiverKey;
  receiverKey.FillBuffer((uint8_t *)rc.choices, N, 0);

  sc.msgKey = senderKey;
  rc.choiceKey = receiverKey;

  for (uint32_t i = 0; i < N; i++)
  {
    rc.choices[i] %= K;
    rc.blindingFactors[i] = sc.blindingFactors[i * K + rc.choices[i]] & ((uint8_t) (1 << bitsPerFSMROT) - 1);
  }
}

void ConstructOTReceiverMsg(uint8_t *receiverMsg, const ReceiverOTCorrelation &rc,
                            const uint8_t *inputArr, uint32_t nOTs)
{
  if (nOTs > rc.N)
  {
    throw "insufficient number of OT correlations";
  }

  for (int i = 0; i < nOTs; i++)
  {
    receiverMsg[i] = (rc.choices[i] - inputArr[i] + rc.K) % rc.K;
  }
}

void ConstructOTSenderMsg(uint8_t *senderMsg, const SenderOTCorrelation &sc,
                          const uint8_t *msgs, uint8_t *receiverArr, uint32_t nOTs, uint8_t bitsPerMsg)
{
  if (nOTs > sc.N)
  {
    throw "insufficient number of OT correlations";
  }

  memcpy(senderMsg, msgs, nOTs * sc.K * sizeof(uint8_t));
  for (int i = 0; i < nOTs; i++)
  {
    for (int j = 0; j < sc.K; j++)
    {
      senderMsg[sc.K * i + j] ^= sc.blindingFactors[sc.K * i + ((j + receiverArr[i]) % sc.K)];
      senderMsg[sc.K * i + j] &= (uint8_t)(1 << bitsPerMsg) - 1;
    }
  }
}

void RecoverReceiverMsg(uint8_t *msgs, const ReceiverOTCorrelation &rc,
                        uint8_t *inputArr, uint8_t *senderMsg, uint32_t nOTs, uint8_t bitsPerMsg, uint8_t bitsPerFSM)
{
  if (nOTs > rc.N) {
    throw "insufficient number of OT correlations";
  }

  memcpy(msgs, rc.blindingFactors, nOTs * sizeof(uint8_t));
  for (int i = 0; i < nOTs; i++) {
      msgs[i] ^= (senderMsg[rc.K*i + inputArr[i]] & ((uint8_t) (1 << bitsPerFSM) - 1));
      msgs[i] &= (uint8_t) (1 << bitsPerMsg) - 1;
  }
}

void ReceiverOTCorrelation::ComputeChoiceBits()
{
  choices = new uint8_t[N];
  
  choiceKey.FillBuffer((uint8_t *)choices, N, 0);

  for (uint32_t i = 0; i < N; i++)
  {
    choices[i] %= K;
  }
}

void OTSenderProtocol(CSocket *socket, const SenderOTCorrelation &corr,
                      uint32_t nOTs, const uint8_t *msgs)
{
  if (nOTs > corr.N)
  {
    throw "insufficient number of of OT correlations";
  }

  uint8_t *clientMsg = new uint8_t[nOTs];
  socket->ReceiveLarge(clientMsg, nOTs * sizeof(uint8_t));

  cout << "Receiving: " << (uint32_t) clientMsg[0] << endl;

  uint8_t *senderMsg = new uint8_t[corr.K * nOTs];
  ConstructOTSenderMsg(senderMsg, corr, msgs, clientMsg, nOTs, 8);

  socket->SendLarge((uint8_t *)senderMsg, corr.K * nOTs * sizeof(uint8_t));

  cout << "Sending: " << (uint32_t) senderMsg[0] << (uint32_t)senderMsg[1] << (uint32_t)senderMsg[2] <<(uint32_t) senderMsg[3] << endl;

  delete[] clientMsg;
  delete[] senderMsg;
}

void OTReceiverProtocol(CSocket *socket, uint8_t *outputs, const ReceiverOTCorrelation &corr,
                        uint32_t nOTs, const uint8_t *choiceBits)
{
  if (nOTs > corr.N)
  {
    throw "insufficient number of of OT correlations";
  }

  cout << "Value has: " << (uint32_t)corr.K << endl;

  uint8_t *clientMsg = new uint8_t[nOTs];
  ConstructOTReceiverMsg(clientMsg, corr, choiceBits, nOTs);

  cout << "Sending: " << (uint32_t) clientMsg[0] << endl;

  socket->SendLarge(clientMsg, nOTs * sizeof(uint8_t));

  uint8_t *serverMsg = new uint8_t[corr.K * nOTs];
  socket->ReceiveLarge((uint8_t *)serverMsg, corr.K * nOTs * sizeof(uint8_t));

  cout << "Recieved: " << (uint32_t) serverMsg[0] << (uint32_t)serverMsg[1] << (uint32_t)serverMsg[2] <<(uint32_t) serverMsg[3] << endl;

  RecoverReceiverMsg(outputs, corr, clientMsg, serverMsg, nOTs, 8, 8);

  delete[] clientMsg;
  delete[] serverMsg;
}

void CreateAndExportOTCorrelation(ofstream &out1, ofstream &out2, uint32_t N, uint8_t K, uint8_t bitsPerFSMROT, uint8_t bitsPerFSMSOT)
{
  SenderOTCorrelation senderOTC(N, K);
  ReceiverOTCorrelation receiverOTC(N, K);

  CreateOTCorrelation(senderOTC, receiverOTC, bitsPerFSMROT, bitsPerFSMSOT);

  senderOTC.WriteToFile(out1, bitsPerFSMSOT);
  receiverOTC.WriteToFile(out2, bitsPerFSMROT);
}

void SenderOTCorrelation::WriteToFile(ofstream &fout, uint8_t bitsPerFSM) const
{
  fout.write((char *)&N, sizeof(N));
  fout.write((char *)&K, sizeof(K));

  if (store_sOT_as_key == 1){
    fout.write((char *)msgKey.keyBytes, sizeof(msgKey.keyBytes));
  }
  else{

    uint8_t* packed = Pack(bitsPerFSM, N * K, blindingFactors);
    uint64_t newLength = packLength(N * K, bitsPerFSM);

    fout.write((char *)packed, sizeof(uint8_t) * newLength);

    delete[] packed;
  }
}

void SenderOTCorrelation::ReadFromFile(ifstream &fin, uint8_t bitsPerFSM)
{
  fin.read((char *)&N, sizeof(N));
  fin.read((char *)&K, sizeof(K));

  blindingFactors = new uint8_t[K * N];

  if (store_sOT_as_key == 1){

    uint8_t keyBuf[AESContext::AES_KEY_LEN];
    fin.read((char *)keyBuf, AESContext::AES_KEY_LEN);

    msgKey.Init(keyBuf);

    msgKey.FillBuffer((uint8_t *)blindingFactors, N * K * sizeof(uint8_t), 0);
  }
  else{
    uint64_t packedLength = packLength(N * K, bitsPerFSM);

    uint8_t *packedBlindingFactors = new uint8_t[packedLength];

    fin.read((char *)packedBlindingFactors, sizeof(uint8_t) * packedLength);

    Unpack(bitsPerFSM, N * K, packedBlindingFactors, blindingFactors);
    delete[] packedBlindingFactors;
  }
}

void ReceiverOTCorrelation::WriteToFile(ofstream &fout, uint8_t bitsPerFSM) const
{
  fout.write((char *)&N, sizeof(N));
  fout.write((char *)&K, sizeof(K));

  uint8_t* packed = Pack(bitsPerFSM, N, blindingFactors);
  uint64_t newLength = packLength(N, bitsPerFSM);

  fout.write((char *)packed, sizeof(uint8_t) * newLength);

  delete[] packed;
  
  fout.write((char *)choiceKey.keyBytes, sizeof(choiceKey.keyBytes));
}

void ReceiverOTCorrelation::ReadFromFile(ifstream &fin, uint8_t bitsPerFSM)
{

  fin.read((char *)&N, sizeof(N));
  fin.read((char *)&K, sizeof(K));

  uint64_t packedLength = packLength(N, bitsPerFSM);

  uint8_t *packedBlindingFactors = new uint8_t[packedLength];
  blindingFactors = new uint8_t[N];

  fin.read((char *)packedBlindingFactors, sizeof(uint8_t) * packedLength);

  Unpack(bitsPerFSM, N, packedBlindingFactors, blindingFactors);
  delete[] packedBlindingFactors;

  uint8_t keyBuf[AESContext::AES_KEY_LEN];
  fin.read((char *)keyBuf, AESContext::AES_KEY_LEN);  

  choiceKey.Init(keyBuf);


  ComputeChoiceBits();
}

void WriteProtocolArgs(ofstream &out1, struct ProtocolArgs args)
{
  out1.write((char *)&args.N, sizeof(args.N));
  out1.write((char *)&args.C, sizeof(args.C));
  out1.write((char *)&args.FSM1_rounds, sizeof(args.FSM1_rounds));
  out1.write((char *)&args.FSM2_rounds, sizeof(args.FSM2_rounds));

  for (int i = 0; i < args.FSM1_rounds; i++){
    out1.write((char *)&args.FSM1_steps[i], sizeof(uint8_t));
  }
  for (int i = 0; i < args.FSM2_rounds; i++){
    out1.write((char *)&args.FSM2_steps[i], sizeof(uint8_t));
  }
  for (int i = 0; i < args.C; i++){
    out1.write((char *)&args.bits_per_component[i], sizeof(uint8_t));
  }
  

}

ProtocolArgs::ProtocolArgs(ifstream &in)
{
  alloc_OT = false;

  in.read((char *)&N, sizeof(N));
  in.read((char *)&C, sizeof(C));
  in.read((char *)&FSM1_rounds, sizeof(FSM1_rounds));
  in.read((char *)&FSM2_rounds, sizeof(FSM2_rounds));

  FSM1_steps = new uint8_t[FSM1_rounds];
  for (int i = 0; i < FSM1_rounds; i++){
    in.read((char *)&FSM1_steps[i], sizeof(uint8_t));
  }

  FSM2_steps = new uint8_t[FSM2_rounds];
  for (int i = 0; i < FSM2_rounds; i++){
    in.read((char *)&FSM2_steps[i], sizeof(uint8_t));
  }
  bits_per_component = new uint8_t[C];
  for (int i = 0; i < C; i++){
    in.read((char *)&bits_per_component[i], sizeof(uint8_t));
  }
}

ProtocolArgs::~ProtocolArgs()
{
  //delete[] FSM1_steps;
  //delete[] FSM2_steps;
}

uint8_t* ReadInConfigFile(ifstream& in, uint32_t &C){
  string line1;
  getline(in, line1);
  string strC = line1.substr(line1.find("=") + 1, line1.length());
  C = atoi(strC.c_str());
  
  uint8_t *out_bits_per_component = new uint8_t[C];

  string line2;
  getline(in, line2);
  string strBits = line2.substr(line2.find("=") + 1, line2.length());
  stringstream ss(strBits);

  int index = 0;
  for (int i; ss >> i;) {
    out_bits_per_component[index] = i;
    index++;
    if (ss.peek() == ',' || ss.peek() == ' ')
      ss.ignore();
    }

  return out_bits_per_component;
}

SenderOTCorrelation** ReadInSenderOTsFSM1(ifstream &in, struct ProtocolArgs args)
{
  SenderOTCorrelation** FSM1 = new SenderOTCorrelation*[args.FSM1_rounds];

  for (int i = 0; i < args.FSM1_rounds; i++)
  {
    FSM1[i] = new SenderOTCorrelation(in, bits_FSM1_SOT);
  }
  return FSM1;

}

SenderOTCorrelation** ReadInSenderOTsFSM2(ifstream &in, struct ProtocolArgs args)
{
  SenderOTCorrelation** FSM2 = new SenderOTCorrelation*[args.FSM2_rounds];
  for (int i = 0; i < args.FSM2_rounds; i++)
  {
    FSM2[i] = new SenderOTCorrelation(in, bits_FSM2_SOT);
  }
  return FSM2;
}

ReceiverOTCorrelation** ReadInReceiverOTsFSM1(ifstream &in, struct ProtocolArgs args)
{
  args.alloc_OT = true;
  ReceiverOTCorrelation** FSM1 = new ReceiverOTCorrelation *[args.FSM1_rounds];
  for (int i = 0; i < args.FSM1_rounds; i++)
  {
    FSM1[i] = new ReceiverOTCorrelation(in, bits_FSM1);
  }
  return FSM1;
}
ReceiverOTCorrelation** ReadInReceiverOTsFSM2(ifstream &in, struct ProtocolArgs args)
{
  args.alloc_OT = true;
  ReceiverOTCorrelation** FSM2 = new ReceiverOTCorrelation *[args.FSM2_rounds];
  for (int i = 0; i < args.FSM2_rounds; i++)
  {
    FSM2[i] = new ReceiverOTCorrelation(in, bits_FSM2);
  }
  return FSM2;
}

uint8_t count_ones(uint8_t byte)
{
  static const uint8_t NIBBLE_LOOKUP[16] =
      {
          0, 1, 1, 2, 1, 2, 2, 3,
          1, 2, 2, 3, 2, 3, 3, 4};
  return NIBBLE_LOOKUP[byte & 0x0F] + NIBBLE_LOOKUP[byte >> 4];
}

uint32_t* ReadInEntry(ifstream& in, uint32_t C){
  string line;
  getline(in, line, '\n');

  istringstream f(line);
  string element;

  uint32_t* entry = new uint32_t[C];
  uint32_t i = 0;

  while (getline(f, element, ',')){
    entry[i] = (uint32_t) atoi(element.c_str());
    i += 1;
  }

  return entry;
}

uint32_t** ReadInDB(ifstream& in, uint32_t N, uint32_t C){
  uint32_t **db = new uint32_t *[N];
  for (int i = 0; i < N; i++)
  {
    db[i] = ReadInEntry(in, C);
  }
  return db;
}

void SendLargePacked(CSocket* socket, uint8_t bitsPerMsg, uint64_t length, uint8_t* data){
  //Assumes bitsPerMsg <= 8
  uint64_t newLength = packLength(length, bitsPerMsg);
  uint8_t* buffer = new uint8_t[newLength];
  memset(buffer, 0, newLength);
  
  uint8_t bitPointer = 0;
  uint64_t index = 0;

  for (int i = 0; i < length; i++){
    if (bitPointer + bitsPerMsg <= 8){
      buffer[index] += data[i] << bitPointer;
      bitPointer += bitsPerMsg;
      if (bitPointer == 8){
        index += 1;
        bitPointer = 0;
      }
    }
    else{
      uint8_t rightBits = 8 - bitPointer;
      uint8_t mask = (1 << rightBits) - 1;
      buffer[index] += (data[i] & mask) << bitPointer;
      
      index += 1;
      buffer[index] =  data[i] >> rightBits;
      bitPointer = bitsPerMsg - rightBits;
    }
  }

#if LOGGING
  struct timespec start, stop;
  uint64_t duration;
  clock_gettime(CLOCK_REALTIME, &start);
#endif

  socket->SendLarge(buffer, newLength);

#if LOGGING
  clock_gettime(CLOCK_REALTIME, &stop);
  duration = (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;
  cout << "Time to send: " << duration << " ns with size: " << newLength << endl;
#endif
  
  delete[] buffer;
}
void ReceiveLargePacked(CSocket* socket, uint8_t bitsPerMsg, uint64_t length, uint8_t* out){
  //Assumes bitsPerMsg <= 8
  uint64_t newLength = packLength(length, bitsPerMsg);
  uint8_t* buffer = new uint8_t[newLength];

#if LOGGING
  struct timespec start, stop;
  uint64_t duration;
  clock_gettime(CLOCK_REALTIME, &start);
#endif

  socket->ReceiveLarge(buffer, newLength);

#if LOGGING
  clock_gettime(CLOCK_REALTIME, &stop);
  duration = (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;
  cout << "Time to receive: " << duration << " ns with size: " << newLength << endl;
#endif
  
  uint8_t bitPointer = 0;
  uint64_t index = 0;

  uint8_t mask = (1 << bitsPerMsg) - 1;

  for (int i = 0; i < length; i++){
    if (bitPointer + bitsPerMsg <= 8){
      //REGULAR CASE
      out[i] = (buffer[index] >> bitPointer) & mask;
      bitPointer += bitsPerMsg;
      if (bitPointer == 8){
        index += 1;
        bitPointer = 0;
      }
    }
    else{
      //OVERLAP CASE
      uint8_t reconstructedValue = 0;

      uint8_t rightBits = 8 - bitPointer;
      reconstructedValue += buffer[index] >> bitPointer;

      index += 1;
      uint8_t leftBits = bitsPerMsg - rightBits;
      uint8_t newMask = (1 << leftBits) - 1;
      reconstructedValue += (buffer[index] & newMask) << rightBits;
      out[i] = reconstructedValue;

      bitPointer = leftBits;
    }
  }
  delete[] buffer;
}

uint64_t packLength(uint64_t length, uint8_t bitsPerMsg){
  return (length * bitsPerMsg + 8 - 1) / 8;
}

uint8_t *Pack(uint8_t bitsPerMsg, uint64_t length, uint8_t* data){
  //Assumes bitsPerMsg <= 8
  uint64_t newLength = packLength(length, bitsPerMsg);
  uint8_t* buffer = new uint8_t[newLength];
  memset(buffer, 0, newLength);
  
  uint8_t bitPointer = 0;
  uint64_t index = 0;

  for (int i = 0; i < length; i++){
    if (bitPointer + bitsPerMsg <= 8){
      buffer[index] += data[i] << bitPointer;
      bitPointer += bitsPerMsg;
      if (bitPointer == 8){
        index += 1;
        bitPointer = 0;
      }
    }
    else{
      uint8_t rightBits = 8 - bitPointer;
      uint8_t mask = (1 << rightBits) - 1;
      buffer[index] += (data[i] & mask) << bitPointer;
      
      index += 1;
      buffer[index] =  data[i] >> rightBits;
      bitPointer = bitsPerMsg - rightBits;
    }
  }
  return buffer;
}
uint8_t *Unpack(uint8_t bitsPerMsg, uint64_t length, uint8_t* data, uint8_t* out){
  //Assumes bitsPerMsg <= 8
  uint64_t newLength = packLength(length, bitsPerMsg);
  
  uint8_t bitPointer = 0;
  uint64_t index = 0;

  uint8_t mask = (1 << bitsPerMsg) - 1;

  for (int i = 0; i < length; i++){
    if (bitPointer + bitsPerMsg <= 8){
      //REGULAR CASE
      out[i] = (data[index] >> bitPointer) & mask;
      bitPointer += bitsPerMsg;
      if (bitPointer == 8){
        index += 1;
        bitPointer = 0;
      }
    }
    else{
      //OVERLAP CASE
      uint8_t reconstructedValue = 0;

      uint8_t rightBits = 8 - bitPointer;
      reconstructedValue += data[index] >> bitPointer;

      index += 1;
      uint8_t leftBits = bitsPerMsg - rightBits;
      uint8_t newMask = (1 << leftBits) - 1;
      reconstructedValue += (data[index] & newMask) << rightBits;
      out[i] = reconstructedValue;

      bitPointer = leftBits;
    }
  }
  return out;
}