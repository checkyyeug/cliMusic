#include <iostream>
#pragma pack(push, 1)
struct DSFHeader {
    char id[4];              // 'D', 'S', 'D', ' '
    uint64_t chunk_size;     // Total chunk size (always 28)
    uint64_t file_size;      // Total file size
};
#pragma pack(pop)
int main() {
    std::cout << "sizeof(DSFHeader) = " << sizeof(DSFHeader) << std::endl;
    return 0;
}
