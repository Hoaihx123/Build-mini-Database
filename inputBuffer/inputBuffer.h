typedef struct {
  char* buffer;
  size_t buffer_length;
  ssize_t input_length;
} InputBuffer;
InputBuffer* new_inp_buf();
void close_input_buffer(InputBuffer* inp_buf);
void read_input(InputBuffer* inp_buf);
