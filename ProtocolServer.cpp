#include <iostream>
#include <sstream>
#include <math.h>

#include "protocol.h"
#include "util.h"

using namespace std;

void RunServerProtocol(CSocket *socket, string pArgsFile, string serverDataDir)
{
  ServerLog("starting server protocol");
#if !OPTIMAL
  ServerLog("Reading in protocol args");
#endif

  ifstream in;
  in.open(pArgsFile, ios::in | ios::binary);
  struct ProtocolArgs pArgs(in);
  in.close();

#if !OPTIMAL
  ServerLog("Reading in sender OTs");
#endif

  ifstream sOTFile1;
  sOTFile1.open(serverDataDir + "sOT.txt", ios::in | ios::binary);

  SenderOTCorrelation **FSM1_SC = ReadInSenderOTsFSM1(sOTFile1, pArgs);
  SenderOTCorrelation **FSM2_SC = ReadInSenderOTsFSM2(sOTFile1, pArgs);

  sOTFile1.close();

#if !OPTIMAL
  ServerLog("Finished Reading in OTs");
  ServerLog("Creating fake DB");
#endif

  uint32_t N = pArgs.N;
  uint32_t C = pArgs.C;
  uint8_t* bits_per_component = pArgs.bits_per_component;

  ifstream dbFile;
  dbFile.open(serverDataDir + "db.txt");
  uint32_t **db = ReadInDB(dbFile, N, C);
  dbFile.close();

#if VERBOSE
  for (int i = 0; i < N; i++)
  {
    for (int j = 0; j < C; j++)
    {
      cout << db[i][j] << ", ";
    }
    cout << endl;
  }
#endif

#if !OPTIMAL
  ServerLog("Creating random permutations");
#endif

  AESContext PRF;

  uint16_t **flipped_bits = new uint16_t *[N];
  for (int i = 0; i < N; i++)
  {
    flipped_bits[i] = new uint16_t[C];
    PRF.FillBuffer((uint8_t *)flipped_bits[i], 2 * C, i);
  }

#if !OPTIMAL
  ServerLog("Running first part FSM");
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
    uint8_t number_of_bit_entries = C - smallest_number_of_bits;

    uint8_t *msgs = new uint8_t[N * number_of_bit_entries * 2 * pow_iterations];

    for (int i = 0; i < N; i++)
    {
      for (int j = 0; j < number_of_bit_entries; j++)
      {
        uint8_t postImageJ = j + C - number_of_bit_entries;

        bool previous_reject = round == 0 ? 1 : !(flipped_bits[i][postImageJ] >> (round - 1) & 0x1);
        bool next_flipped_bit = (flipped_bits[i][postImageJ] >> round) & 0x1;

        uint8_t entry_bits = (db[i][postImageJ] >> l) & all_ones_string;

        for (int state = 0; state < pos_states; state++)
        {
          if (state < (pos_states / 2))
          {
            msgs[number_of_bit_entries * pos_states * i + j * pos_states + state] = (previous_reject != 0 && entry_bits == (state % pow_iterations)) ? next_flipped_bit : !next_flipped_bit;
          }
          else
          {
            msgs[number_of_bit_entries * pos_states * i + j * pos_states + state] = (previous_reject != 1 && entry_bits == (state % pow_iterations)) ? next_flipped_bit : !next_flipped_bit;
          }
        }
      }
    }

#if !OPTIMAL
    ServerLog("Finished creating messages");
#endif

    uint8_t *rMsg = new uint8_t[N * number_of_bit_entries];

    ReceiveLargePacked(socket, 1 + concurrent_iterations, N * number_of_bit_entries, rMsg);

    uint8_t *sMsg = new uint8_t[N * number_of_bit_entries * pos_states];
    ConstructOTSenderMsg(sMsg, *(FSM1_SC[round]), msgs, rMsg, N * number_of_bit_entries, 1);

    SendLargePacked(socket, 1, N * number_of_bit_entries * pos_states, sMsg);

    delete[] msgs;
    delete[] rMsg;
    delete[] sMsg;

    l += concurrent_iterations;
  }

#if !OPTIMAL
  ServerLog("Finished part 1 of the FSM");
#endif
  uint8_t highest_number_of_bits = bits_per_component[C - 1];
  uint8_t **FinalRandomness = new uint8_t *[N];
  for (int i = 0; i < N; i++)
  {
    FinalRandomness[i] = new uint8_t[C];
    for (int j = 0; j < C; j++)
    {
      FinalRandomness[i][j] = (flipped_bits[i][j] >> (pArgs.FSM1_rounds - 1 - ((highest_number_of_bits - bits_per_component[j]) / 2))) & 0x1;
    }
  }

#if !OPTIMAL
  ServerLog("Starting Second FSM");
