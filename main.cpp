#include <PdfToEpub.h>
#include <cstdio>
int main(int argc, char** argv) {
  std::string err;
  std::string out = PdfToEpub::ensureConverted(argv[1], &err);
  if (out.empty()) { fprintf(stderr, "FAILED: %s\n", err.c_str()); return 1; }
  printf("OK: %s\n", out.c_str());
  return 0;
}
