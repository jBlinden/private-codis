#include <iostream>
#include <sstream>
#include <math.h>
#include <chrono>

#include "protocol.h"
#include "util.h"

using namespace std;

void RunClientProtocol(CSocket *socket, string pArgsFile, string clientDataDir)
{
  ClientLog("starting client protocol");

#if !OPTIMAL
  ClientLog("Reading in protocol arguments");
#endif

  ifstream in;
  in.open(pArgsFile, ios::in | ios::binary);
  struct ProtocolArgs pArgs(in);

  in.close();

#if !OPTIMAL
  ClientLog("Reading in receiver OTs");
#endif

  ifstream rOTFile1;
  rOTFile1.open(clientDataDir + "rOT.txt", ios::in | ios::binary);

  ReceiverOTCorrelation **FSM1_RC = ReadInReceiverOTsFSM1(rOTFile1, pArgs);
  ReceiverOTCorrelation **FSM2_RC = ReadInReceiverOTsFSM2(rOTFile1, pArgs);

  rOTFile1.close();

#if !OPTIMAL
  ClientLog("Finished Reading in OTs");
#endif

  uint32_t N = pArgs.N;
  uint32_t C = pArgs.C;
  uint8_t* bits_per_component = pArgs.bits_per_component;

#if !OPTIMAL
  cout << "N: " << N << " C: " << C << endl;
#endif

  ifstream queryFile;
  queryFile.open(clientDataDir + "query.txt");
  uint32_t *query = ReadInEntry(queryFile, C);
  queryFile.close();

#if VERBOSE
  cout << "Query: ";
  for (int i = 0; i < C; i++)
  {
    cout << query[i] << " ";
  }
  cout << endl;
#endif

#if !OPTIMAL
  ClientLog("Creating clients states");
#endif

  uint8_t **client_states = new uint8_t *[N];
  for (int i = 0; i < N; i++)
  {
    client_states[i] = new uint8_t[C];
    for (int j = 0; j < C; j++)
    {
      client_states[i][j] = 0; //Default state is 0
    }
  }

#if !OPTIMAL
  ClientLog("Starting first FSM");

  cout << pArgs.FSM1_rounds << endl;
  for (int i = 0; i < pArgs.FSM1_rounds; i++)
  {
    cout << i << " : " << (uint32_t)pArgs.FSM1_steps[i] << endl;
  }
#endif

  int l = 0;
  int smallest_number_of_bits = 0;
  for (int round = 0; round < pArgs.FSM1_rounds; round++)
  {
    uint8_t concurrent_iterations = pArgs.FSM1_steps[round];
    uint8_t all_ones_string = (uint8_t)((1 << concurrent_iterations) - 1);
    uint8_t pow_iterations = (uint8_t)((1 << concurrent_iterations));
    uint8_t pos_states = 2 * pow_iterations;

    if (l >= bits_per_component[smallest_number_of_bits])
    {
      while(l >= bits_per_component[smallest_number_of_bits])
        smallest_number_of_bits ++;
    }
    uint8_t number_of_higher_bit_entries = C - smallest_number_of_bits;
    uint8_t *choices = new uint8_t[N * number_of_higher_bit_entries * concurrent_iterations];
    
    for (int i = 0; i < N; i++)
    {
      for (int j = 0; j < number_of_higher_bit_entries; j++)
      {
          
        uint8_t postImageJ = j + C - number_of_higher_bit_entries;
        uint8_t query_bits = (query[postImageJ] >> l) & all_ones_string;
        choices[number_of_higher_bit_entries * i + j] = pow_iterations * client_states[i][postImageJ] + query_bits;
      }
    }

#if !OPTIMAL
      ClientLog("Created Choice bits");
#endif
  
      
    uint8_t *rMsg = new uint8_t[N * number_of_higher_bit_entries];
    ConstructOTReceiverMsg(rMsg, *(FSM1_RC[round]), choices, N * number_of_higher_bit_entries);

      
    SendLargePacked(socket, 1 + concurrent_iterations, N * number_of_higher_bit_entries, rMsg);

   
    uint8_t *sMsg = new uint8_t[N * number_of_higher_bit_entries * pos_states];
    ReceiveLargePacked(socket, 1, N * number_of_higher_bit_entries * pos_states, sMsg);

      
    uint8_t *msgs2 = new uint8_t[N * number_of_higher_bit_entries];
    RecoverReceiverMsg(msgs2, *(FSM1_RC[round]), choices, sMsg, N * number_of_higher_bit_entries, 1, bits_FSM1);

#if !OPTIMAL
      ClientLog("Finished One Round");
#endif
    for (int i = 0; i < N; i++)
    {
      for (int j = 0; j < number_of_higher_bit_entries; j++)
      {
        client_states[i][j + C - number_of_higher_bit_entries] = msgs2[number_of_higher_bit_entries * i + j];
      }
    }
    delete[] choices;
    delete[] msgs2;
    delete[] rMsg;
    delete[] sMsg;

    l += concurrent_iterations;
  }

#if !OPTIMAL
  ClientLog("Finished first FSM");
#endif

#if VERBOSE
  for (int i = 0; i < N; i++)
  {
    cout << "Database entry: " << i << endl;
    cout << "DB   : ";
    for (int j = 0; j < C; j++)
    {
      cout << 0 << ", ";
    }
    cout << endl;
    cout << "Query: ";
    for (int j = 0; j < C; j++)
    {
      cout << query[j] << ", ";
    }
    cout << endl;
    cout << "Equal: ";
    for (int j = 0; j < C; j++)
    {
      cout << (uint32_t)client_states[i][j] << ", ";
    }
    cout << endl;
    cout << "Actual: ";
    for (int j = 0; j < C; j++)
    {
      uint32_t value = (uint32_t)client_states[i][j];
      uint8_t accept_state = 0;

      if (accept_state == 1)
      {
        cout << value << ", ";
      }
      if (accept_state == 0)
      {
        cout << !value << ", ";
      }
    }
    cout << endl;
  }
#endif
#if !OPTIMAL
  ClientLog("Starting Second FSM");

  cout << "Finished with 1st part of OT correlation calculations" << endl;
  cout << "Setting up second FSM to compute k matches" << endl;

#endif

  uint8_t *client_k_state = new uint8_t[N];
  for (int i = 0; i < N; i++)
  {
    client_k_state[i] = 2; // default state
  }
  uint32_t c = 0;
  for (int round = 0; round < pArgs.FSM2_rounds; round++)
  {
    uint8_t round_step = pArgs.FSM2_steps[round];
    uint8_t round_factor = 1 << round_step;
    uint8_t *choices = new uint8_t[N];
    for (int i = 0; i < N; i++)
    {
      // (client state, components equal)
      uint8_t client_state_value = 0;
      for (int j = 0; j < round_step; j++)
      {
        client_state_value += client_states[i][c + j] * (1 << j);
      }
      choices[i] = round_factor * client_k_state[i] + client_state_value;

    }

#if !OPTIMAL
    ClientLog("One one rounds");
#endif


    uint8_t *rMsg = new uint8_t[N];
    ConstructOTReceiverMsg(rMsg, *(FSM2_RC[round]), choices, N);

    SendLargePacked(socket, 2 + round_step, N, rMsg);
    
#if !OPTIMAL
    cout << "Sending: " << N << endl;
#endif

    uint8_t *sMsg = new uint8_t[N * 3 * round_factor];

    ReceiveLargePacked(socket, 2, N * 3 * round_factor, sMsg);
    

    uint8_t *msgs2 = new uint8_t[N];
    RecoverReceiverMsg(msgs2, *(FSM2_RC[round]), choices, sMsg, N, 2, bits_FSM2);

    for (int i = 0; i < N; i++)
    {
      client_k_state[i] = msgs2[i];
    }

    c += round_step;

    delete[] choices;
    delete[] msgs2;
    delete[] rMsg;
    delete[] sMsg;
  }
#if !OPTIMAL
  ClientLog("Finished second FSM");
#endif
  for (int i = 0; i < N; i++)
  {
    if (client_k_state[i] > 0)
      ClientLog("found a match at index " + to_string(i));
  }

#if !OPTIMAL
  cout << "Memory clean up" << endl;
#endif

  delete[] query;
  for (int i = 0; i < N; i++)
  {
    delete[] client_states[i];
  }
  delete[] client_states;
  delete[] client_k_state;

  for (int i = 0; i < pArgs.FSM1_rounds; i++)
  {
    delete FSM1_RC[i];
  }
  delete[] FSM1_RC;

  for (int i = 0; i < pArgs.FSM2_rounds; i++)
  {
    delete FSM2_RC[i];
  }
  delete[] FSM2_RC;
}

