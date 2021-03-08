#include <iostream>
#include "protocol.h"
#include "aes.h"
#include <math.h>

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cout << "usage: ./GenerateOTs N config-file" << endl;
        return 1;
    }

    ofstream protocolArgsFile;
    protocolArgsFile.open("./data/info.txt", ios::out | ios::binary);

    ofstream senderOTs1;
    senderOTs1.open("./data/server/sOT.txt", ios::out | ios::binary);

    size_t senderOTFileBytes = senderOTs1.tellp();

    ofstream recieverOTs1;
    recieverOTs1.open("./data/client/rOT.txt", ios::out | ios::binary);
    
    size_t recieverOTFileBytes = recieverOTs1.tellp();

    uint32_t N = atoi(argv[1]);
    char* configFile = argv[2];

    ifstream configReader;
    configReader.open(configFile, ios::in);
    
    uint32_t C;
    uint8_t* bits_per_component = ReadInConfigFile(configReader, C);
    

    uint64_t number_of_OT_correlations = 0;
    uint8_t highest_number_bits = bits_per_component[C - 1];
    uint32_t number_rounds_FSM1 = highest_number_bits / FSM1_StepSize + (highest_number_bits % FSM1_StepSize != 0);
    uint8_t iteration_count_arr_FSM1[number_rounds_FSM1];

    int l = 0;
    int smallest_number_of_bits = 0;
    for (int i = 0; i < number_rounds_FSM1; i++)
    {
        if (l >= bits_per_component[smallest_number_of_bits])
        {
            while(l >= bits_per_component[smallest_number_of_bits])
                smallest_number_of_bits ++;
        }

        int round_step = l + FSM1_StepSize <= highest_number_bits ? FSM1_StepSize : 1;
        iteration_count_arr_FSM1[i] = round_step;

        uint8_t num_states = 2 * pow(2, round_step);
        CreateAndExportOTCorrelation(senderOTs1, recieverOTs1, N * (C - smallest_number_of_bits), num_states, bits_FSM1, bits_FSM1_SOT);
        number_of_OT_correlations += N * (C - smallest_number_of_bits);
        l += round_step;
    }

    uint32_t number_rounds_FSM2 = C / FSM2_StepSize + (C % FSM2_StepSize != 0);

    uint8_t iteration_count_arr_FSM2[number_rounds_FSM2];

    uint8_t count = 0;
    for (int i = 0; i < number_rounds_FSM2; i++)
    {
        int round_step = count + FSM2_StepSize <= C ? FSM2_StepSize : 1;

        iteration_count_arr_FSM2[i] = round_step;
        uint8_t num_states = pow(2, round_step);

        CreateAndExportOTCorrelation(senderOTs1, recieverOTs1, N, 3 * num_states, bits_FSM2, bits_FSM2_SOT);
        number_of_OT_correlations += N;
        count += round_step;
    }

    struct ProtocolArgs pArgs(N, C, iteration_count_arr_FSM1, iteration_count_arr_FSM2, number_rounds_FSM1, number_rounds_FSM2, bits_per_component);

    WriteProtocolArgs(protocolArgsFile, pArgs);

    senderOTFileBytes = (size_t)senderOTs1.tellp() - senderOTFileBytes;
    recieverOTFileBytes = (size_t)recieverOTs1.tellp() - recieverOTFileBytes;
    
    cout << "Number of OT correlations: " << number_of_OT_correlations << endl;
    
    recieverOTs1.close();
    senderOTs1.close();
    protocolArgsFile.close();
}
