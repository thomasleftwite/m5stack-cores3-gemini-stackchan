#include "../firmware/src/ConversationRules.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <stdio.h>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) return 2;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    conversation::WavView view;
    bool valid = conversation::validateWav(bytes.data(), bytes.size(), view);
    printf("{\"firmware_wav_accepted\":%s,\"wav_bytes\":%zu,\"pcm_offset\":%zu,\"pcm_bytes\":%zu}\n",
           valid ? "true" : "false", bytes.size(), view.offset, view.bytes);
    return valid ? 0 : 1;
}
