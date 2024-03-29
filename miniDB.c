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

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT, STATEMENT_SELECT_BY_ID, STATEMENT_DELETE_BY_ID, STATEMENT_UPDATE_BY_ID} StatementType;
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
  uint32_t key;
  Row row_to_insert;
  bool update_user_name;
  bool update_email;
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
  printf("| %-*d | %*s | %*s |\n", 5, row.id, 10, row.user_name, 25, row.email);
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
  if (strncmp(inp_buf->buffer, "delete", 6)==0){
    stm->type = STATEMENT_DELETE_BY_ID;
    if (sscanf(inp_buf->buffer, "delete id=%d", &(stm->key))==1){
      return true;
    }
    return false;
  }
  if (strncmp(inp_buf->buffer, "select", 6)==0) {
    if (strcmp(inp_buf->buffer, "select")==0){
      stm->type = STATEMENT_SELECT;
      return true;
    } else if (sscanf(inp_buf->buffer, "select id=%d", &(stm->key))==1){
      stm->type = STATEMENT_SELECT_BY_ID;
      return true;
    }
  }
  if (strncmp(inp_buf->buffer, "update set", 10)==0){
    stm->type = STATEMENT_UPDATE_BY_ID;
    stm->update_user_name = false;
    stm->update_email = false;
    char* user_name = strstr(inp_buf->buffer, "user_name=");
    char* email = strstr(inp_buf->buffer, "email=");
    if ((user_name)&&(email)){
      if (user_name>email){
        if (sscanf(inp_buf->buffer, "update set email=%s user_name=%s where id=%d",
        stm->row_to_insert.email, stm->row_to_insert.user_name, &(stm->row_to_insert.id))<3){
          return false;
        }
        stm->update_user_name = true;
        stm->update_email = true;
        return true;
      } else {
        if (sscanf(inp_buf->buffer, "update set user_name=%s email=%s where id=%d",
        stm->row_to_insert.user_name, stm->row_to_insert.email, &(stm->row_to_insert.id))<3){
          return false;
        }
        stm->update_user_name = true;
        stm->update_email = true;
        return true;
      }
    } else if (user_name){
      if (sscanf(inp_buf->buffer, "update set user_name=%s where id=%d", stm->row_to_insert.user_name, &(stm->row_to_insert.id))<2){
        return false;
      }
      stm->update_user_name = true;
      return true;
    } else if (email){
      if (sscanf(inp_buf->buffer, "update set email=%s where id=%d", stm->row_to_insert.email, &(stm->row_to_insert.id))<2){
        return false;
      }
      stm->update_email = true;
      return true;
    }
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
uint32_t internal_find(Pager* pager, uint32_t key, uint32_t page_num){
  void* node = get_page(pager, page_num);
  uint32_t num_key = *internal_node_num_key(node);
  uint32_t start = 0;
  uint32_t end = num_key;
  uint32_t mid;
  uint32_t node_key;
  while (start!=end) {
    mid = (start+end)/2;
    node_key = *internal_node_key(node, mid);
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
  uint32_t index = internal_find(table->pager, max_after_split, *get_parent(old_node));
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
    uint32_t index = internal_find(table->pager, child_max, parent_page_num);
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
    uint32_t index = internal_find(cur->table->pager, old_node_max, parent_page_num);
    *internal_node_key(parent, index) = old_node_max;
    return internal_node_insert(cur->table, parent_page_num, page_num);
  }
}
void insert_to_leaf(Cursor* cur, Row* value, uint32_t key){
  void* node = get_page(cur->table->pager, cur->page_num);
  uint32_t num_cell = *leaf_node_num_cells(node);
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
    uint32_t index = internal_find(table->pager, key, page_num);
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
    Row row;
    for (uint32_t i=0; i<num_cells; i++){
      deserialize_row(&row, leaf_node_value(node, i));
      print_row(row);
    }
  } else {
    for (uint32_t i=0; i<=num_cells; i++){
      recursive_print(table, *internal_node_child(node, i));
    }
  }
}
void execute_select(Table* table){
  printf("__________________________________________________\n");
  printf("| %-*s | %*s | %*s |\n", 5, "ID", 10, "USER_NAME", 25, "EMAIL");
  printf("| %-*s | %-*s | %*s |\n", 5, "-----", 10, "----------", 25, "-------------------------");
  recursive_print(table, table->root_page_num);
  printf("--------------------------------------------------\n");
}
void update_internal_node(Pager* pager, uint32_t page_num, uint32_t new_key) {
  void* node = get_page(pager, page_num);
  uint32_t index = internal_find(pager, new_key, page_num);
  if (index==*internal_node_num_key(node)){
    if (!is_node_root(node)){
      return update_internal_node(pager, *get_parent(node), new_key);
    }
  } else {
    *internal_node_key(node, index)=new_key;
  }
}
void delete_leaf_cell(Pager* pager, uint32_t page_num, uint32_t cell_num) {
  void* node = get_page(pager, page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  for (uint32_t i = cell_num; i < num_cells-1; i++){
    memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i+1), LEAF_NODE_CELL_SIZE);
  }
  *leaf_node_num_cells(node)-=1;
  if ((num_cells-1==cell_num)&&(!is_node_root(node))){
    update_internal_node(pager, *get_parent(node), *leaf_node_key(node, cell_num-1));
  }
}
void merge_internal_node(Pager* pager, uint32_t page_num_left, uint32_t page_num_right) {
  void* node_left = get_page(pager, page_num_left);
  void* node_right = get_page(pager, page_num_right);
  uint32_t parent_page_num = *get_parent(node_left);
  void* node_parent = get_page(pager, parent_page_num);
  uint32_t index_in_parent = internal_find(pager, *internal_node_key(node_left, 0), parent_page_num);
  uint32_t left_num_key = *internal_node_num_key(node_left);
  memcpy(internal_node_cell(node_right, left_num_key+1), internal_node_cell(node_right, 0), (*internal_node_num_key(node_right))*INTERNAL_NODE_CELL_SIZE);
  memcpy(internal_node_cell(node_right, 0), internal_node_cell(node_left, 0),left_num_key*INTERNAL_NODE_CELL_SIZE);
  *internal_node_num_key(node_right) += left_num_key+1;
  *internal_node_child(node_right, left_num_key) = *internal_node_right_child(node_left);
  *internal_node_key(node_right, left_num_key)= get_node_max_key(pager, get_page(pager, *internal_node_right_child(node_left)));
  memcpy(internal_node_cell(node_parent, index_in_parent), internal_node_cell(node_parent, index_in_parent+1), (*internal_node_num_key(node_parent)-index_in_parent-1)*INTERNAL_NODE_CELL_SIZE);
  *internal_node_num_key(node_parent)-=1;
  void* child;
  for (uint32_t i=0; i<=left_num_key; i++){
    child = get_page(pager, *internal_node_child(node_right, i));
    *get_parent(child) = page_num_right;
  }
}
uint32_t recursive_delete_internal_node(Pager* pager, uint32_t page_num){
  void* node = get_page(pager, page_num);
  uint32_t parent_page_num = *get_parent(node);
  void* node_parent = get_page(pager, parent_page_num);
  uint32_t index_in_parent = internal_find(pager, *internal_node_key(node, 0), parent_page_num);
  uint32_t node_num_key = *internal_node_num_key(node);
  if (index_in_parent<*leaf_node_num_cells(node_parent)){
    void* right_node = get_page(pager, *internal_node_child(node_parent, index_in_parent+1));
    uint32_t right_num_key = *internal_node_num_key(right_node);
    if (right_num_key>INTERNAL_NODE_MAX_CELLS/2){
      *internal_node_num_key(node)+=1;
      *internal_node_key(node, node_num_key)=get_node_max_key(pager, get_page(pager, *internal_node_right_child(node)));
      *internal_node_child(node, node_num_key)=*internal_node_right_child(node);
      *internal_node_right_child(node)=*internal_node_child(right_node, 0);
      *internal_node_key(node_parent, index_in_parent)=*internal_node_key(right_node, 0);
      *get_parent(get_page(pager, *internal_node_right_child(node)))=page_num;
      memcpy(internal_node_cell(right_node, 0), internal_node_cell(right_node, 1), (right_num_key-1)*INTERNAL_NODE_CELL_SIZE);
      *internal_node_num_key(right_node)-=1;
      return page_num;
    }
  }
  if (index_in_parent>0){
    void* left_node = get_page(pager, *internal_node_child(node_parent, index_in_parent-1));
    uint32_t left_num_key = *internal_node_num_key(left_node);
    if (left_num_key>INTERNAL_NODE_MAX_CELLS/2){
      *internal_node_num_key(node)+=1;
      memcpy(internal_node_cell(node, 1), internal_node_cell(node, 0), node_num_key*INTERNAL_NODE_CELL_SIZE);
      void* right_child_of_lefrt = get_page(pager, *internal_node_right_child(left_node));
      *internal_node_key(node, 0) = get_node_max_key(pager, right_child_of_lefrt);
      *internal_node_child(node, 0) = *internal_node_right_child(left_node);
      *get_parent(right_child_of_lefrt) = page_num;
      *internal_node_right_child(left_node) = *internal_node_child(left_node, left_num_key-1);
      *internal_node_key(node_parent, index_in_parent-1)=*internal_node_key(left_node, left_num_key-1);
      *internal_node_num_key(left_node)-=1;
      return page_num;
    }
  }
  uint32_t parent_num_keys = *internal_node_num_key(node_parent);
  if ((parent_num_keys<=INTERNAL_NODE_MAX_CELLS/2)&&(!is_node_root(node_parent))){
    parent_page_num = recursive_delete_internal_node(pager, parent_page_num);
    node_parent = get_page(pager, parent_page_num);
    index_in_parent = internal_find(pager, *internal_node_key(node, 0), parent_page_num);
  }
  uint32_t new_page_num;
  if (index_in_parent==0){
    merge_internal_node(pager, page_num, *internal_node_child(node_parent, index_in_parent+1));
    new_page_num = *internal_node_child(node_parent, index_in_parent);
  } else {
    merge_internal_node(pager, *internal_node_child(node_parent, index_in_parent-1), page_num);
    new_page_num = page_num;
  }
  if ((is_node_root(node_parent))&&(*internal_node_num_key(node_parent)==0)){
    void* new_page = get_page(pager, new_page_num);
    uint32_t num_cells = *internal_node_num_key(new_page);
    memcpy(internal_node_cell(node_parent, 0), internal_node_cell(new_page, 0), num_cells*INTERNAL_NODE_CELL_SIZE);
    *internal_node_right_child(node_parent) = *internal_node_right_child(new_page);
    *internal_node_num_key(node_parent) = num_cells;
    void* child;
    for (uint32_t i=0; i<=num_cells; i++){
      child = get_page(pager, *internal_node_child(node_parent, i));
      *get_parent(child) = parent_page_num;
    }
    new_page_num = parent_page_num;
  }
  return new_page_num;
}
void merge_leaf_node(Pager* pager, uint32_t page_num_left, uint32_t page_num_right){
  void* node_left = get_page(pager, page_num_left);
  void* node_right = get_page(pager, page_num_right);
  void* node_parent = get_page(pager, *get_parent(node_left));
  if ((*internal_node_num_key(node_parent)<=INTERNAL_NODE_MAX_CELLS/2)&&(!is_node_root(node_parent))){
    uint32_t parent_page_num = recursive_delete_internal_node(pager, *get_parent(node_left));
    node_parent = get_page(pager, parent_page_num);
  }
  uint32_t num_cells_left = *leaf_node_num_cells(node_left);
  memcpy(leaf_node_cell(node_right, num_cells_left), leaf_node_cell(node_right, 0), (*leaf_node_num_cells(node_right))*LEAF_NODE_CELL_SIZE);
  memcpy(leaf_node_cell(node_right, 0), leaf_node_cell(node_left, 0), num_cells_left*LEAF_NODE_CELL_SIZE);
  *leaf_node_num_cells(node_right) += num_cells_left;
  uint32_t index_in_parent = internal_find(pager, *leaf_node_key(node_left, num_cells_left-1), *get_parent(node_left));
  memcpy(internal_node_cell(node_parent, index_in_parent), internal_node_cell(node_parent, index_in_parent+1), (*internal_node_num_key(node_parent)-index_in_parent-1)*INTERNAL_NODE_CELL_SIZE);
  *internal_node_num_key(node_parent)-=1;
  if ((is_node_root(node_parent))&&(*internal_node_num_key(node_parent)==0)){
    initialize_leaf_node(node_parent);
    set_node_root(node_parent, true);
    uint32_t num_cells = *leaf_node_num_cells(node_right);
    memcpy(leaf_node_cell(node_parent, 0), leaf_node_cell(node_right, 0), num_cells*LEAF_NODE_CELL_SIZE);
    *leaf_node_num_cells(node_parent) = num_cells;
  }
}
bool execute_delete(Table* table, uint32_t key){
  Cursor* cur = table_find(table, key, table->root_page_num);
  void* node = get_page(table->pager, cur->page_num);
  if ((*leaf_node_key(node, cur->cell_num)!=key)||(*leaf_node_num_cells(node) <= cur->cell_num)){
    printf("id not found\n");
    free(cur);
    return false;
  }
  if ((*leaf_node_num_cells(node)<=LEAF_NODE_CELLS_LEFT)&&(!is_node_root(node))){
    void* parent = get_page(table->pager, *get_parent(node));
    uint32_t index_in_parent = internal_find(table->pager, key, *get_parent(node));
    if (index_in_parent < *internal_node_num_key(parent)){
      void* right_node = get_page(table->pager, *internal_node_child(parent, index_in_parent+1));
      uint32_t right_num_cells = *leaf_node_num_cells(right_node);
      if (right_num_cells>LEAF_NODE_CELLS_LEFT){
        memcpy(leaf_node_cell(node, *leaf_node_num_cells(node)), leaf_node_cell(right_node, 0), LEAF_NODE_CELL_SIZE);
        *leaf_node_num_cells(node)+=1;
        *internal_node_key(parent, index_in_parent)=*leaf_node_key(node, *leaf_node_num_cells(node)-1);
        memcpy(leaf_node_cell(right_node, 0), leaf_node_cell(right_node, 1), (right_num_cells-1)*LEAF_NODE_CELL_SIZE);
        *leaf_node_num_cells(right_node)-=1;
        delete_leaf_cell(table->pager, cur->page_num, cur->cell_num);
        free(cur);
        return true;
      }
    }
    if (index_in_parent>0){
      void* left_node = get_page(table->pager, *internal_node_child(parent, index_in_parent-1));
      uint32_t left_num_cells = *leaf_node_num_cells(left_node);
      if (left_num_cells>LEAF_NODE_CELLS_LEFT){
        memcpy(leaf_node_cell(node, 1), leaf_node_cell(node, 0), (*leaf_node_num_cells(node))*LEAF_NODE_CELL_SIZE);
        memcpy(leaf_node_cell(node, 0), leaf_node_cell(left_node, left_num_cells-1), LEAF_NODE_CELL_SIZE);
        *leaf_node_num_cells(node)+=1;
        *leaf_node_num_cells(left_node)-=1;
        *internal_node_key(parent, index_in_parent-1)=*leaf_node_key(left_node, left_num_cells-2);
        delete_leaf_cell(table->pager, cur->page_num, cur->cell_num + 1);
        free(cur);
        return true;
      }
    }
    if (index_in_parent < *internal_node_num_key(parent)){
      merge_leaf_node(table->pager, *internal_node_child(parent, index_in_parent), *internal_node_child(parent, index_in_parent+1));
    } else {
      merge_leaf_node(table->pager, *internal_node_child(parent, index_in_parent-1), *internal_node_child(parent, index_in_parent));
    }
    free(cur);
    Cursor* cur = table_find(table, key, table->root_page_num);
  }
  delete_leaf_cell(table->pager, cur->page_num, cur->cell_num);
  free(cur);
  return true;
}
bool execute_statement(Statement* stm, Table* table){
  switch (stm->type) {
    case STATEMENT_INSERT:
      return execute_insert(table, stm);
    case STATEMENT_SELECT:
      execute_select(table);
      return true;
    case STATEMENT_SELECT_BY_ID:
      Cursor* cur = table_find(table, stm->key, table->root_page_num);
      void* node = get_page(table->pager, cur->page_num);
      if ((*leaf_node_key(node, cur->cell_num)==stm->key)&&(*leaf_node_num_cells(node)>cur->cell_num)){
        Row row;
        deserialize_row(&row, leaf_node_value(node, cur->cell_num));
        printf("__________________________________________________\n");
        printf("| %-*s | %*s | %*s |\n", 5, "ID", 10, "USER_NAME", 25, "EMAIL");
        printf("| %-*s | %-*s | %*s |\n", 5, "-----", 10, "----------", 25, "-------------------------");
        print_row(row);
      } else {
        printf("not found row when id = %d\n", stm->key);
      }
      free(cur);
      return true;
    case STATEMENT_DELETE_BY_ID:
      return execute_delete(table, stm->key);
    case STATEMENT_UPDATE_BY_ID:
      Cursor* cur_to_update = table_find(table, stm->row_to_insert.id, table->root_page_num);
      void* node_to_update = get_page(table->pager, cur_to_update->page_num);
      if ((*leaf_node_key(node_to_update, cur_to_update->cell_num)!=stm->row_to_insert.id)||(*leaf_node_num_cells(node_to_update)<=cur_to_update->cell_num)){
        printf("not found row when id = %d\n", stm->row_to_insert.id);
        free(cur_to_update);
        return false;
      }
      if (stm->update_email){
        memcpy(leaf_node_value(node_to_update, cur_to_update->cell_num)+EMAIL_OFFSET, &(stm->row_to_insert.email), EMAIL_SIZE);
      }
      if (stm->update_user_name){
        memcpy(leaf_node_value(node_to_update, cur_to_update->cell_num)+USERNAME_OFFSET, &(stm->row_to_insert.user_name), USERNAME_SIZE);
      }
      free(cur_to_update);
      return true;
  }
}

int main(int argc, char const *argv[]) {
  InputBuffer* inp_buf = new_inp_buf();
  Table* table = open_db("data.db");
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