void StartClient(const char *address, int port, string pArgsFile, string clientDataDir)
{
  CSocket *socket = new CSocket();

  if (Connect(socket, address, port))
  {
    ClientLog("successfully connected to server");
  }
  else
  {
    stringstream ss;
    ss << "unable to connect to port " << port << " on address " << address;
    ClientLog(ss.str());
    return;
  }

  auto t_start = chrono::high_resolution_clock::now();

  RunClientProtocol(socket, pArgsFile, clientDataDir);

  auto t_end = chrono::high_resolution_clock::now();
  chrono::duration<float> duration = t_end - t_start;

  ClientLog("protocol execution completed");

  cout << "protocol execution time: " << duration.count() << " seconds" << endl;
  cout << "total bytes sent: " << socket->GetBytesSent() << endl;
  cout << "total bytes received: " << socket->GetBytesReceived() << endl;
  cout << "total bytes communicated (received + sent): " << socket->GetBytesSent() + socket->GetBytesReceived() << endl;


  socket->Close();

  delete socket;
}

int main(int argc, const char **argv)
{
  if (argc < 5)
  {
    cout << "usage: ./ProtocolClient ProtocolArgsFile clientDataDir addr port" << endl;
    return 1;
  }

  srand(time(NULL));

  int port = atoi(argv[4]);

  string pArgsFile(argv[1]);
  string clientDataDir(argv[2]);

  StartClient(argv[3], port, pArgsFile, clientDataDir);
}
