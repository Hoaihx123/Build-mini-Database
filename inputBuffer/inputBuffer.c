#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include <stdbool.h>
#include "inputBuffer.h"

InputBuffer* new_inp_buf(){
  InputBuffer* inp_buf = (InputBuffer*)malloc(sizeof(InputBuffer));
  inp_buf->buffer = NULL;
  inp_buf->buffer_length = 0;
  inp_buf->input_length = 0;
  return inp_buf;
}

void close_input_buffer(InputBuffer* inp_buf) {
  free(inp_buf->buffer);
  free(inp_buf);
}

void read_input(InputBuffer* inp_buf) {
  ssize_t byte_inp = getline(&(inp_buf->buffer), &(inp_buf->buffer_length), stdin);
  if (byte_inp == -1){
    printf("Error reading buffer");
    return;
  }
  inp_buf->input_length = byte_inp-1;
  inp_buf->buffer[byte_inp-1] = 0;
}
