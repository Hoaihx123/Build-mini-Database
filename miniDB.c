#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include <stdbool.h>
#include "inputBuffer/inputBuffer.h"
#include <fcntl.h>
#include <unistd.h>

#define TABLE_MAX_PAGE 100
#define INVALID_PAGE_NUM UINT32_MAX

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT} StatementType;
typedef enum { NODE_LEAF, NODE_INTERNAL} NodeType;

typedef struct{
  uint32_t file_length;
  int file_des;
  uint32_t num_pages;
  void* pages[TABLE_MAX_PAGE];
} Pager;
typedef struct {
  Pager* pager;
  uint32_t root_page_num;
} Table;
typedef struct {
  uint32_t id;
  char user_name[32];
  char email[256];
} Row;
typedef struct {
  StatementType type;
  Row row_to_insert;
} Statement;
typedef struct{
  Table* table;
  uint32_t page_num;
  uint32_t cell_num;
  bool end_of_table;
} Cursor;

const uint32_t PAGES_SIZE = 4096;
const uint32_t ID_SIZE = sizeof(((Row*)0)->id);
const uint32_t USERNAME_SIZE = sizeof(((Row*)0)->user_name);
const uint32_t EMAIL_SIZE = sizeof(((Row*)0)->email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_SIZE+ID_OFFSET;
const uint32_t EMAIL_OFFSET= USERNAME_SIZE+USERNAME_OFFSET;
const uint32_t ROW_SIZE = USERNAME_SIZE+ID_SIZE+EMAIL_SIZE;

const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE+NODE_TYPE_OFFSET;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET+IS_ROOT_SIZE;
const uint32_t NODE_HEADER_SIZE = NODE_TYPE_SIZE+IS_ROOT_SIZE+PARENT_POINTER_SIZE;

const uint32_t LEAF_NODE_NCELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NCELLS_OFFSET = NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = NODE_HEADER_SIZE+LEAF_NODE_NCELLS_SIZE;
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_VALUE_SIZE+LEAF_NODE_KEY_SIZE;
// const uint32_t LEAF_NODE_MAX_CELLS = (PAGES_SIZE-LEAF_NODE_HEADER_SIZE)/LEAF_NODE_CELL_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS = 5;

const uint32_t LEAF_NODE_CELLS_LEFT = (LEAF_NODE_MAX_CELLS+1)/2;
const uint32_t LEAF_NODE_CELLS_RIGHT = LEAF_NODE_MAX_CELLS-LEAF_NODE_CELLS_LEFT+1;

const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_SIZE+INTERNAL_NODE_NUM_KEYS_OFFSET;
const uint32_t INTERNAL_NODE_HEADER_SIZE = INTERNAL_NODE_RIGHT_CHILD_SIZE+INTERNAL_NODE_RIGHT_CHILD_OFFSET;
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILL_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_KEY_SIZE+INTERNAL_NODE_CHILL_SIZE;
// const uint32_t INTERNAL_NODE_MAX_CELLS = (PAGES_SIZE-INTERNAL_NODE_HEADER_SIZE)/INTERNAL_NODE_CELL_SIZE;
const uint32_t INTERNAL_NODE_MAX_CELLS = 3;
const uint32_t INTERNAL_NODE_CELLS_LEFT = (INTERNAL_NODE_MAX_CELLS+1)/2;
const uint32_t INTERNAL_NODE_CELLS_RIGHT = INTERNAL_NODE_MAX_CELLS-INTERNAL_NODE_CELLS_LEFT;

NodeType node_type(void* node){
  return (NodeType)(*(uint8_t*)(node));
}
void set_node_type(void* node, NodeType type) {
  *(uint8_t*)node = (uint8_t)type;
}
bool is_node_root(void* node){
  return (bool)*((uint8_t*)(node+IS_ROOT_OFFSET));
}
void set_node_root(void* node, bool is_root){
  *(uint8_t*)(node+IS_ROOT_OFFSET) = (uint8_t)is_root;
}
uint32_t* get_parent(void* page){
  return page + PARENT_POINTER_OFFSET;
}
uint32_t* leaf_node_num_cells(void* node){
  return node+LEAF_NODE_NCELLS_OFFSET;
}
void* leaf_node_cell(void* node, uint32_t cell_num){
  return node+LEAF_NODE_HEADER_SIZE+LEAF_NODE_CELL_SIZE*cell_num;
}
uint32_t* leaf_node_key(void* node, uint32_t cell_num){
  return leaf_node_cell(node, cell_num);
}
void* leaf_node_value(void* node, uint32_t cell_num){
  return leaf_node_cell(node, cell_num)+LEAF_NODE_VALUE_OFFSET;
}
uint32_t* internal_node_num_key(void* node){
  return node+INTERNAL_NODE_NUM_KEYS_OFFSET;
}
uint32_t* internal_node_right_child(void* node){
  return node+INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}
void* internal_node_cell(void* node, uint32_t cell_num){
  return node+INTERNAL_NODE_HEADER_SIZE+INTERNAL_NODE_CELL_SIZE*cell_num;
}
uint32_t* internal_node_key(void* node, uint32_t cell_num){
  return internal_node_cell(node, cell_num) + INTERNAL_NODE_CHILL_SIZE;
}
uint32_t* internal_node_child(void* node, uint32_t cell_num){
  uint32_t num_key = *internal_node_num_key(node);
  if (num_key<cell_num){
    exit(EXIT_FAILURE);
  } else if (num_key==cell_num){
    return internal_node_right_child(node);
  } else{
    return internal_node_cell(node, cell_num);
  }
}
void initialize_leaf_node(void* node){
  set_node_type(node, NODE_LEAF);
  *leaf_node_num_cells(node) = 0;
  set_node_root(node, false);
}
void initialize_internal_node(void* node){
  set_node_type(node, NODE_INTERNAL);
  *internal_node_num_key(node)=0;
  set_node_root(node, false);
  *internal_node_right_child(node)=INVALID_PAGE_NUM;
}

Pager* pager_open(const char* filename){
  int fd = open(filename, O_RDWR|O_CREAT, S_IWUSR|S_IRUSR);
  if (fd==-1){
    printf("Error open file\n");
    exit(EXIT_FAILURE);
  }
  Pager* pager  = (Pager*)malloc(sizeof(Pager));
  pager->file_length = lseek(fd, 0, SEEK_END);
  pager->file_des = fd;
  pager->num_pages = pager->file_length/PAGES_SIZE;
  for (uint32_t i=0; i<TABLE_MAX_PAGE; i++){
    pager->pages[i]=NULL;
  }
  return pager;
}
void print_row(Row row){
  printf("(%d, %s, %s)\n", row.id, row.user_name, row.email);
}
bool prepare_statement(InputBuffer* inp_buf, Statement* stm){
  if (strncmp(inp_buf->buffer, "insert", 6)==0){
    stm->type = STATEMENT_INSERT;
    if (sscanf(inp_buf->buffer, "insert %d %s %s", &(stm->row_to_insert.id),
      stm->row_to_insert.user_name, stm->row_to_insert.email)<3){
      return false;
    }
    return true;
  }
  if (strcmp(inp_buf->buffer, "select")==0){
    stm->type = STATEMENT_SELECT;
    return true;
  }
  return false;
}
void pager_flush(Pager* pager, uint32_t page_num){
  if (lseek(pager->file_des, page_num*PAGES_SIZE, SEEK_SET)==-1){
    printf("Error\n");
    exit(EXIT_FAILURE);
  }
  if (write(pager->file_des, pager->pages[page_num], PAGES_SIZE)==-1){
    printf("Error write\n");
    exit(EXIT_FAILURE);
  }
}

void close_db(Table* table){
  Pager* pager = table->pager;
  for (uint32_t i=0; i<table->pager->num_pages; i++){
    if (pager->pages[i]!=NULL){
      pager_flush(pager, i);
      free(pager->pages[i]);
    }
  }
  if (close(pager->file_des)==-1){
    printf("Error close\n");
    exit(EXIT_FAILURE);
  }
  free(pager);
  free(table);
}
void serialize_row(Row* source, void* des){
  memcpy(des+ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(des+USERNAME_OFFSET, &(source->user_name), USERNAME_SIZE);
  memcpy(des+EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}
void deserialize_row(Row* des, void* source){
  memcpy(&(des->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(des->user_name), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(des->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}
void* get_page(Pager* pager, uint32_t page_num){
  if (pager->pages[page_num]==NULL){
    void* page = malloc(PAGES_SIZE);
    uint32_t num_page = pager->num_pages;
    if (page_num<num_page){
      lseek(pager->file_des, PAGES_SIZE*page_num, SEEK_SET);
      if (read(pager->file_des, page, PAGES_SIZE)==-1){
        printf("Error read file\n");
        exit(EXIT_FAILURE);
      }
    }
    else{
      pager->num_pages = page_num + 1;
    }
    pager->pages[page_num] = page;
  }
  return pager->pages[page_num];
}
void advance_cur(Cursor* cur) {
  cur->cell_num +=1;
  if (cur->cell_num >= *leaf_node_num_cells(get_page(cur->table->pager, cur->page_num))){
    cur->end_of_table = true;
  }
}
Table* open_db(const char* filename){
  Pager* pager = pager_open(filename);
  Table* tab = (Table*)malloc(sizeof(Table));
  tab->root_page_num = 0;
  tab->pager = pager;
  if (pager->num_pages==0){
    void* root = get_page(pager, 0);
    initialize_leaf_node(root);
    set_node_root(root, true);
  }
  return tab;
}
void* cur_value(Cursor* cur) {
  void* page = get_page(cur->table->pager, cur->page_num);
  return leaf_node_value(page, cur->cell_num);
}
uint32_t get_node_max_key(Pager* pager, void* node){
  switch (node_type(node)) {
    case NODE_INTERNAL:
      return get_node_max_key(pager, get_page(pager, *internal_node_right_child(node)));
    case NODE_LEAF:
      return *leaf_node_key(node, *leaf_node_num_cells(node)-1);
  }
}
void print_pr() {
  printf("miniDB > ");
}
uint32_t internal_find(Table* table, uint32_t key, uint32_t page_num){
  void* node = get_page(table->pager, page_num);
  uint32_t num_key = *internal_node_num_key(node);
  uint32_t start = 0;
  uint32_t end = num_key;
  uint32_t mid;
  uint32_t node_key;
  while (start!=end) {
    mid = (start+end)/2;
    node_key = *internal_node_key(node, mid);
    printf("%d %d\n", node_key, key);
    if (node_key==key){
      return mid;
    } else if (node_key<key){
      start = mid + 1;
    } else {
      end = mid;
    }
  }
  return start;
}
void creat_new_root(Table* table, uint32_t page_num_right){
  void* root = get_page(table->pager, table->root_page_num);
  void* right = get_page(table->pager, page_num_right);
  uint32_t page_num_left = table->pager->num_pages;
  void* left = get_page(table->pager, page_num_left);
  memcpy(left, root, PAGES_SIZE);
  set_node_root(left, false);
  if (node_type(left)==NODE_INTERNAL){
    initialize_internal_node(right);
    void* child;
    for (int i=0; i <= *internal_node_num_key(left); i++){
      child = get_page(table->pager, *internal_node_child(left, i));
      *get_parent(child) = page_num_left;
    }
  }
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_key(root)=1;
  *internal_node_child(root, 0)=page_num_left;
  *internal_node_right_child(root)=page_num_right;
  *internal_node_key(root, 0)=get_node_max_key(table->pager, left);
  *get_parent(right)=table->root_page_num;
  *get_parent(left)=table->root_page_num;
}
void internal_node_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_split_and_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num) {
  uint32_t old_page_num = parent_page_num;
  void* old_node = get_page(table->pager, old_page_num);
  void* child_node = get_page(table->pager, child_page_num);
  uint32_t new_page_num = table->pager->num_pages;
  void* parent;
  void* new_node;
  uint32_t is_root_parent = is_node_root(old_node);
  if (is_root_parent){
    creat_new_root(table, new_page_num);
    parent = get_page(table->pager, table->root_page_num);
    old_page_num = *internal_node_child(parent, 0);
    old_node = get_page(table->pager, old_page_num);
  } else{
    parent = get_page(table->pager, *get_parent(old_node));
    new_node = get_page(table->pager, new_page_num);
    initialize_internal_node(new_node);
  }
  uint32_t cur_page_num = *internal_node_right_child(old_node);
  internal_node_insert(table, new_page_num, cur_page_num);
  for (uint32_t i = INTERNAL_NODE_MAX_CELLS-1; i>INTERNAL_NODE_MAX_CELLS/2; i--){
    cur_page_num = *internal_node_child(old_node, i);
    internal_node_insert(table, new_page_num, cur_page_num);
    *internal_node_num_key(old_node)-=1;
  }
  *internal_node_right_child(old_node)=*internal_node_child(old_node, INTERNAL_NODE_MAX_CELLS/2);
  *internal_node_num_key(old_node)-=1;
  uint32_t max_after_split = get_node_max_key(table->pager, old_node);
  uint32_t child_max_key = get_node_max_key(table->pager, child_node);
  uint32_t des_page_num = child_max_key<max_after_split?old_page_num:new_page_num;
  internal_node_insert(table, des_page_num, child_page_num);
  max_after_split = get_node_max_key(table->pager, old_node);
  uint32_t index = internal_find(table, max_after_split, *get_parent(old_node));
  *internal_node_key(parent, index) = max_after_split;
  if (!is_root_parent){
    internal_node_insert(table, *get_parent(old_node), new_page_num);
  }
}
void internal_node_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num){
  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);

  uint32_t child_max = get_node_max_key(table->pager, child);
  uint32_t parent_num_keys = *internal_node_num_key(parent);

  if (parent_num_keys>=INTERNAL_NODE_MAX_CELLS){
    internal_node_split_and_insert(table, parent_page_num, child_page_num);
    return;
  }
  uint32_t right_parent = *internal_node_right_child(parent);
  *get_parent(child)=parent_page_num;
  if (right_parent==INVALID_PAGE_NUM){
    *internal_node_right_child(parent)=child_page_num;
    return;
  }
  void* right_child = get_page(table->pager, right_parent);
  uint32_t right_child_max = get_node_max_key(table->pager, right_child);
  if (right_child_max<child_max){
    *internal_node_num_key(parent) += 1;
    *internal_node_key(parent, parent_num_keys)=right_child_max;
    *internal_node_child(parent, parent_num_keys)=right_parent;
    *internal_node_right_child(parent)=child_page_num;
  } else {
    uint32_t index = internal_find(table, child_max, parent_page_num);
    for (uint32_t i=parent_num_keys; i>index; i--){
      memcpy(internal_node_cell(parent, i), internal_node_cell(parent, i-1), INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_num_key(parent) += 1;
    *internal_node_child(parent, index)=child_page_num;
    *internal_node_key(parent, index)=child_max;
  }
}

void leaf_node_split_and_insert(Cursor* cur, Row* value, uint32_t key) {
  void* old_node = get_page(cur->table->pager, cur->page_num);
  uint32_t page_num = cur->table->pager->num_pages;
  void* new_node = get_page(cur->table->pager, page_num);
  initialize_leaf_node(new_node);
  void* temp;
  uint32_t index_in_node;
  for(int32_t i=LEAF_NODE_MAX_CELLS; i>=0; i--){
    index_in_node = i%LEAF_NODE_CELLS_LEFT;
    if (i<LEAF_NODE_CELLS_LEFT){
      temp = old_node;
    } else{
      temp = new_node;
    }
    if (i == cur->cell_num){
      serialize_row(value, leaf_node_value(temp, index_in_node));
      *leaf_node_key(temp, index_in_node) = key;
    } else if (i > cur->cell_num){
      memcpy(leaf_node_cell(temp, index_in_node), leaf_node_cell(old_node, i-1), LEAF_NODE_CELL_SIZE);
    } else{
      memcpy(leaf_node_cell(temp, index_in_node), leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
    }
  }
  *leaf_node_num_cells(old_node)=LEAF_NODE_CELLS_LEFT;
  *leaf_node_num_cells(new_node)=LEAF_NODE_CELLS_RIGHT;
  if (is_node_root(old_node)){
    return creat_new_root(cur->table, page_num);
  } else {
    uint32_t parent_page_num = *get_parent(old_node);
    void* parent = get_page(cur->table->pager, parent_page_num);
    uint32_t old_node_max = get_node_max_key(cur->table->pager, old_node);
    uint32_t index = internal_find(cur->table, old_node_max, parent_page_num);
    *internal_node_key(parent, index) = old_node_max;
    return internal_node_insert(cur->table, parent_page_num, page_num);
  }
}
void insert_to_leaf(Cursor* cur, Row* value, uint32_t key){
  void* node = get_page(cur->table->pager, cur->page_num);
  uint32_t num_cell = *leaf_node_num_cells(node);
  printf("%d\n", LEAF_NODE_MAX_CELLS);
  if (num_cell >= LEAF_NODE_MAX_CELLS){
    leaf_node_split_and_insert(cur, value, key);
    return;
  }
  for(uint32_t i = num_cell; i > cur->cell_num; i--){
    memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i-1), LEAF_NODE_CELL_SIZE);
  }
  *leaf_node_key(node, cur->cell_num) = key;
  serialize_row(value, leaf_node_value(node, cur->cell_num));
  *leaf_node_num_cells(node) += 1;
}
Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key){
  void* node = get_page(table->pager, page_num);
  Cursor* cur = (Cursor*)malloc(sizeof(Cursor));
  cur->table = table;
  cur->page_num = page_num;
  uint32_t num_cell = *leaf_node_num_cells(node);
  uint32_t start = 0;
  uint32_t end = num_cell;
  uint32_t mid;
  uint32_t key_node;
  while (start!=end) {
    mid = (start+end)/2;
    key_node = *leaf_node_key(node, mid);
    if (key_node == key){
      cur->cell_num = mid;
      return cur;
    }
    if (key_node > key){
      end = mid;
    }
    else{
      start = mid+1;
    }
  }
  cur->cell_num = start;
  return cur;
}
Cursor* table_find(Table* table, uint32_t key, uint32_t page_num){
  void* node = get_page(table->pager, page_num);
  if (node_type(node) == NODE_LEAF){
    return leaf_node_find(table, page_num, key);
  } else{
    uint32_t index = internal_find(table, key, page_num);
    return table_find(table, key, *internal_node_child(node, index));
  }
}
bool execute_insert(Table* table, Statement* statement){
  Row* row_to_insert = &(statement->row_to_insert);
  Cursor* cur = table_find(table, row_to_insert->id, table->root_page_num);
  if (*leaf_node_key(get_page(table->pager, cur->page_num), cur->cell_num)==row_to_insert->id){
    printf("id exists\n");
    return false;
  }
  insert_to_leaf(cur, row_to_insert, row_to_insert->id);
  free(cur);
  return true;
}

void recursive_print(Table* table, uint32_t page_num){
  void* node = get_page(table->pager, page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  if (node_type(node)==NODE_LEAF){
    printf("Node leaf: %d\n", page_num);
    Row row;
    for (uint32_t i=0; i<num_cells; i++){
      deserialize_row(&row, leaf_node_value(node, i));
      print_row(row);
    }
  } else {
    printf("%d\n", table->root_page_num);

    printf("%d %d\n",*internal_node_num_key(node), page_num);
    printf("[");
    for (uint32_t i=0; i<num_cells; i++){
      printf("%d, ", *internal_node_key(node, i));
    }
    printf("]\n");
    for (uint32_t i=0; i<=num_cells; i++){
      recursive_print(table, *internal_node_child(node, i));
    }
  }
}
void execute_select(Table* table){
  recursive_print(table, table->root_page_num);
}
bool execute_statement(Statement* stm, Table* table){
  switch (stm->type) {
    case STATEMENT_INSERT:
      return execute_insert(table, stm);
    case STATEMENT_SELECT:
      execute_select(table);
      return true;
  }
}

int main(int argc, char const *argv[]) {
  InputBuffer* inp_buf = new_inp_buf();
  Table* table = open_db("data.db");
  printf("%d\n", INVALID_PAGE_NUM);
  while (1){
    print_pr();
    read_input(inp_buf);
    if (strcmp(inp_buf->buffer, ".exit")==0){
      close_input_buffer(inp_buf);
      close_db(table);
      exit(EXIT_SUCCESS);
    }
    Statement statement;
    if (prepare_statement(inp_buf, &statement)==false){
      printf("query exis\n");
      continue;
    }
    execute_statement(&statement, table);
    printf("Executed.\n");
  }
  return 0;
}
