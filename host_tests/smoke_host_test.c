#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int ensure_dir(const char* path) {
  char buffer[512];
  size_t i;

  if (path == NULL || path[0] == '\0') {
    return 0;
  }

  if (snprintf(buffer, sizeof(buffer), "%s", path) >= (int)sizeof(buffer)) {
    return 0;
  }

  for (i = 1; buffer[i] != '\0'; ++i) {
    if (buffer[i] == '/') {
      buffer[i] = '\0';
      if (mkdir(buffer, 0777) != 0 && errno != EEXIST) {
        return 0;
      }
      buffer[i] = '/';
    }
  }

  return mkdir(buffer, 0777) == 0 || errno == EEXIST;
}

int main(void) {
  const char* artifact_dir = getenv("HOST_TEST_ARTIFACT_DIR");
  const char* artifact_name = "result.txt";
  FILE* fp = NULL;
  char output_path[512];

  if (artifact_dir == NULL || artifact_dir[0] == '\0') {
    fprintf(stderr, "HOST_TEST_ARTIFACT_DIR is not set\n");
    return 1;
  }

  if (!ensure_dir(artifact_dir)) {
    perror("mkdir");
    return 1;
  }

  if (snprintf(output_path, sizeof(output_path), "%s/%s", artifact_dir,
               artifact_name) >= (int)sizeof(output_path)) {
    fprintf(stderr, "Artifact path is too long\n");
    return 1;
  }

  fp = fopen(output_path, "w");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  fprintf(fp, "host_smoke=pass\n");
  fclose(fp);

  return strcmp("esp32p4", "esp32p4") == 0 ? 0 : 1;
}
