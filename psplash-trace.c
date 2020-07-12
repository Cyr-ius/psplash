#include "psplash.h"
#include "psplash-trace.h"

static void write_trace_file (FILE * fichier, const char * msg, va_list args);
static void close_trace_file(FILE *fichier);

static void write_trace_file (FILE * fichier, const char * msg, va_list args)
{
 vfprintf (fichier, msg, args);
}

static void close_trace_file(FILE *fichier) {
  fclose (fichier);
}

FILE *
open_trace_file(char *pathFile) {
  FILE *file = NULL;
  if (pathFile == NULL) {
    pathFile = PATHFILE;
  }
  char *nameFile  = TRACEFILE;
  char pathTrace[255];
  strcpy(pathTrace,pathFile);
  strcat(pathTrace,nameFile);
  file = fopen(pathTrace, "a");
  if (file ==NULL) {
    printf("[ERROR] : trace file not created");
    exit(-100);
  } 
  return file;
}

void psplash_trace(char *msg, ...) {
  va_list arguments;
  va_start(arguments, msg);
  FILE *fichier = NULL;
  fichier = open_trace_file(NULL);
  write_trace_file (fichier, msg, arguments);
  va_end(arguments);
  close_trace_file (fichier);
}
