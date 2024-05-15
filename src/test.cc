#include <fmt/core.h>
#include <fmt/ranges.h>
#include <vector>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

int main()
{
  std::string test(16 + 100, '\0');
  char * ptr = test.data();
  memcpy(ptr, "firs", 4);
  memset(ptr + 4, 7, 4);
  memset(ptr + 8, 0, 4);
  memcpy(ptr + 12, "seco", 4);
  memset(ptr + 16, 0xaa, 100);
  SPDLOG_ERROR("Binary example: {:a}", spdlog::to_hex(test));
}