#endif

  AESContext PRF2;

  uint8_t **random_shift = new uint8_t *[N];
  uint32_t tag_counter = 0;
  for (int i = 0; i < N; i++)
  {
    random_shift[i] = new uint8_t[pArgs.FSM2_rounds];
    PRF2.FillBuffer(random_shift[i], pArgs.FSM2_rounds, tag_counter);
    for (int j = 0; j < pArgs.FSM2_rounds; j++)
    {
      while (random_shift[i][j] == 255)
      {
        tag_counter += 1;
        PRF2.FillBuffer(&random_shift[i][j], 1, tag_counter);
      }
      random_shift[i][j] %= 3;
    }
    tag_counter += 1;
  }

  for (int i = 0; i < N; i++)
    random_shift[i][pArgs.FSM2_rounds - 1] = 0; // Make sure that the final permutation is always 0 to reveal the true state to the client

  uint32_t c = 0;
  for (int round = 0; round < pArgs.FSM2_rounds; round++)
  {
    uint8_t round_step = pArgs.FSM2_steps[round];
    uint8_t round_factor = 1 << round_step;

    uint8_t *msgs = new uint8_t[N * 3 * round_factor];
    for (int i = 0; i < N; i++)
    {
      int previous_shift = (c == 0) ? 0 : random_shift[i][round - 1];
      for (int possible_state = 0; possible_state < 3; possible_state++)
      {
        uint8_t old_state = (possible_state + 3 - previous_shift) % 3;

        uint8_t equality_bits = 0;
        for (int j = 0; j < round_step; j++)
        {
          equality_bits += (FinalRandomness[i][c + j] * (1 << j));
        }

        // equality bits = the best correct state -> having it correct is plus FSM_RC
        // ~equality bits = worst case -> having it correct is plus 0
        //
        for (uint8_t j = 0; j < round_factor; j++)
        {
          uint8_t no_match_count = round_step - count_ones(((~(j ^ equality_bits)) & (round_factor - 1)));
          if (old_state - no_match_count < 0)
            msgs[((3 * round_factor) * i + possible_state * round_factor + j)] = (0 + random_shift[i][round]);
          else
            msgs[((3 * round_factor) * i + possible_state * round_factor + j)] = (old_state - no_match_count + random_shift[i][round]) % 3;
  
        }
      }

    }

#if !OPTIMAL
    ServerLog("Finished one round");
#endif

    

    uint8_t *rMsg = new uint8_t[N];

    ReceiveLargePacked(socket, 2 + round_step, N, rMsg);
    
    uint8_t *sMsg = new uint8_t[N * 3 * round_factor];
    ConstructOTSenderMsg(sMsg, *(FSM2_SC[round]), msgs, rMsg, N, 2);

    SendLargePacked(socket, 2, N * 3 * round_factor, sMsg);

    c += round_step;

    delete[] msgs;
    delete[] rMsg;
    delete[] sMsg;
  }

#if !OPTIMAL
  ServerLog("finished everything");
  cout << "Memory clean up" << endl;
#endif

  for (int i = 0; i < N; i++)
  {
    delete[] db[i];
    delete[] flipped_bits[i];
    delete[] random_shift[i];
    delete[] FinalRandomness[i];
  }
  delete[] db;
  delete[] flipped_bits;
  delete[] random_shift;
  delete[] FinalRandomness;

  for (int i = 0; i < pArgs.FSM1_rounds; i++)
  {
    delete FSM1_SC[i];
  }
  delete[] FSM1_SC;

  for (int i = 0; i < pArgs.FSM2_rounds; i++)
  {
    delete FSM2_SC[i];
  }
  delete[] FSM2_SC;
}

void StartServer(int port, string pArgsFile, string serverDataDir)
{
  CSocket *socket = new CSocket();
  if (Listen(socket, port))
  {
    ServerLog("accepted connection from client");
  }
  else
  {
    stringstream ss;
    ss << "failed to listen for client connections on port " << port;
    ServerLog(ss.str());
    return;
  }

  RunServerProtocol(socket, pArgsFile, serverDataDir);

  ServerLog("protocol execution completed");

  cout << endl
       << "bytes sent: " << socket->GetBytesSent() << endl;
  cout << "bytes received: " << socket->GetBytesReceived() << endl;

  socket->Close();

  delete socket;
}

int main(int argc, char **argv)
{
  if (argc < 4)
  {
    cout << "usage: ./ProtocolServer ProtocolArgsFile serverDataDir port" << endl;
    return 1;
  }

  srand(time(NULL));

  ServerLog("finished loading data from disk");

  int port = atoi(argv[3]);

  string pArgsFile(argv[1]);
  string serverDataDir(argv[2]);

  StartServer(port, pArgsFile, serverDataDir);
}
